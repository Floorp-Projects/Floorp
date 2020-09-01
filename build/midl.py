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
    if path.startswith('/'):
        return os.path.relpath(path, base)
    # For Windows absolute paths, we can just use the unmodified path.
    # And if the path starts with '-', it's a command line argument.
    if os.path.isabs(path) or path.startswith('-'):
        return path
    # Remaining case is relative paths, which may be relative to a different
    # directory (os.getcwd()) than the needed `base`, so we "rebase" it.
    return os.path.relpath(path, base)


def midl(out, input, *flags):
    out.avoid_writing_to_file()
    midl = buildconfig.substs['MIDL']
    wine = buildconfig.substs.get('WINE')
    base = os.path.dirname(out.name) or '.'
    if midl.lower().endswith('.exe') and wine:
        command = [wine, midl]
    else:
        command = [midl]
    command.extend(buildconfig.substs['MIDL_FLAGS'])
    command.extend([relativize(f, base) for f in flags])
    command.append('-Oicf')
    command.append(relativize(input, base))
    print('Executing:', ' '.join(command))
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
        lines = [f.readline() if read_a_line[n] else '\n' for n, f in enumerate(inputs)]
        unique_lines = set(lines)
        if len(unique_lines) == 1:
            # All the lines are identical
            if not lines[0]:
                break
            out.write(lines[0])
            read_a_line = [True] * len(inputs)
        elif len(unique_lines) == 2 and '\n' in unique_lines:
            # Most lines are identical, except for some that are empty.
            # In this case, we print out the line, but on next iteration, don't read
            # a new line from the inputs that had nothing. This typically happens when
            # some files have #defines that others don't.
            unique_lines.remove('\n')
            out.write(unique_lines.pop())
            read_a_line = [l != '\n' for l in lines]
        elif len(unique_lines) != len(lines):
            # If for some reason, we don't get lines that are entirely different
            # from each other, we have some unexpected input.
            print('Error while merging dlldata. Last lines read: {}'.format(lines),
                  file=sys.stderr)
            return 1
        else:
            for line in lines:
                out.write(line)
            read_a_line = [True] * len(inputs)

    return 0
