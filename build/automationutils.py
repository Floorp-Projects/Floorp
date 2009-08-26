import sys, glob, os, subprocess, logging

__all__ = [
    "addCommonOptions",
    "checkForCrashes",
    ]

log = logging.getLogger()

def addCommonOptions(parser, defaults={}):
    parser.add_option("--xre-path",
                      action = "store", type = "string", dest = "xrePath",
                      # individual scripts will set a sane default
                      default = None,
                      help = "absolute path to directory containing XRE (probably xulrunner)")
    if 'SYMBOLS_PATH' not in defaults:
        defaults['SYMBOLS_PATH'] = None
    parser.add_option("--symbols-path",
                      action = "store", type = "string", dest = "symbolsPath",
                      default = defaults['SYMBOLS_PATH'],
                      help = "absolute path to directory containing breakpad symbols")

def checkForCrashes(dumpDir, symbolsPath, testName=None):
  stackwalkPath = os.environ.get('MINIDUMP_STACKWALK', None)
  # try to get the caller's filename if no test name is given
  if testName is None:
      try:
          testName = os.path.basename(sys._getframe(1).f_code.co_filename)
      except:
          testName = "unknown"

  foundCrash = False
  dumps = glob.glob(os.path.join(dumpDir, '*.dmp'))
  for d in dumps:
    log.info("TEST-UNEXPECTED-FAIL | %s | application crashed (minidump found)", testName)
    if symbolsPath and stackwalkPath:
      nullfd = open(os.devnull, 'w')
      # eat minidump_stackwalk errors
      subprocess.call([stackwalkPath, d, symbolsPath], stderr=nullfd)
      nullfd.close()
    os.remove(d)
    extra = os.path.splitext(d)[0] + ".extra"
    if os.path.exists(extra):
        os.remove(extra)
    foundCrash = True
  return foundCrash
