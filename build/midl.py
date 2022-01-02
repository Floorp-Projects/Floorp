# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import buildconfig
import subprocess
import os
import sys


def relativize(path, base=None):
    # For absolute path in Unix builds, we need relative paths because
    # Windows programs run via Wine don't like these Unix absolute paths
    # (they look like command line arguments).
    if path.startswith("/"):
        return os.path.relpath(path, base)
    # For Windows absolute paths, we can just use the unmodified path.
    # And if the path starts with '-', it's a command line argument.
    if os.path.isabs(path) or path.startswith("-"):
        return path
    # Remaining case is relative paths, which may be relative to a different
    # directory (os.getcwd()) than the needed `base`, so we "rebase" it.
    return os.path.relpath(path, base)


def midl(out, input, *flags):
    out.avoid_writing_to_file()
    midl = buildconfig.substs["MIDL"]
    wine = buildconfig.substs.get("WINE")
    base = os.path.dirname(out.name) or "."
    if midl.lower().endswith(".exe") and wine:
        command = [wine, midl]
    else:
        command = [midl]
    command.extend(buildconfig.substs["MIDL_FLAGS"])
    command.extend([relativize(f, base) for f in flags])
    command.append("-Oicf")
    command.append(relativize(input, base))
    print("Executing:", " ".join(command))
    result = subprocess.run(command, cwd=base)
    return result.returncode


# midl outputs dlldata to a single dlldata.c file by default. This prevents running
# midl in parallel in the same directory for idl files that would generate dlldata.c
# because of race conditions updating the file. Instead, we ask midl to create
# separate files, and we merge them manually.
def merge_dlldata(out, *inputs):
    inputs = [open(i) for i in inputs]
    read_a_line = [True] * len(inputs)
    while True:
        lines = [
            f.readline() if read_a_line[n] else lines[n] for n, f in enumerate(inputs)
        ]
        unique_lines = set(lines)
        if len(unique_lines) == 1:
            # All the lines are identical
            if not lines[0]:
                break
            out.write(lines[0])
            read_a_line = [True] * len(inputs)
        elif (
            len(unique_lines) == 2
            and len([l for l in unique_lines if "#define" in l]) == 1
        ):
            # Most lines are identical. When they aren't, it's typically because some
            # files have an extra #define that others don't. When that happens, we
            # print out the #define, and get a new input line from the files that had
            # a #define on the next iteration. We expect that next line to match what
            # the other files had on this iteration.
            # Note: we explicitly don't support the case where there are different
            # defines across different files, except when there's a different one
            # for each file, in which case it's handled further below.
            a = unique_lines.pop()
            if "#define" in a:
                out.write(a)
            else:
                out.write(unique_lines.pop())
            read_a_line = ["#define" in l for l in lines]
        elif len(unique_lines) != len(lines):
            # If for some reason, we don't get lines that are entirely different
            # from each other, we have some unexpected input.
            print(
                "Error while merging dlldata. Last lines read: {}".format(lines),
                file=sys.stderr,
            )
            return 1
        else:
            for line in lines:
                out.write(line)
            read_a_line = [True] * len(inputs)

    return 0
