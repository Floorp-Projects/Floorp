#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# run the Winchecksec tool (https://github.com/trailofbits/winchecksec)
# against a given Windows binary.

import buildconfig
import json
import subprocess
import sys

# usage
if len(sys.argv) != 2:
    print("""usage : autowinchecksec.by path_to_binary""")
    sys.exit(0)

binary_path = sys.argv[1]

# execute winchecksec against the binary, using the WINCHECKSEC environment
# variable as the path to winchecksec.exe
try:
    winchecksec_path = buildconfig.substs['WINCHECKSEC']
except KeyError:
    print("TEST-UNEXPECTED-FAIL | autowinchecksec.py | WINCHECKSEC environment variable is "
          "not set, can't check DEP/ASLR etc. status.")
    sys.exit(1)

wine = buildconfig.substs.get('WINE')
if wine and winchecksec_path.lower().endswith('.exe'):
    cmd = [wine, winchecksec_path]
else:
    cmd = [winchecksec_path]

try:
    result = subprocess.check_output(cmd + ['-j', binary_path],
                                     universal_newlines=True)

except subprocess.CalledProcessError as e:
    print("TEST-UNEXPECTED-FAIL | autowinchecksec.py | Winchecksec returned error code %d:\n%s" % (
          e.returncode, e.output))
    sys.exit(1)


result = json.loads(result)

checks = [
    'aslr',
    'cfg',
    'dynamicBase',
    'gs',
    'isolation',
    'nx',
    'seh',
]

if buildconfig.substs['CPU_ARCH'] == 'x86':
    checks += [
        'safeSEH',
    ]
else:
    checks += [
        'highEntropyVA',
    ]

failed = [c for c in checks if result.get(c) is False]

if failed:
    print("TEST-UNEXPECTED-FAIL | autowinchecksec.py | Winchecksec reported %d error(s) for %s" %
          (len(failed), binary_path))
    print("TEST-UNEXPECTED-FAIL | autowinchecksec.py | The following check(s) failed: %s" %
          (', '.join(failed)))
    sys.exit(1)
else:
    print("TEST-PASS | autowinchecksec.py | %s succeeded" % binary_path)
