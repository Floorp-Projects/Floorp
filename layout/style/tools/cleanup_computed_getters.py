#!/usr/bin/env python3

"""
Script to remove unused getters in nsComputedDOMStyle.

It needs to be run from the topsrcdir, and it requires passing in the objdir
as first argument. It can only be run after nsComputedDOMStyleGenerated.cpp
is generated in the objdir.
"""

import re
import sys

from pathlib import Path

if len(sys.argv) != 2:
    print("Usage: {} objdir".format(sys.argv[0]))
    exit(1)

generated = Path(sys.argv[1]) / "layout" / "style"
generated = generated / "nsComputedDOMStyleGenerated.cpp"
RE_GENERATED = re.compile(r"DoGet\w+")
keeping = set()
with generated.open() as f:
    for line in f:
        m = RE_GENERATED.search(line)
        if m is not None:
            keeping.add(m.group(0))

HEADER = "layout/style/nsComputedDOMStyle.h"
SOURCE = "layout/style/nsComputedDOMStyle.cpp"

# We need to keep functions invoked by others
RE_DEF = re.compile(r"nsComputedDOMStyle::(DoGet\w+)\(\)")
RE_SRC = re.compile(r"\b(DoGet\w+)\(\)")
with open(SOURCE, "r") as f:
    for line in f:
        m = RE_DEF.search(line)
        if m is not None:
            continue
        m = RE_SRC.search(line)
        if m is not None:
            keeping.add(m.group(1))

removing = set()
remaining_lines = []
with open(HEADER, "r") as f:
    for line in f:
        m = RE_SRC.search(line)
        if m is not None:
            name = m.group(1)
            if name not in keeping:
                print("Removing " + name)
                removing.add(name)
                continue
        remaining_lines.append(line)

with open(HEADER, "w", newline="") as f:
    f.writelines(remaining_lines)

remaining_lines = []
is_removing = False
with open(SOURCE, "r") as f:
    for line in f:
        if is_removing:
            if line == "}\n":
                is_removing = False
            continue
        m = RE_DEF.search(line)
        if m is not None:
            name = m.group(1)
            if name in removing:
                remaining_lines.pop()
                if remaining_lines[-1] == "\n":
                    remaining_lines.pop()
                is_removing = True
                continue
        remaining_lines.append(line)

with open(SOURCE, "w", newline="") as f:
    f.writelines(remaining_lines)
