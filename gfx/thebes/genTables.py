#!/usr/bin/python

from __future__ import print_function
import sys

def table_generator(f):
    return ",\n".join([", ".join(["0x%2.2x" % h for h in [f(i) for i in range(r,r+16)]]) for r in range(0, 65536, 16)])

def generate(output):
    output.write("const uint8_t gfxUtils::sPremultiplyTable[256*256] = {\n");
    output.write(table_generator(lambda i: ((i / 256) * (i % 256) + 254) / 255) + "\n")
    output.write("};\n");
    output.write("const uint8_t gfxUtils::sUnpremultiplyTable[256*256] = {\n");
    output.write(table_generator(lambda i: (i % 256) * 255 / ((i / 256) if (i / 256) > 0 else 255) % 256) + "\n")
    output.write("};\n");

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: genTables.py <header.h>", file=sys.stderr)
        sys.exit(1)
    with open(sys.argv[1], 'w') as f:
        generate(f)
