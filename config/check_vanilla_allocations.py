# vim: set ts=8 sts=4 et sw=4 tw=79:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# ----------------------------------------------------------------------------
# All heap allocations in SpiderMonkey must go through js_malloc, js_calloc,
# js_realloc, and js_free.  This is so that any embedder who uses a custom
# allocator (by defining JS_USE_CUSTOM_ALLOCATOR) will see all heap allocation
# go through that custom allocator.
#
# Therefore, the presence of any calls to "vanilla" allocation/free functions
# from within SpiderMonkey itself (e.g. malloc(), free()) is a bug.  Calls from
# within mozglue and non-SpiderMonkey locations are fine; there is a list of
# exceptions that can be added to as the need arises.
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
# ----------------------------------------------------------------------------

import argparse
import re
import subprocess
import sys
from collections import defaultdict

import buildconfig

# The obvious way to implement this script is to search for occurrences of
# malloc et al, succeed if none are found, and fail is some are found.
# However, "none are found" does not necessarily mean "none are present" --
# this script could be buggy.  (Or the output format of |nm| might change in
# the future.)
#
# So util/Utility.cpp deliberately contains a (never-called) function that
# contains a single use of all the vanilla allocation/free functions.  And this
# script fails if it (a) finds uses of those functions in files other than
# util/Utility.cpp, *or* (b) fails to find them in util/Utility.cpp.

# Tracks overall success of the test.
has_failed = False


def fail(msg):
    print("TEST-UNEXPECTED-FAIL | check_vanilla_allocations.py |", msg)
    global has_failed
    has_failed = True


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--aggressive",
        action="store_true",
        help="also check for malloc, calloc, realloc and free",
    )
    parser.add_argument("file", type=str, help="name of the file to check")
    args = parser.parse_args()

    # Run |nm|.  Options:
    # -C: demangle symbol names
    # -A: show an object filename for each undefined symbol
    nm = buildconfig.substs.get("NM") or "nm"
    cmd = [nm, "-C", "-A", args.file]
    lines = subprocess.check_output(
        cmd, universal_newlines=True, stderr=subprocess.PIPE
    ).split("\n")

    # alloc_fns contains all the vanilla allocation/free functions that we look
    # for. Regexp chars are escaped appropriately.

    operator_news = [
        # Matches |operator new(unsigned T)|, where |T| is |int| or |long|.
        r"operator new(unsigned",
        # Matches |operator new[](unsigned T)|, where |T| is |int| or |long|.
        r"operator new[](unsigned",
    ]

    # operator new may end up inlined and replaced with moz_xmalloc.
    inlined_operator_news = [
        r"moz_xmalloc",
    ]

    alloc_fns = (
        operator_news
        + inlined_operator_news
        + [
            r"memalign",
            # These three aren't available on all Linux configurations.
            # r'posix_memalign',
            # r'aligned_alloc',
            # r'valloc',
        ]
    )

    if args.aggressive:
        alloc_fns += [r"malloc", r"calloc", r"realloc", r"free", r"strdup"]

    # This is like alloc_fns, but regexp chars are not escaped.
    alloc_fns_escaped = [re.escape(fn) for fn in alloc_fns]

    # This regexp matches the relevant lines in the output of |nm|, which look
    # like the following.
    #
    #   js/src/libjs_static.a:Utility.o:                  U malloc
    #   js/src/libjs_static.a:Utility.o: 00000000000007e0 T js::SetSourceOptions(...)
    #
    # It may also, in LTO builds, look like
    #   js/src/libjs_static.a:Utility.o: ---------------- T js::SetSourceOptions(...)
    #
    nm_line_re = re.compile(r"([^:/ ]+):\s*(?:[0-9a-fA-F]*|-*)\s+([TUw]) (.*)")
    alloc_fns_re = re.compile(r"|".join(alloc_fns_escaped))

    # This tracks which allocation/free functions have been seen.
    functions = defaultdict(set)
    files = defaultdict(int)

    # Files to ignore allocation/free functions from.
    ignored_files = [
        # Ignore implicit call to operator new in std::condition_variable_any.
        #
        # From intl/icu/source/common/umutex.h:
        # On Linux, the default constructor of std::condition_variable_any
        # produces an in-line reference to global operator new(), [...].
        "umutex.o",
        # Ignore allocations from decimal conversion functions inside mozglue.
        "Decimal.o",
        # Ignore use of std::string in regexp AST debug output.
        "regexp-ast.o",
    ]
    all_ignored_files = set((f, 1) for f in ignored_files)

    # Would it be helpful to emit detailed line number information after a failure?
    emit_line_info = False

    prev_filename = None
    for line in lines:
        m = nm_line_re.search(line)
        if m is None:
            continue

        filename, symtype, fn = m.groups()
        if prev_filename != filename:
            # When the same filename appears multiple times, separated by other
            # file names, this denotes a different file. Thankfully, we can more
            # or less safely assume that dir1/Foo.o and dir2/Foo.o are not going
            # to be next to each other.
            files[filename] += 1
            prev_filename = filename

        # The stdc++compat library has an implicit call to operator new in
        # thread::_M_start_thread.
        if "stdc++compat" in filename:
            continue

        # The memory allocator code contains calls to memalign. These are ok, so
        # we whitelist them.
        if "_memory_" in filename:
            continue

        # Ignore the fuzzing code imported from m-c
        if "Fuzzer" in filename:
            continue

        # Ignore the profiling pseudo-stack, since it needs to run even when
        # SpiderMonkey's allocator isn't initialized.
        if "ProfilingStack" in filename:
            continue

        if symtype == "T":
            # We can't match intl/components files by file name because in
            # non-unified builds they overlap with files in js/src.
            # So we check symbols they define, and consider files with symbols
            # in the mozilla::intl namespace to be those.
            if fn.startswith("mozilla::intl::"):
                all_ignored_files.add((filename, files[filename]))
        else:
            m = alloc_fns_re.match(fn)
            if m:
                functions[(filename, files[filename])].add(m.group(0))

    util_Utility_cpp = functions.pop(("Utility.o", 1))
    if ("Utility.o", 2) in functions:
        fail("There should be only one Utility.o file")

    for f, n in all_ignored_files:
        functions.pop((f, n), None)
        if f in ignored_files and (f, 2) in functions:
            fail(f"There should be only one {f} file")

    for filename, n in sorted(functions):
        for fn in functions[(filename, n)]:
            # An allocation is present in a non-special file.  Fail!
            fail("'" + fn + "' present in " + filename)
            # Try to give more precise information about the offending code.
            emit_line_info = True

    # Check that all functions we expect are used in util/Utility.cpp.  (This
    # will fail if the function-detection code breaks at any point.)
    # operator new and its inlined equivalent are mutually exclusive.
    has_operator_news = any(fn in operator_news for fn in util_Utility_cpp)
    has_inlined_operator_news = any(
        fn in inlined_operator_news for fn in util_Utility_cpp
    )
    if has_operator_news and has_inlined_operator_news:
        fail(
            "Both operator new and moz_xmalloc aren't expected in util/Utility.cpp at the same time"
        )

    for fn in alloc_fns:
        if fn not in util_Utility_cpp:
            if (
                (fn in operator_news and not has_inlined_operator_news)
                or (fn in inlined_operator_news and not has_operator_news)
                or (fn not in operator_news and fn not in inlined_operator_news)
            ):
                fail("'" + fn + "' isn't used as expected in util/Utility.cpp")
        else:
            util_Utility_cpp.remove(fn)

    # This should never happen, but check just in case.
    if util_Utility_cpp:
        fail(
            "unexpected allocation fns used in util/Utility.cpp: "
            + ", ".join(util_Utility_cpp)
        )

    # If we found any improper references to allocation functions, try to use
    # DWARF debug info to get more accurate line number information about the
    # bad calls. This is a lot slower than 'nm -A', and it is not always
    # precise when building with --enable-optimized.
    if emit_line_info:
        print("check_vanilla_allocations.py: Source lines with allocation calls:")
        print(
            "check_vanilla_allocations.py: Accurate in unoptimized builds; "
            "util/Utility.cpp expected."
        )

        # Run |nm|.  Options:
        # -u: show only undefined symbols
        # -C: demangle symbol names
        # -l: show line number information for each undefined symbol
        cmd = ["nm", "-u", "-C", "-l", args.file]
        lines = subprocess.check_output(
            cmd, universal_newlines=True, stderr=subprocess.PIPE
        ).split("\n")

        # This regexp matches the relevant lines in the output of |nm -l|,
        # which look like the following.
        #
        #       U malloc util/Utility.cpp:117
        #
        alloc_lines_re = (
            r"[Uw] ((" + r"|".join(alloc_fns_escaped) + r").*)\s+(\S+:\d+)$"
        )

        for line in lines:
            m = re.search(alloc_lines_re, line)
            if m:
                print(
                    "check_vanilla_allocations.py:", m.group(1), "called at", m.group(3)
                )

    if has_failed:
        sys.exit(1)

    print("TEST-PASS | check_vanilla_allocations.py | ok")
    sys.exit(0)


if __name__ == "__main__":
    main()
