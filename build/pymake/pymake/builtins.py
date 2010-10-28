# Basic commands implemented in Python
import sys, os, shutil, time
from getopt import getopt, GetoptError

from process import PythonException

__all__ = ["rm", "sleep", "touch"]

def rm(args):
  """
  Emulate most of the behavior of rm(1).
  Only supports the -r (--recursive) and -f (--force) arguments.
  """
  try:
    opts, args = getopt(args, "rRf", ["force", "recursive"])
  except GetoptError, e:
    raise PythonException, ("rm: %s" % e, 1)
  force = False
  recursive = False
  for o, a in opts:
    if o in ('-f', '--force'):
      force = True
    elif o in ('-r', '-R', '--recursive'):
      recursive = True
  for f in args:
    if os.path.isdir(f):
      if not recursive:
        raise PythonException, ("rm: cannot remove '%s': Is a directory" % f, 1)
      else:
        shutil.rmtree(f, force)
    elif os.path.exists(f):
      try:
        os.unlink(f)
      except:
        if not force:
          raise PythonException, ("rm: failed to remove '%s': %s" % (f, sys.exc_info()[0]), 1)
    elif not force:
      raise PythonException, ("rm: cannot remove '%s': No such file or directory" % f, 1)

def sleep(args):
    """
    Emulate the behavior of sleep(1).
    """
    total = 0
    values = {'s': 1, 'm': 60, 'h': 3600, 'd': 86400}
    for a in args:
        multiplier = 1
        for k, v in values.iteritems():
            if a.endswith(k):
                a = a[:-1]
                multiplier = v
                break
        try:
            f = float(a)
            total += f * multiplier
        except ValueError:
            raise PythonException, ("sleep: invalid time interval '%s'" % a, 1)
    time.sleep(total)

def touch(args):
    """
    Emulate the behavior of touch(1).
    """
    for f in args:
        if os.path.exists(f):
            os.utime(f, None)
        else:
            open(f, 'w').close()
