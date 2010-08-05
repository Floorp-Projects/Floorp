#!/usr/bin/env python

"""
make.py

A drop-in or mostly drop-in replacement for GNU make.
"""

import sys, os
import pymake.command, pymake.process

import gc

if __name__ == '__main__':
  gc.disable()

  pymake.command.main(sys.argv[1:], os.environ, os.getcwd(), cb=sys.exit)
  pymake.process.ParallelContext.spin()
  assert False, "Not reached"
