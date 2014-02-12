#!/usr/bin/python

def table_generator(f):
    return ",\n".join([", ".join(["0x%2.2x" % h for h in [f(i) for i in range(r,r+16)]]) for r in range(0, 65536, 16)])

with open("DeprecatedPremultiplyTables.h", "w") as f:
  f.write("const uint8_t gfxUtils::sPremultiplyTable[256*256] = {\n");
  f.write(table_generator(lambda i: ((i / 256) * (i % 256) + 254) / 255) + "\n")
  f.write("};\n");
  f.write("const uint8_t gfxUtils::sUnpremultiplyTable[256*256] = {\n");
  f.write(table_generator(lambda i: (i % 256) * 255 / ((i / 256) if (i / 256) > 0 else 255) % 256) + "\n")
  f.write("};\n");
