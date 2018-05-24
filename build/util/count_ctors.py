#!/usr/bin/python
import json

import re
import subprocess
import sys


def count_ctors(filename):
    proc = subprocess.Popen(
        ['readelf', '-W', '-S', filename], stdout=subprocess.PIPE)

    # Some versions of ld produce both .init_array and .ctors.  So we have
    # to check for both.
    n_init_array_ctors = 0
    have_init_array = False
    n_ctors_ctors = 0
    have_ctors = False

    for line in proc.stdout:
        f = line.split()
        if len(f) != 11:
            continue
        # Don't try to int()-parse the header line for the section summaries.
        if not re.match("\\[\\d+\\]", f[0]):
            continue
        section_name, contents, size, align = f[1], f[2], int(f[5], 16), int(f[10])
        if section_name == ".ctors" and contents == "PROGBITS":
            have_ctors = True
            # Subtract 2 for the uintptr_t(-1) header and the null terminator.
            n_ctors_ctors = size / align - 2
        if section_name == ".init_array" and contents == "INIT_ARRAY":
            have_init_array = True
            n_init_array_ctors = size / align

    if have_init_array:
        # Even if we have .ctors, we shouldn't have any constructors in .ctors.
        # Complain if .ctors does not look how we expect it to.
        if have_ctors and n_ctors_ctors != 0:
            print >>sys.stderr, "Unexpected .ctors contents for", filename
            sys.exit(1)
        return n_init_array_ctors
    if have_ctors:
        return n_ctors_ctors

    # We didn't find anything; somebody switched initialization mechanisms on
    # us, or the binary is completely busted.  Complain either way.
    print >>sys.stderr, "Couldn't find .init_array or .ctors in", filename
    sys.exit(1)


if __name__ == '__main__':
    for f in sys.argv[1:]:
        perfherder_data = {
            "framework": {"name": "build_metrics"},
            "suites": [{
                "name": "compiler_metrics",
                "subtests": [{
                    "name": "num_static_constructors",
                    "value": count_ctors(f),
                    "alertChangeType": "absolute",
                    "alertThreshold": 3
                }]}
            ]
        }
        print("PERFHERDER_DATA: %s" % json.dumps(perfherder_data))
