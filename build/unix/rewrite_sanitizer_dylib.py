# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from argparse import ArgumentParser
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
from buildconfig import substs

"""
Scans the given directories for binaries referencing the AddressSanitizer
runtime library, copies it to the main directory.
"""

# This is the dylib name pattern
DYLIB_NAME_PATTERN = re.compile(r"libclang_rt\.(a|ub)san_osx_dynamic\.dylib")


def resolve_rpath(filename):
    otoolOut = subprocess.check_output([substs["OTOOL"], "-l", filename], text=True)
    currentCmd = None

    # The lines we need to find look like this:
    # ...
    # Load command 22
    #           cmd LC_RPATH
    #       cmdsize 80
    #          path /home/build/src/clang/bin/../lib/clang/3.8.0/lib/darwin (offset 12)
    # Load command 23
    # ...
    # Other load command types have a varying number of fields.
    for line in otoolOut.splitlines():
        cmdMatch = re.match(r"^\s+cmd ([A-Z_]+)", line)
        if cmdMatch is not None:
            currentCmd = cmdMatch.group(1)
            continue

        if currentCmd == "LC_RPATH":
            pathMatch = re.match(r"^\s+path (.*) \(offset \d+\)", line)
            if pathMatch is not None:
                path = pathMatch.group(1)
                if Path(path).is_dir():
                    return path

    print(f"@rpath could not be resolved from {filename}", file=sys.stderr)
    sys.exit(1)


def scan_directory(path):
    dylibsCopied = set()
    dylibsRequired = set()

    if not path.is_dir():
        print(f"Input path {path} is not a folder", file=sys.stderr)
        sys.exit(1)

    for file in path.rglob("*"):

        if not file.is_file():
            continue

        # Skip all files that aren't either dylibs or executable
        if not (file.suffix == ".dylib" or os.access(str(file), os.X_OK)):
            continue

        try:
            otoolOut = subprocess.check_output(
                [substs["OTOOL"], "-L", str(file)], text=True
            )
        except Exception:
            # Errors are expected on non-mach executables, ignore them and continue
            continue

        for line in otoolOut.splitlines():
            match = DYLIB_NAME_PATTERN.search(line)

            if match is not None:
                dylibName = match.group(0)
                absDylibPath = line.split()[0]

                dylibsRequired.add(dylibName)

                if dylibName not in dylibsCopied:
                    if absDylibPath.startswith("@rpath/"):
                        rpath = resolve_rpath(str(file))
                        copyDylibPath = absDylibPath.replace("@rpath", rpath)
                    else:
                        copyDylibPath = absDylibPath

                    if Path(copyDylibPath).is_file():
                        # Copy the runtime once to the main directory, which is passed
                        # as the argument to this function.
                        shutil.copy(copyDylibPath, str(path))
                        dylibsCopied.add(dylibName)
                    else:
                        print(
                            f"dylib path in {file} was not found at: {copyDylibPath}",
                            file=sys.stderr,
                        )

                break

    dylibsMissing = dylibsRequired - dylibsCopied
    if dylibsMissing:
        for dylibName in dylibsMissing:
            print(f"{dylibName} could not be found", file=sys.stderr)
        sys.exit(1)


def parse_args(argv=None):
    parser = ArgumentParser()
    parser.add_argument("paths", metavar="path", type=Path, nargs="+")
    return parser.parse_args(argv)


if __name__ == "__main__":
    args = parse_args()
    for d in args.paths:
        scan_directory(d)
