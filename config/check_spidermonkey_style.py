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
#   - The guard name used should be appropriate for the filename.
#
# We check the following things in all files.
#
# - #includes should have full paths, e.g. "jit/Ion.h", not "Ion.h".
#
# - #includes should use the appropriate form for system headers (<...>) and
#   local headers ("...").
#
# - #includes should be ordered correctly.
#   - Each one should be in the correct section.
#   - Alphabetical order should be used within sections.
#   - Sections should be in the right order.
#   Note that the presence of #if/#endif blocks complicates things, to the
#   point that it's not always clear where a conditionally-compiled #include
#   statement should go, even to a human.  Therefore, we check the #include
#   statements within each #if/#endif block (including nested ones) in
#   isolation, but don't try to do any order checking between such blocks.
#----------------------------------------------------------------------------

from __future__ import print_function

import difflib
import os
import re
import subprocess
import sys
import traceback
from check_utils import get_all_toplevel_filenames

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
    'jscustomallocator.h',      # provided by embedders;  allowed to be missing
    'js-config.h',              # generated in $OBJDIR
    'pratom.h',                 # NSPR
    'prcvar.h',                 # NSPR
    'prerror.h',                # NSPR
    'prinit.h',                 # NSPR
    'prlink.h',                 # NSPR
    'prlock.h',                 # NSPR
    'prprf.h',                  # NSPR
    'prthread.h',               # NSPR
    'prtypes.h',                # NSPR
    'selfhosted.out.h',         # generated in $OBJDIR
    'shellmoduleloader.out.h',  # generated in $OBJDIR
    'unicode/locid.h',          # ICU
    'unicode/numsys.h',         # ICU
    'unicode/timezone.h',       # ICU
    'unicode/ucal.h',           # ICU
    'unicode/uclean.h',         # ICU
    'unicode/ucol.h',           # ICU
    'unicode/udat.h',           # ICU
    'unicode/udatpg.h',         # ICU
    'unicode/uenum.h',          # ICU
    'unicode/unorm.h',          # ICU
    'unicode/unum.h',           # ICU
    'unicode/ustring.h',        # ICU
    'unicode/utypes.h',         # ICU
    'vtune/VTuneWrapper.h'      # VTune
])

# These files have additional constraints on where they are #included, so we
# ignore #includes of them when checking #include ordering.
oddly_ordered_inclnames = set([
    'ctypes/typedefs.h',        # Included multiple times in the body of ctypes/CTypes.h
    'jsautokw.h',               # Included in the body of frontend/TokenStream.h
    'jswin.h',                  # Must be #included before <psapi.h>
    'machine/endian.h',         # Must be included after <sys/types.h> on BSD
    'winbase.h',                # Must precede other system headers(?)
    'windef.h'                  # Must precede other system headers(?)
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

js/src/tests/style/BadIncludes.h:3: error:
    the file includes itself

js/src/tests/style/BadIncludes.h:6: error:
    "BadIncludes2.h" is included using the wrong path;
    did you forget a prefix, or is the file not yet committed?

js/src/tests/style/BadIncludes.h:8: error:
    <tests/style/BadIncludes2.h> should be included using
    the #include "..." form

js/src/tests/style/BadIncludes.h:10: error:
    "stdio.h" is included using the wrong path;
    did you forget a prefix, or is the file not yet committed?

js/src/tests/style/BadIncludesOrder-inl.h:5:6: error:
    "vm/Interpreter-inl.h" should be included after "jsscriptinlines.h"

js/src/tests/style/BadIncludesOrder-inl.h:6:7: error:
    "jsscriptinlines.h" should be included after "js/Value.h"

js/src/tests/style/BadIncludesOrder-inl.h:7:8: error:
    "js/Value.h" should be included after "ds/LifoAlloc.h"

js/src/tests/style/BadIncludesOrder-inl.h:8:9: error:
    "ds/LifoAlloc.h" should be included after "jsapi.h"

js/src/tests/style/BadIncludesOrder-inl.h:9:10: error:
    "jsapi.h" should be included after <stdio.h>

js/src/tests/style/BadIncludesOrder-inl.h:10:11: error:
    <stdio.h> should be included after "mozilla/HashFunctions.h"

js/src/tests/style/BadIncludesOrder-inl.h:27:28: error:
    "jsobj.h" should be included after "jsfun.h"

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
    if linenum is not None:
        location += ':' + str(linenum)
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


def check_style():
    # We deal with two kinds of name.
    # - A "filename" is a full path to a file from the repository root.
    # - An "inclname" is how a file is referred to in a #include statement.
    #
    # Examples (filename -> inclname)
    # - "mfbt/Attributes.h"     -> "mozilla/Attributes.h"
    # - "mfbt/decimal/Decimal.h -> "mozilla/Decimal.h"
    # - "js/public/Vector.h"    -> "js/Vector.h"
    # - "js/src/vm/String.h"    -> "vm/String.h"

    mfbt_inclnames = set()      # type: set(inclname)
    mozalloc_inclnames = set()  # type: set(inclname)
    js_names = dict()           # type: dict(filename, inclname)

    # Select the appropriate files.
    for filename in get_all_toplevel_filenames():
        if filename.startswith('mfbt/') and filename.endswith('.h'):
            inclname = 'mozilla/' + filename.split('/')[-1]
            mfbt_inclnames.add(inclname)

        if filename.startswith('memory/mozalloc/') and filename.endswith('.h'):
            inclname = 'mozilla/' + filename.split('/')[-1]
            mozalloc_inclnames.add(inclname)

        if filename.startswith('js/public/') and filename.endswith('.h'):
            inclname = 'js/' + filename[len('js/public/'):]
            js_names[filename] = inclname

        if filename.startswith('js/src/') and \
           not filename.startswith(tuple(ignored_js_src_dirs)) and \
           filename.endswith(('.c', '.cpp', '.h', '.tbl', '.msg')):
            inclname = filename[len('js/src/'):]
            js_names[filename] = inclname

    all_inclnames = mfbt_inclnames | mozalloc_inclnames | set(js_names.values())

    edges = dict()      # type: dict(inclname, set(inclname))

    # We don't care what's inside the MFBT and MOZALLOC files, but because they
    # are #included from JS files we have to add them to the inclusion graph.
    for inclname in mfbt_inclnames | mozalloc_inclnames:
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


def module_name(name):
    '''Strip the trailing .cpp, .h, inlines.h or -inl.h from a filename.'''

    return name.replace('inlines.h', '').replace('-inl.h', '').replace('.h', '').replace('.cpp', '')


def is_module_header(enclosing_inclname, header_inclname):
    '''Determine if an included name is the "module header", i.e. should be
    first in the file.'''

    module = module_name(enclosing_inclname)

    # Normal case, e.g. module == "foo/Bar", header_inclname == "foo/Bar.h".
    if module == module_name(header_inclname):
        return True

    # A public header, e.g. module == "foo/Bar", header_inclname == "js/Bar.h".
    m = re.match(r'js\/(.*)\.h', header_inclname)
    if m is not None and module.endswith('/' + m.group(1)):
        return True

    return False


class Include(object):
    '''Important information for a single #include statement.'''

    def __init__(self, inclname, linenum, is_system):
        self.inclname = inclname
        self.linenum = linenum
        self.is_system = is_system

    def isLeaf(self):
        return True

    def section(self, enclosing_inclname):
        '''Identify which section inclname belongs to.

        The section numbers are as follows.
          0. Module header (e.g. jsfoo.h or jsfooinlines.h within jsfoo.cpp)
          1. mozilla/Foo.h
          2. <foo.h> or <foo>
          3. jsfoo.h, prmjtime.h, etc
          4. foo/Bar.h
          5. jsfooinlines.h
          6. foo/Bar-inl.h
          7. non-.h, e.g. *.tbl, *.msg
        '''

        if self.is_system:
            return 2

        if not self.inclname.endswith('.h'):
            return 7

        # A couple of modules have the .h file in js/ and the .cpp file elsewhere and so need
        # special handling.
        if is_module_header(enclosing_inclname, self.inclname):
            return 0

        if '/' in self.inclname:
            if self.inclname.startswith('mozilla/'):
                return 1

            if self.inclname.endswith('-inl.h'):
                return 6

            return 4

        if self.inclname.endswith('inlines.h'):
            return 5

        return 3

    def quote(self):
        if self.is_system:
            return '<' + self.inclname + '>'
        else:
            return '"' + self.inclname + '"'


class HashIfBlock(object):
    '''Important information about a #if/#endif block.

    A #if/#endif block is the contents of a #if/#endif (or similar) section.
    The top-level block, which is not within a #if/#endif pair, is also
    considered a block.

    Each leaf is either an Include (representing a #include), or another
    nested HashIfBlock.'''
    def __init__(self):
        self.kids = []

    def isLeaf(self):
        return False


def do_file(filename, inclname, file_kind, f, all_inclnames, included_h_inclnames):
    block_stack = [HashIfBlock()]

    # Extract the #include statements as a tree of IBlocks and IIncludes.
    for linenum, line in enumerate(f, start=1):
        # We're only interested in lines that contain a '#'.
        if not '#' in line:
            continue

        # Look for a |#include "..."| line.
        m = re.match(r'\s*#\s*include\s+"([^"]*)"', line)
        if m is not None:
            block_stack[-1].kids.append(Include(m.group(1), linenum, False))

        # Look for a |#include <...>| line.
        m = re.match(r'\s*#\s*include\s+<([^>]*)>', line)
        if m is not None:
            block_stack[-1].kids.append(Include(m.group(1), linenum, True))

        # Look for a |#{if,ifdef,ifndef}| line.
        m = re.match(r'\s*#\s*(if|ifdef|ifndef)\b', line)
        if m is not None:
            # Open a new block.
            new_block = HashIfBlock()
            block_stack[-1].kids.append(new_block)
            block_stack.append(new_block)

        # Look for a |#{elif,else}| line.
        m = re.match(r'\s*#\s*(elif|else)\b', line)
        if m is not None:
            # Close the current block, and open an adjacent one.
            block_stack.pop()
            new_block = HashIfBlock()
            block_stack[-1].kids.append(new_block)
            block_stack.append(new_block)

        # Look for a |#endif| line.
        m = re.match(r'\s*#\s*endif\b', line)
        if m is not None:
            # Close the current block.
            block_stack.pop()

    def check_include_statement(include):
        '''Check the style of a single #include statement.'''

        if include.is_system:
            # Check it is not a known local file (in which case it's probably a system header).
            if include.inclname in included_inclnames_to_ignore or \
               include.inclname in all_inclnames:
                error(filename, include.linenum,
                      include.quote() + ' should be included using',
                      'the #include "..." form')

        else:
            if include.inclname not in included_inclnames_to_ignore:
                included_kind = FileKind.get(include.inclname)

                # Check the #include path has the correct form.
                if include.inclname not in all_inclnames:
                    error(filename, include.linenum,
                          include.quote() + ' is included using the wrong path;',
                          'did you forget a prefix, or is the file not yet committed?')

                # Record inclusions of .h files for cycle detection later.
                # (Exclude .tbl and .msg files.)
                elif included_kind == FileKind.H or included_kind == FileKind.INL_H:
                    included_h_inclnames.add(include.inclname)

                # Check a H file doesn't #include an INL_H file.
                if file_kind == FileKind.H and included_kind == FileKind.INL_H:
                    error(filename, include.linenum,
                          'vanilla header includes an inline-header file ' + include.quote())

                # Check a file doesn't #include itself.  (We do this here because the cycle
                # detection below doesn't detect this case.)
                if inclname == include.inclname:
                    error(filename, include.linenum, 'the file includes itself')

    def check_includes_order(include1, include2):
        '''Check the ordering of two #include statements.'''

        if include1.inclname in oddly_ordered_inclnames or \
           include2.inclname in oddly_ordered_inclnames:
            return

        section1 = include1.section(inclname)
        section2 = include2.section(inclname)
        if (section1 > section2) or \
           ((section1 == section2) and (include1.inclname.lower() > include2.inclname.lower())):
            error(filename, str(include1.linenum) + ':' + str(include2.linenum),
                  include1.quote() + ' should be included after ' + include2.quote())

    # Check the extracted #include statements, both individually, and the ordering of
    # adjacent pairs that live in the same block.
    def pair_traverse(prev, this):
        if this.isLeaf():
            check_include_statement(this)
            if prev is not None and prev.isLeaf():
                check_includes_order(prev, this)
        else:
            for prev2, this2 in zip([None] + this.kids[0:-1], this.kids):
                pair_traverse(prev2, this2)

    pair_traverse(None, block_stack[-1])


def find_cycles(all_inclnames, edges):
    '''Find and draw any cycles.'''

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


if __name__ == '__main__':
    main()
