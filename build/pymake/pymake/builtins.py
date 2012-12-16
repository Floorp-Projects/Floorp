# Basic commands implemented in Python
import errno, sys, os, shutil, time
from getopt import getopt, GetoptError

from process import PythonException

__all__ = ["mkdir", "rm", "sleep", "touch"]

def mkdir(args):
  """
  Emulate some of the behavior of mkdir(1).
  Only supports the -p (--parents) argument.
  """
  try:
    opts, args = getopt(args, "p", ["parents"])
  except GetoptError, e:
    raise PythonException, ("mkdir: %s" % e, 1)
  parents = False
  for o, a in opts:
    if o in ('-p', '--parents'):
      parents = True
  for f in args:
    try:
      if parents:
        os.makedirs(f)
      else:
        os.mkdir(f)
    except OSError, e:
      if e.errno == errno.EEXIST and parents:
        pass
      else:
        raise PythonException, ("mkdir: %s" % e, 1)

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
    try:
        opts, args = getopt(args, "t:")
    except GetoptError, e:
        raise PythonException, ("touch: %s" % e, 1)
    opts = dict(opts)
    times = None
    if '-t' in opts:
        import re
        from time import mktime, localtime
        m = re.match('^(?P<Y>(?:\d\d)?\d\d)?(?P<M>\d\d)(?P<D>\d\d)(?P<h>\d\d)(?P<m>\d\d)(?:\.(?P<s>\d\d))?$', opts['-t'])
        if not m:
            raise PythonException, ("touch: invalid date format '%s'" % opts['-t'], 1)
        def normalized_field(m, f):
            if f == 'Y':
                if m.group(f) is None:
                    return localtime()[0]
                y = int(m.group(f))
                if y < 69:
                    y += 2000
                elif y < 100:
                    y += 1900
                return y
            if m.group(f) is None:
                return localtime()[0] if f == 'Y' else 0
            return int(m.group(f))
        time = [normalized_field(m, f) for f in ['Y', 'M', 'D', 'h', 'm', 's']] + [0, 0, -1]
        time = mktime(time)
        times = (time, time)
    for f in args:
        if not os.path.exists(f):
            open(f, 'a').close()
        os.utime(f, times)
