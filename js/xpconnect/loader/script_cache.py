#!/usr/bin/env python

from __future__ import absolute_import, print_function

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import io
import os
import struct
import sys

MAGIC = b"mozXDRcachev002\0"


def usage():
    print(
        """Usage: script_cache.py <file.bin> ...

        Decodes and prints out the contents of a startup script cache file
        (e.g., startupCache/scriptCache.bin) in human-readable form."""
    )

    sys.exit(1)


class ProcessTypes:
    Uninitialized = 0
    Parent = 1
    Web = 2
    Extension = 3
    Privileged = 4

    def __init__(self, val):
        self.val = val

    def __str__(self):
        res = []
        if self.val & (1 << self.Uninitialized):
            raise Exception("Uninitialized process type")
        if self.val & (1 << self.Parent):
            res.append("Parent")
        if self.val & (1 << self.Web):
            res.append("Web")
        if self.val & (1 << self.Extension):
            res.append("Extension")
        if self.val & (1 << self.Privileged):
            res.append("Privileged")
        return "|".join(res)


class InputBuffer(object):
    def __init__(self, data):
        self.data = data
        self.offset = 0

    @property
    def remaining(self):
        return len(self.data) - self.offset

    def unpack(self, fmt):
        res = struct.unpack_from(fmt, self.data, self.offset)
        self.offset += struct.calcsize(fmt)
        return res

    def unpack_str(self):
        (size,) = self.unpack("<H")
        res = self.data[self.offset : self.offset + size].decode("utf-8")
        self.offset += size
        return res


if len(sys.argv) < 2 or not os.path.exists(sys.argv[1]):
    usage()

for filename in sys.argv[1:]:
    with io.open(filename, "rb") as f:
        magic = f.read(len(MAGIC))
        if magic != MAGIC:
            raise Exception("Bad magic number")

        (hdrSize,) = struct.unpack("<I", f.read(4))

        hdr = InputBuffer(f.read(hdrSize))

        i = 0
        while hdr.remaining:
            i += 1
            print("{}: {}".format(i, hdr.unpack_str()))
            print("  Key:       {}".format(hdr.unpack_str()))
            print("  Offset:    {:>9,}".format(*hdr.unpack("<I")))
            print("  Size:      {:>9,}".format(*hdr.unpack("<I")))
            print("  Processes: {}".format(ProcessTypes(*hdr.unpack("B"))))
            print("")
