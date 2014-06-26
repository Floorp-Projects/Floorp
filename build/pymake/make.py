#!/usr/bin/env python

"""
make.py

A drop-in or mostly drop-in replacement for GNU make.
"""

import sys, os
import pymake.command, pymake.process

import gc

if __name__ == '__main__':
  if 'TINDERBOX_OUTPUT' in os.environ:
    # When building on mozilla build slaves, execute mozmake instead. Until bug
    # 978211, this is the easiest, albeit hackish, way to do this.
    import subprocess
    mozmake = os.path.join(os.path.dirname(__file__), '..', '..',
        'mozmake.exe')
    cmd = [mozmake]
    cmd.extend(sys.argv[1:])
    shell = os.environ.get('SHELL')
    if shell and not shell.lower().endswith('.exe'):
        cmd += ['SHELL=%s.exe' % shell]
    sys.exit(subprocess.call(cmd))

  sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', 0)
  sys.stderr = os.fdopen(sys.stderr.fileno(), 'w', 0)

  gc.disable()

  pymake.command.main(sys.argv[1:], os.environ, os.getcwd(), cb=sys.exit)
  pymake.process.ParallelContext.spin()
  assert False, "Not reached"
