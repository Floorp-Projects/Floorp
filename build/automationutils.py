#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import with_statement
import logging
from operator import itemgetter
import os
import platform
import re
import signal
import subprocess
import sys
import tempfile
import mozinfo

__all__ = [
  'dumpScreen',
  "setAutomationLog",
  ]

log = logging.getLogger()
def resetGlobalLog():
  while log.handlers:
    log.removeHandler(log.handlers[0])
  handler = logging.StreamHandler(sys.stdout)
  log.setLevel(logging.INFO)
  log.addHandler(handler)
resetGlobalLog()

def setAutomationLog(alt_logger):
  global log
  log = alt_logger

# Python does not provide strsignal() even in the very latest 3.x.
# This is a reasonable fake.
def strsig(n):
  # Signal numbers run 0 through NSIG-1; an array with NSIG members
  # has exactly that many slots
  _sigtbl = [None]*signal.NSIG
  for k in dir(signal):
    if k.startswith("SIG") and not k.startswith("SIG_") and k != "SIGCLD" and k != "SIGPOLL":
      _sigtbl[getattr(signal, k)] = k
  # Realtime signals mostly have no names
  if hasattr(signal, "SIGRTMIN") and hasattr(signal, "SIGRTMAX"):
    for r in range(signal.SIGRTMIN+1, signal.SIGRTMAX+1):
      _sigtbl[r] = "SIGRTMIN+" + str(r - signal.SIGRTMIN)
  # Fill in any remaining gaps
  for i in range(signal.NSIG):
    if _sigtbl[i] is None:
      _sigtbl[i] = "unrecognized signal, number " + str(i)
  if n < 0 or n >= signal.NSIG:
    return "out-of-range signal, number "+str(n)
  return _sigtbl[n]

def printstatus(status, name = ""):
  # 'status' is the exit status
  if os.name != 'posix':
    # Windows error codes are easier to look up if printed in hexadecimal
    if status < 0:
      status += 2**32
    print "TEST-INFO | %s: exit status %x\n" % (name, status)
  elif os.WIFEXITED(status):
    print "TEST-INFO | %s: exit %d\n" % (name, os.WEXITSTATUS(status))
  elif os.WIFSIGNALED(status):
    # The python stdlib doesn't appear to have strsignal(), alas
    print "TEST-INFO | {}: killed by {}".format(name,strsig(os.WTERMSIG(status)))
  else:
    # This is probably a can't-happen condition on Unix, but let's be defensive
    print "TEST-INFO | %s: undecodable exit status %04x\n" % (name, status)

def dumpScreen(utilityPath):
  """dumps a screenshot of the entire screen to a directory specified by
  the MOZ_UPLOAD_DIR environment variable"""

  # Need to figure out which OS-dependent tool to use
  if mozinfo.isUnix:
    utility = [os.path.join(utilityPath, "screentopng")]
    utilityname = "screentopng"
  elif mozinfo.isMac:
    utility = ['/usr/sbin/screencapture', '-C', '-x', '-t', 'png']
    utilityname = "screencapture"
  elif mozinfo.isWin:
    utility = [os.path.join(utilityPath, "screenshot.exe")]
    utilityname = "screenshot"

  # Get dir where to write the screenshot file
  parent_dir = os.environ.get('MOZ_UPLOAD_DIR', None)
  if not parent_dir:
    log.info('Failed to retrieve MOZ_UPLOAD_DIR env var')
    return

  # Run the capture
  try:
    tmpfd, imgfilename = tempfile.mkstemp(prefix='mozilla-test-fail-screenshot_', suffix='.png', dir=parent_dir)
    os.close(tmpfd)
    returncode = subprocess.call(utility + [imgfilename])
    printstatus(returncode, utilityname)
  except OSError, err:
    log.info("Failed to start %s for screenshot: %s"
             % (utility[0], err.strerror))
    return
