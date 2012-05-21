#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


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
