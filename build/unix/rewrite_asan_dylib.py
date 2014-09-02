# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import os
import subprocess
import shutil

'''
Scans the given directories for binaries referencing the AddressSanitizer
runtime library, copies it to the main directory and rewrites binaries to not
reference it with absolute paths but with @executable_path instead.
'''

# This is the dylib we're looking for
DYLIB_NAME='libclang_rt.asan_osx_dynamic.dylib'

def scan_directory(path):
    dylibCopied = False

    for root, subdirs, files in os.walk(path):
        for filename in files:
            filename = os.path.join(root, filename)

            # Skip all files that aren't either dylibs or executable
            if not (filename.endswith('.dylib') or os.access(filename, os.X_OK)):
                continue

            try:
                otoolOut = subprocess.check_output(['otool', '-L', filename])
            except:
                # Errors are expected on non-mach executables, ignore them and continue
                continue

            for line in otoolOut.splitlines():
                if line.find(DYLIB_NAME) != -1:
                    absDylibPath = line.split()[0]

                    # Don't try to rewrite binaries twice
                    if absDylibPath.find('@executable_path/') == 0:
                        continue

                    if not dylibCopied:
                        # Copy the runtime once to the main directory, which is passed
                        # as the argument to this function.
                        shutil.copy(absDylibPath, path)

                        # Now rewrite the library itself
                        subprocess.check_call(['install_name_tool', '-id', '@executable_path/' + DYLIB_NAME, os.path.join(path, DYLIB_NAME)])
                        dylibCopied = True

                    # Now use install_name_tool to rewrite the path in our binary
                    subprocess.check_call(['install_name_tool', '-change', absDylibPath, '@executable_path/' + DYLIB_NAME, filename])
                    break

if __name__ == '__main__':
    for d in sys.argv[1:]:
        scan_directory(d)
