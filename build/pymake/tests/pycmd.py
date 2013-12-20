import os, sys, subprocess

def writetofile(args):
  with open(args[0], 'w') as f:
    f.write(' '.join(args[1:]))

def writeenvtofile(args):
  with open(args[0], 'w') as f:
    f.write(os.environ[args[1]])

def writesubprocessenvtofile(args):
  with open(args[0], 'w') as f:
    p = subprocess.Popen([sys.executable, "-c",
                          "import os; print os.environ['%s']" % args[1]],
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = p.communicate()
    assert p.returncode == 0
    f.write(stdout)

def convertasplode(arg):
  try:
    return int(arg)
  except:
    return (None if arg == "None" else arg)

def asplode(args):
  arg0 = convertasplode(args[0])
  sys.exit(arg0)

def asplode_return(args):
  arg0 = convertasplode(args[0])
  return arg0

def asplode_raise(args):
  raise Exception(args[0])

def delayloadfn(args):
    import delayload
