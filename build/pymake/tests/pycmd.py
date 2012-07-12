import os, sys

def writetofile(args):
  with open(args[0], 'w') as f:
    f.write(' '.join(args[1:]))

def writeenvtofile(args):
  with open(args[0], 'w') as f:
    f.write(os.environ[args[1]])

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
