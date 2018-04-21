#! /usr/bin/env python3
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

assert __name__ == '__main__'

from pathlib import *
import subprocess
import sys

REL_PATH = '/dom/canvas/test/webgl-conf'
REPO_DIR = Path.cwd()
DIR_IN_GECKO = Path(__file__).parent
assert not REPO_DIR.samefile(DIR_IN_GECKO), 'Run this script from the source git checkout.'
assert DIR_IN_GECKO.as_posix().endswith(REL_PATH) # Be paranoid with rm -rf.

gecko_base_dir = DIR_IN_GECKO.as_posix()[:-len(REL_PATH)]
angle_dir = Path(gecko_base_dir, 'gfx/angle').as_posix()
sys.path.append(angle_dir)
from vendor_from_git import *

# --

merge_base_from = sys.argv[1]
record_cherry_picks(DIR_IN_GECKO, merge_base_from)

# --

src_dir = Path(REPO_DIR, 'sdk/tests').as_posix()
dest_dir = Path(DIR_IN_GECKO, 'checkout').as_posix()
run_checked("rm -rI '{}'".format(dest_dir), shell=True)
run_checked("mkdir '{}'".format(dest_dir), shell=True);
run_checked("cp -rT '{}' '{}'".format(src_dir, dest_dir), shell=True);

print_now('Done!')
