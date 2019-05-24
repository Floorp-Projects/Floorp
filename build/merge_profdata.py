# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import glob
import subprocess
import sys
import buildconfig


def main(_, profile_dir):
    profraw_files = glob.glob(profile_dir + '/*.profraw')
    if not profraw_files:
        print('Could not find any profraw files in ' + profile_dir)
        sys.exit(1)

    subprocess.check_call([buildconfig.substs['LLVM_PROFDATA'], 'merge',
                           '-o', 'merged.profdata'] + profraw_files)
