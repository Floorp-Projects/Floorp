# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla build system.
#
# The Initial Developer of the Original Code is
# Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2008
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Axel Hecht <axel@pike.org>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

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
    os.remove(self.lockfile)


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
      if e.errno != errno.EEXIST:
        # should not occur
        raise
  
    try:
      # the lock file exists, try to stat it to get its age
      # and read its contents to report the owner PID
      f = open(lockfile, "r")
      s = os.stat(lockfile)
    except OSError, e:
      if e.errno != errno.ENOENT:
        sys.exit("%s exists but stat() failed: %s" %
                 (lockfile, e.strerror))
      # we didn't create the lockfile, so it did exist, but it's
      # gone now. Just try again
      continue
  
    # we didn't create the lockfile and it's still there, check
    # its age
    now = int(time.time())
    if now - s[stat.ST_MTIME] > max_wait:
      pid = f.readline()
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
