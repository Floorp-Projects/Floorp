#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from fluent.syntax import parse
from fluent.syntax.ast import Message
import sys


def find_error_ids(filename, known_strings):
    with open(filename, "r", encoding="utf-8") as f:
        known_strings += [
            m.id.name for m in parse(f.read()).body if isinstance(m, Message)
        ]


def main(output, *filenames):
    known_strings = []
    for filename in filenames:
        find_error_ids(filename, known_strings)

    output.write("const KNOWN_ERROR_MESSAGE_IDS = new Set([\n")
    for known_string in known_strings:
        output.write('  "{}",\n'.format(known_string))
    output.write("]);\n")


if __name__ == "__main__":
    sys.exit(main(sys.stdout, *sys.argv[1:]))
