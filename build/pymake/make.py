#!/usr/bin/env python

"""
make.py

A drop-in or mostly drop-in replacement for GNU make.
"""

import sys, os
import pymake.command, pymake.process

import gc

if __name__ == '__main__':
  sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', 0)
  sys.stderr = os.fdopen(sys.stderr.fileno(), 'w', 0)

  gc.disable()

  # This is meant as a temporary workaround until issues with many targets
  # and prerequisites is addressed. Bug 874210 tracks.
  try:
      sys.setrecursionlimit(2 * sys.getrecursionlimit())
  except Exception:
      print >>sys.stderr, 'Unable to increase Python recursion limit.'

  pymake.command.main(sys.argv[1:], os.environ, os.getcwd(), cb=sys.exit)
  pymake.process.ParallelContext.spin()
  assert False, "Not reached"
