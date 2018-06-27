#!/usr/bin/env python3

"""
Script to remove unused keywords from nsCSSKeywordList.

It needs to be run from the topsrcdir.
"""

import re
import subprocess

LIST_FILE = "layout/style/nsCSSKeywordList.h"

RE_KEYWORD = re.compile(r"\beCSSKeyword_(\w+)")
rg_result = subprocess.check_output(["rg", r"eCSSKeyword_\w+"], encoding="UTF-8")
to_keep = set()
for item in rg_result.splitlines():
    file, line = item.split(':', 1)
    for m in RE_KEYWORD.finditer(line):
        to_keep.add(m.group(1))

remaining_lines = []
RE_ITEM = re.compile(r"CSS_KEY\(.+, (\w+)\)")
with open(LIST_FILE, "r") as f:
    for line in f:
        m = RE_ITEM.search(line)
        if m is not None and m.group(1) not in to_keep:
            print("Removing " + m.group(1))
            continue
        remaining_lines.append(line)
with open(LIST_FILE, "w", newline="") as f:
    f.writelines(remaining_lines)
