#!/usr/bin/env python3

"""
Script to remove unused keyword tables in nsCSSProps.

It needs to be run from the topsrcdir.
"""

import re
import subprocess

from pathlib import Path

HEADER = Path("layout/style/nsCSSProps.h")
SOURCE = Path("layout/style/nsCSSProps.cpp")

RE_TABLE = re.compile(r"\b(k\w+KTable)")
rg_result = subprocess.check_output(["rg", r"\bk\w+KTable"], encoding="UTF-8")
to_keep = set()
all = set()
for item in rg_result.splitlines():
    file, line = item.split(':', 1)
    name = RE_TABLE.search(line).group(1)
    path = Path(file)
    if path != HEADER and path != SOURCE:
        to_keep.add(name)
    else:
        all.add(name)

to_remove = all - to_keep
remaining_lines = []
with HEADER.open() as f:
    for line in f:
        m = RE_TABLE.search(line)
        if m is not None and m.group(1) in to_remove:
            print("Removing " + m.group(1))
            continue
        remaining_lines.append(line)
with HEADER.open("w", newline="") as f:
    f.writelines(remaining_lines)

remaining_lines = []
removing = False
RE_DEF = re.compile(r"KTableEntry nsCSSProps::(k\w+KTable)\[\]")
with SOURCE.open() as f:
    for line in f:
        if removing:
            if line == "};\n":
                removing = False
            continue
        m = RE_DEF.search(line)
        if m is not None and m.group(1) in to_remove:
            if remaining_lines[-1] == "\n":
                remaining_lines.pop()
            removing = True
            continue
        remaining_lines.append(line)
with SOURCE.open("w", newline="") as f:
    f.writelines(remaining_lines)
