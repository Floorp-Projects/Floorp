# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''Utility methods to be used by python build infrastructure.
'''

import os
import errno
import sys
import time
import stat

class LockFile(object):
  '''LockFile is used by the lockFile method to hold the lock.

  This object should not be used directly, but only through
  the lockFile method below.
  '''
  def __init__(self, lockfile):
    self.lockfile = lockfile
  def __del__(self):
    while True:
      try:
        os.remove(self.lockfile)
        break
      except OSError, e:
        if e.errno == errno.EACCES:
          # another process probably has the file open, we'll retry.
          # just a short sleep since we want to drop the lock ASAP
          # (but we need to let some other process close the file first)
          time.sleep(0.1)
        else:
          # re-raise unknown errors
          raise

def lockFile(lockfile, max_wait = 600):
  '''Create and hold a lockfile of the given name, with the given timeout.

  To release the lock, delete the returned object.
  '''
  while True:
    try:
      fd = os.open(lockfile, os.O_EXCL | os.O_RDWR | os.O_CREAT)
      # we created the lockfile, so we're the owner
      break
    except OSError, e:
      if e.errno == errno.EEXIST or \
         (sys.platform == "win32" and e.errno == errno.EACCES):
        pass
      else:
        # should not occur
        raise
  
    try:
      # the lock file exists, try to stat it to get its age
      # and read its contents to report the owner PID
      f = open(lockfile, "r")
      s = os.stat(lockfile)
    except EnvironmentError, e:
      if e.errno == errno.ENOENT or e.errno == errno.EACCES:
        # we didn't create the lockfile, so it did exist, but it's
        # gone now. Just try again
        continue
      sys.exit("%s exists but stat() failed: %s" %
               (lockfile, e.strerror))
  
    # we didn't create the lockfile and it's still there, check
    # its age
    now = int(time.time())
    if now - s[stat.ST_MTIME] > max_wait:
      pid = f.readline().rstrip()
      sys.exit("%s has been locked for more than " \
               "%d seconds (PID %s)" % (lockfile, max_wait,
                                        pid))
  
    # it's not been locked too long, wait a while and retry
    f.close()
    time.sleep(1)
  
  # if we get here. we have the lockfile. Convert the os.open file
  # descriptor into a Python file object and record our PID in it
  
  f = os.fdopen(fd, "w")
  f.write("%d\n" % os.getpid())
  f.close()
  return LockFile(lockfile)

class pushback_iter(object):
  '''Utility iterator that can deal with pushed back elements.

  This behaves like a regular iterable, just that you can call
    iter.pushback(item)
  to get the givem item as next item in the iteration.
  '''
  def __init__(self, iterable):
    self.it = iter(iterable)
    self.pushed_back = []

  def __iter__(self):
    return self

  def __nonzero__(self):
    if self.pushed_back:
      return True

    try:
      self.pushed_back.insert(0, self.it.next())
    except StopIteration:
      return False
    else:
      return True

  def next(self):
    if self.pushed_back:
      return self.pushed_back.pop()
    return self.it.next()

  def pushback(self, item):
    self.pushed_back.append(item)
