#!/usr/bin/python

def table_generator(f):
    return ",\n".join([", ".join(["0x%2.2x" % h for h in [f(i) for i in range(r,r+16)]]) for r in range(0, 65536, 16)])

f = open("sPremultiplyTable.h", "w")
f.write(table_generator(lambda i: ((i / 256) * (i % 256) + 254) / 255) + "\n")
f.close()

f = open("sUnpremultiplyTable.h", "w")
f.write(table_generator(lambda i: (i % 256) * 255 / ((i / 256) if (i / 256) > 0 else 255) % 256) + "\n")
f.close()
