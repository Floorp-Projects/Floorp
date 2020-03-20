#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# run Microsoft's Binscope tool (https://www.microsoft.com/en-us/download/details.aspx?id=44995)
# against a fresh Windows build. output a 'binscope.log' file with full details
# of the run and appropriate strings to integrate with the buildbots

# the symbol dir should point to the symbol dir hierarchy created
# via running make buildsymbols in a windows build's objdir

import os
import shutil
import subprocess
import sys
import tempfile

# usage
if len(sys.argv) != 3:
    print("""usage : autobinscope.by path_to_binary path_to_symbols""")
    sys.exit(0)

binary_path = sys.argv[1]
symbol_path = sys.argv[2]

# execute binscope against the binary, using the BINSCOPE environment
# variable as the path to binscope.exe
try:
    binscope_path = os.environ['BINSCOPE']
except KeyError:
    print("TEST-UNEXPECTED-FAIL | autobinscope.py | BINSCOPE environment variable is not set, "
          "can't check DEP/ASLR etc. status.")
    sys.exit(1)

try:
    tmpdir = tempfile.mkdtemp()
    proc = subprocess.Popen([
        binscope_path,
        "/NoLogo",
        "/OutDir", tmpdir,
        "/Target", binary_path,
        "/SymPath", symbol_path,
        # ATLVersionCheck triggers a crash in msdia120: bug 1525113
        "/SkippedChecks", "ATLVersionCheck",
        "/Checks", "ATLVulnCheck",
        # We do not ship in the Windows Store
        "/SkippedChecks", "AppContainerCheck",
        # The CompilerVersionCheck doesn't like clang-cl (we would need to set MinimumCompilerVersion)  # NOQA: E501
        # But we check the compiler in our build system anyway, so this doesn't seem useful
        "/SkippedChecks", "CompilerVersionCheck",
        "/Checks", "DBCheck",
        "/Checks", "DefaultGSCookieCheck",
        "/Checks", "ExecutableImportsCheck",
        # FunctonPointersCheck is disabled per bug 1014002
        "/SkippedChecks", "FunctionPointersCheck",
        # GSCheck doesn't know how to deal with Rust libs
        "/SkippedChecks", "GSCheck",
        "/Checks", "GSFriendlyInitCheck",
        # We are not safebuffers-clean, bug 1449951
        "/SkippedChecks", "GSFunctionSafeBuffersCheck",
        "/Checks", "HighEntropyVACheck",
        "/Checks", "NXCheck",
        "/Checks", "RSA32Check",
        "/Checks", "SafeSEHCheck",
        "/Checks", "SharedSectionCheck",
        # VB6Check triggers a crash in msdia120: bug 1525113
        "/SkippedChecks", "VB6Check",
        "/Checks", "WXCheck"
    ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)

except WindowsError, (errno, strerror):  # noqa
    if errno != 2 and errno != 3:
        print("TEST-UNEXPECTED-FAIL | autobinscope.py | Unexpected error %d : %s" (
            errno, strerror))
        sys.exit(1)
    else:
        print("TEST-UNEXPECTED-FAIL | autobinscope.py | Could not locate binscope at location : "
              "%s\n" % binscope_path)
        sys.exit(1)
finally:
    shutil.rmtree(tmpdir)

proc.wait()

output = proc.communicate()[1].decode('utf-8').splitlines()

errors = 0
for line in output:
    print(line)
    if 'error' in line:
        errors += 1

if proc.returncode != 0:
    print("TEST-UNEXPECTED-FAIL | autobinscope.py | Binscope returned error code %d for file %s" %
          (proc.returncode, binary_path))
    sys.exit(1)
elif errors != 0:
    print("TEST-UNEXPECTED-FAIL | autobinscope.py | Binscope reported %d error(s) for file %s" % (
        errors, binary_path))
    sys.exit(1)
else:
    print("TEST-PASS | autobinscope.py | %s succeeded" % binary_path)
