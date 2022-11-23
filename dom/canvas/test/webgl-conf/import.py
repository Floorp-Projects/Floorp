#! /usr/bin/env python3
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

assert __name__ == "__main__"

import shutil
import sys
from pathlib import Path

REL_PATH = "/dom/canvas/test/webgl-conf"
REPO_DIR = Path.cwd()
DIR_IN_GECKO = Path(__file__).parent
assert not REPO_DIR.samefile(
    DIR_IN_GECKO
), "Run this script from the source git checkout."
assert DIR_IN_GECKO.as_posix().endswith(REL_PATH)  # Be paranoid with rm -rf.

gecko_base_dir = DIR_IN_GECKO.as_posix()[: -len(REL_PATH)]
angle_dir = Path(gecko_base_dir, "gfx/angle").as_posix()
sys.path.append(angle_dir)
from vendor_from_git import print_now, record_cherry_picks

# --

(MERGE_BASE_ORIGIN,) = sys.argv[1:]  # Not always 'origin'!
record_cherry_picks(DIR_IN_GECKO, MERGE_BASE_ORIGIN)

# --

src_dir = Path(REPO_DIR, "sdk/tests")
dest_dir = Path(DIR_IN_GECKO, "checkout")
print_now("Nuking old checkout...")
shutil.rmtree(dest_dir, True)
print_now("Writing new checkout...")
shutil.copytree(src_dir, dest_dir, copy_function=shutil.copy)

print_now("Done!")
