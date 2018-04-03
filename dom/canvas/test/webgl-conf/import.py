#! /usr/bin/env python3
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

assert __name__ == '__main__'

from pathlib import *
import subprocess
import sys

REPO_DIR = Path.cwd()
DIR_IN_GECKO = Path(__file__).parent
assert not REPO_DIR.samefile(DIR_IN_GECKO), 'Run from the KhronosGroup/WebGL checkout.'
assert DIR_IN_GECKO.as_posix().endswith('dom/canvas/test/webgl-conf')

def print_now(*args):
    print(*args)
    sys.stdout.flush()


def run_checked(*args, **kwargs):
    print_now(' ', args)
    return subprocess.run(args, check=True, shell=True, **kwargs)


src_dir = Path(REPO_DIR, 'sdk/tests').as_posix()
dest_dir = Path(DIR_IN_GECKO, 'checkout').as_posix()
assert dest_dir.endswith('dom/canvas/test/webgl-conf/checkout') # Be paranoid with rm -rf.
run_checked("rm -rf '{}'".format(dest_dir))
run_checked("mkdir '{}'".format(dest_dir));
run_checked("cp -rT '{}' '{}'".format(src_dir, dest_dir));

print_now('Done!')
