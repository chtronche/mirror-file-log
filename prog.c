/* See usage
 * Author: Ch. Tronche <ch.tronche@gmail.com>, started on 2013/04/26.
 * This is free software, MIT/Expat license, please use at will.
 * Not tested for file size >= 2Gb.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <unistd.h>

static int _openFile(const char *fileName)
{
  int fd = open(fileName, O_RDONLY);
  if (fd >= 0) return fd;

  perror("hot-logfs-backup/_openFile");
  exit(-2);
}

static off_t _getFileLength(int fd)
{
  struct stat buf;
  int error = fstat(fd, &buf);
  if (!error) return buf.st_size;
  perror("hot-logfs-backup/_getFileLength");
  exit(-3);
}

static long int _getLastSize(const char *sizeStr)
{
  char *endptr;
  long int res = strtol(sizeStr, &endptr, 10);
  if (endptr > sizeStr && !*endptr) return res;
  
  fprintf(stderr, "hot-logfs-backup/_getLastSize: Malformed number: %s\n", sizeStr);
  exit(-1);
}

static void _seek(int fd, off_t lastSize)
{
  off_t res = lseek(fd, lastSize, SEEK_SET);
  if (res != -1) return;
  perror("hot-logfs-backup/_seek");
  exit(-2);
}

static ssize_t _read(int fd, char *buffer, size_t count)
{
  ssize_t r = read(fd, buffer, count);
  if (r >= 0) return r;
  perror("hot-logfs-backup/read");
  exit(-3);
}

static ssize_t _write(int fd, const void *buf, size_t count)
{
  ssize_t res = write(fd, buf, count);
  if (res >= 0) return res;
  perror("hot-logfs-backup/_write");
  exit(-4);
}

static void _copyDelta(int fd, long *lastSize)
{
    off_t currentFileLength = _getFileLength(fd);
    long int delta = currentFileLength - *lastSize;
    if (delta < 0) exit(0);
    while(delta > 0) {
      char buffer[4096];
      ssize_t r = _read(fd, buffer, 4096);
      if (!r) break;
      ssize_t w = _write(1, buffer, r);
      if (!w) {
	fprintf(stderr, "hot-logfs-backup zero write ?\n");
	exit(-6);
      }
      delta -= w;
      *lastSize += w;
    }
}

static int _initNotify(const char *pathname)
{
  int fd = inotify_init();
  if (fd < 0) {
    perror("hot-logfs-backup/inotify_init");
    exit(-2);
  }

  int res = inotify_add_watch(fd, pathname, IN_MODIFY|IN_ATTRIB);
  if (res != -1) return fd;
  
  perror("hot-logfs-backup/inotify_add_watch");
  exit(-2);
}

static void _waitForChange(int inotify)
{
  struct inotify_event event;
  int res = read(inotify, &event, sizeof(event));
  if (res >= 0) return;
  perror("hot-logfs-backup/_waitForChange");
  exit(-5);
}

int main(int argc, const char *argv[])
{
  if (argc != 3) {
    fprintf(stderr, "Usage: hot-logfs-backup backup-original-length watch-file\n");
    exit(-1);
  }
  const char *watchedFileName = argv[2];
  int inotify = _initNotify(watchedFileName);
  long lastSize = _getLastSize(argv[1]);
  int fd = _openFile(watchedFileName);
  _seek(fd, lastSize);
  for(;;) {
    _copyDelta(fd, &lastSize);
    _waitForChange(inotify);
  }
  return 0;
}
