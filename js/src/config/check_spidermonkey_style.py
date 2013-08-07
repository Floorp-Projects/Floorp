# vim: set ts=8 sts=4 et sw=4 tw=99:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#----------------------------------------------------------------------------
# This script checks various aspects of SpiderMonkey code style.  The current checks are as
# follows.
#
# We check the following things in headers.
#
# - No cyclic dependencies.
# 
# - No normal header should #include a inlines.h/-inl.h file.
# 
# - #ifndef wrappers should have the right form. (XXX: not yet implemented)
#   - Every header file should have one.
#   - It should be in the vanilla form and have no tokens before/after it so
#     that GCC and clang can avoid multiple-inclusion.
#   - The guard name used should be appropriate for the filename.
# 
# We check the following things in all files.
#
# - #includes should have full paths, e.g. "ion/Ion.h", not "Ion.h". 
#
# - #includes should use the appropriate form for system headers (<...>) and
#   local headers ("...").
#
# - #includes should be ordered correctly. (XXX: not yet implemented;  see bug
#   888088)
#   - Each one should be in the correct section.
#   - Alphabetical order should be used within sections.
#   - Sections should be in the right order.
#----------------------------------------------------------------------------

from __future__ import print_function

import difflib
import os
import re
import subprocess
import sys
import traceback

# We don't bother checking files in these directories, because they're (a) auxiliary or (b)
# imported code that doesn't follow our coding style.
ignored_js_src_dirs = [
   'js/src/config/',            # auxiliary stuff
   'js/src/ctypes/libffi/',     # imported code
   'js/src/devtools/',          # auxiliary stuff
   'js/src/editline/',          # imported code
   'js/src/gdb/',               # auxiliary stuff
   'js/src/vtune/'              # imported code
]

# We ignore #includes of these files, because they don't follow the usual rules.
included_inclnames_to_ignore = set([
    'ffi.h',                    # generated in ctypes/libffi/
    'devtools/sharkctl.h',      # we ignore devtools/ in general
    'devtools/Instruments.h',   # we ignore devtools/ in general
    'double-conversion.h',      # strange MFBT case
    'javascript-trace.h',       # generated in $OBJDIR if HAVE_DTRACE is defined
    'jsautokw.h',               # generated in $OBJDIR
    'jsautooplen.h',            # generated in $OBJDIR
    'jscustomallocator.h',      # provided by embedders;  allowed to be missing
    'js-config.h',              # generated in $OBJDIR
    'pratom.h',                 # NSPR
    'prcvar.h',                 # NSPR
    'prinit.h',                 # NSPR
    'prlink.h',                 # NSPR
    'prlock.h',                 # NSPR
    'prprf.h',                  # NSPR
    'prthread.h',               # NSPR
    'prtypes.h',                # NSPR
    'selfhosted.out.h',         # generated in $OBJDIR
    'unicode/locid.h',          # ICU
    'unicode/numsys.h',         # ICU
    'unicode/ucal.h',           # ICU
    'unicode/uclean.h',         # ICU
    'unicode/ucol.h',           # ICU
    'unicode/udat.h',           # ICU
    'unicode/udatpg.h',         # ICU
    'unicode/uenum.h',          # ICU
    'unicode/unum.h',           # ICU
    'unicode/ustring.h',        # ICU
    'unicode/utypes.h',         # ICU
    'vtune/VTuneWrapper.h'      # VTune
])

# The files in tests/style/ contain code that fails this checking in various
# ways.  Here is the output we expect.  If the actual output differs from
# this, one of the following must have happened.
# - New SpiderMonkey code violates one of the checked rules.
# - The tests/style/ files have changed without expected_output being changed
#   accordingly.
# - This script has been broken somehow.
#
expected_output = '''\
js/src/tests/style/BadIncludes2.h:1: error:
    vanilla header includes an inline-header file "tests/style/BadIncludes2-inl.h"

js/src/tests/style/BadIncludes.h:1: error:
    the file includes itself

js/src/tests/style/BadIncludes.h:3: error:
    "BadIncludes2.h" is included using the wrong path;
    did you forget a prefix, or is the file not yet committed?

js/src/tests/style/BadIncludes.h:4: error:
    <tests/style/BadIncludes2.h> should be included using
    the #include "..." form

js/src/tests/style/BadIncludes.h:5: error:
    "stdio.h" is included using the wrong path;
    did you forget a prefix, or is the file not yet committed?

(multiple files): error:
    header files form one or more cycles

   tests/style/HeaderCycleA1.h
   -> tests/style/HeaderCycleA2.h
      -> tests/style/HeaderCycleA3.h
         -> tests/style/HeaderCycleA1.h

   tests/style/HeaderCycleB1-inl.h
   -> tests/style/HeaderCycleB2-inl.h
      -> tests/style/HeaderCycleB3-inl.h
         -> tests/style/HeaderCycleB4-inl.h
            -> tests/style/HeaderCycleB1-inl.h
            -> tests/style/jsheadercycleB5inlines.h
               -> tests/style/HeaderCycleB1-inl.h
      -> tests/style/HeaderCycleB4-inl.h

'''.splitlines(True)

actual_output = []


def out(*lines):
    for line in lines:
        actual_output.append(line + '\n')


def error(filename, linenum, *lines):
    location = filename
    if linenum != None:
        location += ":" + str(linenum)
    out(location + ': error:')
    for line in (lines):
        out('    ' + line)
    out('')


class FileKind(object):
    C = 1
    CPP = 2
    INL_H = 3
    H = 4
    TBL = 5
    MSG = 6

    @staticmethod
    def get(filename):
        if filename.endswith('.c'):
            return FileKind.C
       
        if filename.endswith('.cpp'):
            return FileKind.CPP
       
        if filename.endswith(('inlines.h', '-inl.h')):
            return FileKind.INL_H

        if filename.endswith('.h'):
            return FileKind.H

        if filename.endswith('.tbl'):
            return FileKind.TBL

        if filename.endswith('.msg'):
            return FileKind.MSG

        error(filename, None, 'unknown file kind')


def get_all_filenames():
    """Get a list of all the files in the (Mercurial or Git) repository."""
    cmds = [['hg', 'manifest', '-q'], ['git', 'ls-files']]
    for cmd in cmds:
        try:
            all_filenames = subprocess.check_output(cmd, universal_newlines=True,
                                                    stderr=subprocess.PIPE).split('\n')
            return all_filenames
        except:
            continue
    else:
        raise Exception('failed to run any of the repo manifest commands', cmds)


def check_style():
    # We deal with two kinds of name.
    # - A "filename" is a full path to a file from the repository root.
    # - An "inclname" is how a file is referred to in a #include statement.
    #
    # Examples (filename -> inclname)
    # - "mfbt/Attributes.h"  -> "mozilla/Attributes.h"
    # - "js/public/Vector.h" -> "js/Vector.h"
    # - "js/src/vm/String.h" -> "vm/String.h"

    mfbt_inclnames = set()      # type: set(inclname)
    js_names = dict()           # type: dict(filename, inclname)

    # Select the appropriate files.
    for filename in get_all_filenames():
        if filename.startswith('mfbt/') and filename.endswith('.h'):
            inclname = 'mozilla/' + filename[len('mfbt/'):]
            mfbt_inclnames.add(inclname)

        if filename.startswith('js/public/') and filename.endswith('.h'):
            inclname = 'js/' + filename[len('js/public/'):]
            js_names[filename] = inclname

        if filename.startswith('js/src/') and \
           not filename.startswith(tuple(ignored_js_src_dirs)) and \
           filename.endswith(('.c', '.cpp', '.h', '.tbl', '.msg')):
            inclname = filename[len('js/src/'):]
            js_names[filename] = inclname

    all_inclnames = mfbt_inclnames | set(js_names.values())

    edges = dict()      # type: dict(inclname, set(inclname))

    # We don't care what's inside the MFBT files, but because they are
    # #included from JS files we have to add them to the inclusion graph.
    for inclname in mfbt_inclnames:
        edges[inclname] = set()

    # Process all the JS files.
    for filename in js_names.keys():
        inclname = js_names[filename]
        file_kind = FileKind.get(filename)
        if file_kind == FileKind.C or file_kind == FileKind.CPP or \
           file_kind == FileKind.H or file_kind == FileKind.INL_H:
            included_h_inclnames = set()    # type: set(inclname)

            # This script is run in js/src/, so prepend '../../' to get to the root of the Mozilla
            # source tree.
            with open(os.path.join('../..', filename)) as f:
                do_file(filename, inclname, file_kind, f, all_inclnames, included_h_inclnames)

        edges[inclname] = included_h_inclnames

    find_cycles(all_inclnames, edges)

    # Compare expected and actual output.
    difflines = difflib.unified_diff(expected_output, actual_output,
                                     fromfile='check_spider_monkey_style.py expected output',
                                       tofile='check_spider_monkey_style.py actual output')
    ok = True
    for diffline in difflines:
        ok = False
        print(diffline, end='')

    return ok


def do_file(filename, inclname, file_kind, f, all_inclnames, included_h_inclnames):
    for linenum, line in enumerate(f, start=1):
        # Look for a |#include "..."| line.
        m = re.match(r'\s*#\s*include\s+"([^"]*)"', line)
        if m is not None:
            included_inclname = m.group(1)

            if included_inclname not in included_inclnames_to_ignore:
                included_kind = FileKind.get(included_inclname)

                # Check the #include path has the correct form.
                if included_inclname not in all_inclnames:
                    error(filename, linenum,
                          '"' + included_inclname + '" is included ' + 'using the wrong path;',
                          'did you forget a prefix, or is the file not yet committed?')

                # Record inclusions of .h files for cycle detection later.
                # (Exclude .tbl and .msg files.)
                elif included_kind == FileKind.H or included_kind == FileKind.INL_H:
                    included_h_inclnames.add(included_inclname)

                # Check a H file doesn't #include an INL_H file.
                if file_kind == FileKind.H and included_kind == FileKind.INL_H:
                    error(filename, linenum,
                          'vanilla header includes an inline-header file "' + included_inclname + '"')

                # Check a file doesn't #include itself.  (We do this here because the
                # cycle detection below doesn't detect this case.)
                if inclname == included_inclname:
                    error(filename, linenum, 'the file includes itself')

        # Look for a |#include <...>| line.
        m = re.match(r'\s*#\s*include\s+<([^>]*)>', line)
        if m is not None:
            included_inclname = m.group(1)

            # Check it is not a known local file (in which case it's
            # probably a system header).
            if included_inclname in included_inclnames_to_ignore or \
               included_inclname in all_inclnames:
                error(filename, linenum,
                      '<' + included_inclname + '> should be included using',
                      'the #include "..." form')


def find_cycles(all_inclnames, edges):
    """Find and draw any cycles."""
    
    SCCs = tarjan(all_inclnames, edges)

    # The various sorted() calls below ensure the output is deterministic.

    def draw_SCC(c):
        cset = set(c)
        drawn = set()
        def draw(v, indent):
            out('   ' * indent + ('-> ' if indent else '   ') + v)
            if v in drawn:
                return
            drawn.add(v)
            for succ in sorted(edges[v]):
                if succ in cset:
                    draw(succ, indent + 1)
        draw(sorted(c)[0], 0)
        out('')

    have_drawn_an_SCC = False
    for scc in sorted(SCCs):
        if len(scc) != 1:
            if not have_drawn_an_SCC:
                error('(multiple files)', None, 'header files form one or more cycles')
                have_drawn_an_SCC = True

            draw_SCC(scc)


# Tarjan's algorithm for finding the strongly connected components (SCCs) of a graph.
# https://en.wikipedia.org/wiki/Tarjan%27s_strongly_connected_components_algorithm
def tarjan(V, E):
    vertex_index = {}
    vertex_lowlink = {}
    index = 0
    S = []
    all_SCCs = []

    def strongconnect(v, index):
        # Set the depth index for v to the smallest unused index
        vertex_index[v] = index
        vertex_lowlink[v] = index
        index += 1
        S.append(v)

        # Consider successors of v
        for w in E[v]:
            if w not in vertex_index:
                # Successor w has not yet been visited; recurse on it
                index = strongconnect(w, index)
                vertex_lowlink[v] = min(vertex_lowlink[v], vertex_lowlink[w])
            elif w in S:
                # Successor w is in stack S and hence in the current SCC
                vertex_lowlink[v] = min(vertex_lowlink[v], vertex_index[w])

        # If v is a root node, pop the stack and generate an SCC
        if vertex_lowlink[v] == vertex_index[v]:
            i = S.index(v)
            scc = S[i:]
            del S[i:]
            all_SCCs.append(scc)

        return index

    for v in V:
        if v not in vertex_index:
            index = strongconnect(v, index)

    return all_SCCs


def main():
    ok = check_style()

    if ok:
        print('TEST-PASS | check_spidermonkey_style.py | ok')
    else:
        print('TEST-UNEXPECTED-FAIL | check_spidermonkey_style.py | actual output does not match expected output;  diff is above')

    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
