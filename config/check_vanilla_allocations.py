# vim: set ts=8 sts=4 et sw=4 tw=79:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#----------------------------------------------------------------------------
# All heap allocations in SpiderMonkey must go through js_malloc, js_calloc,
# js_realloc, and js_free.  This is so that any embedder who uses a custom
# allocator (by defining JS_USE_CUSTOM_ALLOCATOR) will see all heap allocation
# go through that custom allocator.
#
# Therefore, the presence of any calls to "vanilla" allocation/free functions
# (e.g. malloc(), free()) is a bug.
#
# This script checks for the presence of such disallowed vanilla
# allocation/free function in SpiderMonkey when it's built as a library.  It
# relies on |nm| from the GNU binutils, and so only works on Linux, but one
# platform is good enough to catch almost all violations.
#
# This checking is only 100% reliable in a JS_USE_CUSTOM_ALLOCATOR build in
# which the default definitions of js_malloc et al (in Utility.h) -- which call
# malloc et al -- are replaced with empty definitions.  This is because the
# presence and possible inlining of the default js_malloc et al can cause
# malloc/calloc/realloc/free calls show up in unpredictable places.
#
# Unfortunately, that configuration cannot be tested on Mozilla's standard
# testing infrastructure.  Instead, by default this script only tests that none
# of the other vanilla allocation/free functions (operator new, memalign, etc)
# are present.  If given the --aggressive flag, it will also check for
# malloc/calloc/realloc/free.
#
# Note:  We don't check for |operator delete| and |operator delete[]|.  These
# can be present somehow due to virtual destructors, but this is not too
# because vanilla delete/delete[] calls don't make sense without corresponding
# vanilla new/new[] calls, and any explicit calls will be caught by Valgrind's
# mismatched alloc/free checking.
#----------------------------------------------------------------------------

from __future__ import print_function

import argparse
import re
import subprocess
import sys

# The obvious way to implement this script is to search for occurrences of
# malloc et al, succeed if none are found, and fail is some are found.
# However, "none are found" does not necessarily mean "none are present" --
# this script could be buggy.  (Or the output format of |nm| might change in
# the future.)
#
# So jsutil.cpp deliberately contains a (never-called) function that contains a
# single use of all the vanilla allocation/free functions.  And this script
# fails if it (a) finds uses of those functions in files other than jsutil.cpp,
# *or* (b) fails to find them in jsutil.cpp.

# Tracks overall success of the test.
has_failed = False


def fail(msg):
    print('TEST-UNEXPECTED-FAIL | check_vanilla_allocations.py |', msg)
    global has_failed
    has_failed = True


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--aggressive', action='store_true',
                        help='also check for malloc, calloc, realloc and free')
    parser.add_argument('file', type=str,
                        help='name of the file to check')
    args = parser.parse_args()

    # Run |nm|.  Options:
    # -u: show only undefined symbols
    # -C: demangle symbol names
    # -l: show a filename and line number for each undefined symbol
    cmd = ['nm', '-u', '-C', '-l', args.file]
    lines = subprocess.check_output(cmd, universal_newlines=True,
                                    stderr=subprocess.PIPE).split('\n')

    # alloc_fns contains all the vanilla allocation/free functions that we look
    # for. Regexp chars are escaped appropriately.

    alloc_fns = [
        # Matches |operator new(unsigned T)|, where |T| is |int| or |long|.
        r'operator new\(unsigned',

        # Matches |operator new[](unsigned T)|, where |T| is |int| or |long|.
        r'operator new\[\]\(unsigned',

        r'memalign',
        # These two aren't available on all Linux configurations.
        #r'posix_memalign',
        #r'aligned_alloc',
        r'valloc',
        r'strdup'
    ]

    if args.aggressive:
        alloc_fns += [
            r'malloc',
            r'calloc',
            r'realloc',
            r'free'
        ]

    # This is like alloc_fns, but regexp chars are not escaped.
    alloc_fns_unescaped = [fn.translate(None, r'\\') for fn in alloc_fns]

    # This regexp matches the relevant lines in the output of |nm|, which look
    # like the following.
    #
    #   U malloc  /path/to/objdir/dist/include/js/Utility.h:142
    #
    alloc_fns_re = r'U (' + r'|'.join(alloc_fns) + r').*\/([\w\.]+):(\d+)$'

    # This tracks which allocation/free functions have been seen in jsutil.cpp.
    jsutil_cpp = set([])

    for line in lines:
        m = re.search(alloc_fns_re, line)
        if m is None:
            continue

        fn = m.group(1)
        filename = m.group(2)
        linenum = m.group(3)
        if filename == 'jsutil.cpp':
            jsutil_cpp.add(fn)
        else:
            # An allocation is present in a non-special file.  Fail!
            fail("'" + fn + "' present at " + filename + ':' + linenum)


    # Check that all functions we expect are used in jsutil.cpp.  (This will
    # fail if the function-detection code breaks at any point.)
    for fn in alloc_fns_unescaped:
        if fn not in jsutil_cpp:
            fail("'" + fn + "' isn't used as expected in jsutil.cpp")
        else:
            jsutil_cpp.remove(fn)

    # This should never happen, but check just in case.
    if jsutil_cpp:
        fail('unexpected allocation fns used in jsutil.cpp: ' +
             ', '.join(jsutil_cpp))

    if has_failed:
        sys.exit(1)

    print('TEST-PASS | check_vanilla_allocations.py | ok')
    sys.exit(0)


if __name__ == '__main__':
    main()

