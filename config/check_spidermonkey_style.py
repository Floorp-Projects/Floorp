# vim: set ts=8 sts=4 et sw=4 tw=99:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# ----------------------------------------------------------------------------
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
# ----------------------------------------------------------------------------

from __future__ import absolute_import, print_function

import difflib
import os
import re
import sys

# We don't bother checking files in these directories, because they're (a) auxiliary or (b)
# imported code that doesn't follow our coding style.
ignored_js_src_dirs = [
    "js/src/config/",  # auxiliary stuff
    "js/src/ctypes/libffi/",  # imported code
    "js/src/devtools/",  # auxiliary stuff
    "js/src/editline/",  # imported code
    "js/src/gdb/",  # auxiliary stuff
    "js/src/vtune/",  # imported code
    "js/src/zydis/",  # imported code
]

# We ignore #includes of these files, because they don't follow the usual rules.
included_inclnames_to_ignore = set(
    [
        "ffi.h",  # generated in ctypes/libffi/
        "devtools/Instruments.h",  # we ignore devtools/ in general
        "double-conversion/double-conversion.h",  # strange MFBT case
        "javascript-trace.h",  # generated in $OBJDIR if HAVE_DTRACE is defined
        "frontend/ReservedWordsGenerated.h",  # generated in $OBJDIR
        "frontend/smoosh_generated.h",  # generated in $OBJDIR
        "gc/StatsPhasesGenerated.h",  # generated in $OBJDIR
        "gc/StatsPhasesGenerated.inc",  # generated in $OBJDIR
        "jit/AtomicOperationsGenerated.h",  # generated in $OBJDIR
        "jit/CacheIROpsGenerated.h",  # generated in $OBJDIR
        "jit/LIROpsGenerated.h",  # generated in $OBJDIR
        "jit/MIROpsGenerated.h",  # generated in $OBJDIR
        "js/ProfilingCategoryList.h",  # comes from mozglue/baseprofiler
        "jscustomallocator.h",  # provided by embedders;  allowed to be missing
        "js-config.h",  # generated in $OBJDIR
        "fdlibm.h",  # fdlibm
        "FuzzerDefs.h",  # included without a path
        "FuzzingInterface.h",  # included without a path
        "mozmemory.h",  # included without a path
        "pratom.h",  # NSPR
        "prcvar.h",  # NSPR
        "prerror.h",  # NSPR
        "prinit.h",  # NSPR
        "prio.h",  # NSPR
        "private/pprio.h",  # NSPR
        "prlink.h",  # NSPR
        "prlock.h",  # NSPR
        "prprf.h",  # NSPR
        "prthread.h",  # NSPR
        "prtypes.h",  # NSPR
        "selfhosted.out.h",  # generated in $OBJDIR
        "shellmoduleloader.out.h",  # generated in $OBJDIR
        "unicode/locid.h",  # ICU
        "unicode/uchar.h",  # ICU
        "unicode/uniset.h",  # ICU
        "unicode/unistr.h",  # ICU
        "unicode/utypes.h",  # ICU
        "vtune/VTuneWrapper.h",  # VTune
        "wasm/WasmIntrinsicGenerated.h",  # generated in $OBJDIR"
        "zydis/ZydisAPI.h",  # Zydis
    ]
)

deprecated_inclnames = {
    "mozilla/Unused.h": "Use [[nodiscard]] and (void)expr casts instead.",
}

# JSAPI functions should be included through headers from js/public instead of
# using the old, catch-all jsapi.h file.
deprecated_inclnames_in_header = {
    "jsapi.h": "Prefer including headers from js/public.",
}

# Temporary exclusions for files which still need to include jsapi.h.
deprecated_inclnames_in_header_excludes = {
    "js/src/vm/Compartment-inl.h",  # for JS::InformalValueTypeName
    "js/src/jsapi-tests/tests.h",  # for JS_ValueToSource
}

# These files have additional constraints on where they are #included, so we
# ignore #includes of them when checking #include ordering.
oddly_ordered_inclnames = set(
    [
        "ctypes/typedefs.h",  # Included multiple times in the body of ctypes/CTypes.h
        # Included in the body of frontend/TokenStream.h
        "frontend/ReservedWordsGenerated.h",
        "gc/StatsPhasesGenerated.h",  # Included in the body of gc/Statistics.h
        "gc/StatsPhasesGenerated.inc",  # Included in the body of gc/Statistics.cpp
        "psapi.h",  # Must be included after "util/WindowsWrapper.h" on Windows
        "machine/endian.h",  # Must be included after <sys/types.h> on BSD
        "winbase.h",  # Must precede other system headers(?)
        "windef.h",  # Must precede other system headers(?)
    ]
)

# The files in tests/style/ contain code that fails this checking in various
# ways.  Here is the output we expect.  If the actual output differs from
# this, one of the following must have happened.
# - New SpiderMonkey code violates one of the checked rules.
# - The tests/style/ files have changed without expected_output being changed
#   accordingly.
# - This script has been broken somehow.
#
expected_output = """\
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

js/src/tests/style/BadIncludes.h:12: error:
    "mozilla/Unused.h" is deprecated: Use [[nodiscard]] and (void)expr casts instead.

js/src/tests/style/BadIncludes2.h:1: error:
    vanilla header includes an inline-header file "tests/style/BadIncludes2-inl.h"

js/src/tests/style/BadIncludesOrder-inl.h:5:6: error:
    "vm/JSScript-inl.h" should be included after "vm/Interpreter-inl.h"

js/src/tests/style/BadIncludesOrder-inl.h:6:7: error:
    "vm/Interpreter-inl.h" should be included after "js/Value.h"

js/src/tests/style/BadIncludesOrder-inl.h:7:8: error:
    "js/Value.h" should be included after "ds/LifoAlloc.h"

js/src/tests/style/BadIncludesOrder-inl.h:9: error:
    "jsapi.h" is deprecated: Prefer including headers from js/public.

js/src/tests/style/BadIncludesOrder-inl.h:8:9: error:
    "ds/LifoAlloc.h" should be included after "jsapi.h"

js/src/tests/style/BadIncludesOrder-inl.h:9:10: error:
    "jsapi.h" should be included after <stdio.h>

js/src/tests/style/BadIncludesOrder-inl.h:10:11: error:
    <stdio.h> should be included after "mozilla/HashFunctions.h"

js/src/tests/style/BadIncludesOrder-inl.h:20: error:
    "jsapi.h" is deprecated: Prefer including headers from js/public.

js/src/tests/style/BadIncludesOrder-inl.h:28:29: error:
    "vm/JSScript.h" should be included after "vm/JSFunction.h"

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

""".splitlines(
    True
)

actual_output = []


def out(*lines):
    for line in lines:
        actual_output.append(line + "\n")


def error(filename, linenum, *lines):
    location = filename
    if linenum is not None:
        location += ":" + str(linenum)
    out(location + ": error:")
    for line in lines:
        out("    " + line)
    out("")


class FileKind(object):
    C = 1
    CPP = 2
    INL_H = 3
    H = 4
    TBL = 5
    MSG = 6

    @staticmethod
    def get(filename):
        if filename.endswith(".c"):
            return FileKind.C

        if filename.endswith(".cpp"):
            return FileKind.CPP

        if filename.endswith(("inlines.h", "-inl.h")):
            return FileKind.INL_H

        if filename.endswith(".h"):
            return FileKind.H

        if filename.endswith(".tbl"):
            return FileKind.TBL

        if filename.endswith(".msg"):
            return FileKind.MSG

        error(filename, None, "unknown file kind")


def check_style(enable_fixup):
    # We deal with two kinds of name.
    # - A "filename" is a full path to a file from the repository root.
    # - An "inclname" is how a file is referred to in a #include statement.
    #
    # Examples (filename -> inclname)
    # - "mfbt/Attributes.h"         -> "mozilla/Attributes.h"
    # - "mozglue/misc/TimeStamp.h   -> "mozilla/TimeStamp.h"
    # - "memory/mozalloc/mozalloc.h -> "mozilla/mozalloc.h"
    # - "js/public/Vector.h"        -> "js/Vector.h"
    # - "js/src/vm/String.h"        -> "vm/String.h"

    non_js_dirnames = (
        "mfbt/",
        "memory/mozalloc/",
        "mozglue/",
        "intl/components/",
    )  # type: tuple(str)
    non_js_inclnames = set()  # type: set(inclname)
    js_names = dict()  # type: dict(filename, inclname)

    # Process files in js/src.
    js_src_root = os.path.join("js", "src")
    for dirpath, dirnames, filenames in os.walk(js_src_root):
        if dirpath == js_src_root:
            # Skip any subdirectories that contain a config.status file
            # (likely objdirs).
            builddirs = []
            for dirname in dirnames:
                path = os.path.join(dirpath, dirname, "config.status")
                if os.path.isfile(path):
                    builddirs.append(dirname)
            for dirname in builddirs:
                dirnames.remove(dirname)
        for filename in filenames:
            filepath = os.path.join(dirpath, filename).replace("\\", "/")
            if not filepath.startswith(
                tuple(ignored_js_src_dirs)
            ) and filepath.endswith((".c", ".cpp", ".h", ".tbl", ".msg")):
                inclname = filepath[len("js/src/") :]
                js_names[filepath] = inclname

    # Look for header files in directories in non_js_dirnames.
    for non_js_dir in non_js_dirnames:
        for dirpath, dirnames, filenames in os.walk(non_js_dir):
            for filename in filenames:
                if filename.endswith(".h"):
                    inclname = "mozilla/" + filename
                    if non_js_dir == "intl/components/":
                        inclname = "mozilla/intl/" + filename
                    non_js_inclnames.add(inclname)

    # Look for header files in js/public.
    js_public_root = os.path.join("js", "public")
    for dirpath, dirnames, filenames in os.walk(js_public_root):
        for filename in filenames:
            if filename.endswith((".h", ".msg")):
                filepath = os.path.join(dirpath, filename).replace("\\", "/")
                inclname = "js/" + filepath[len("js/public/") :]
                js_names[filepath] = inclname

    all_inclnames = non_js_inclnames | set(js_names.values())

    edges = dict()  # type: dict(inclname, set(inclname))

    # We don't care what's inside the MFBT and MOZALLOC files, but because they
    # are #included from JS files we have to add them to the inclusion graph.
    for inclname in non_js_inclnames:
        edges[inclname] = set()

    # Process all the JS files.
    for filename in sorted(js_names.keys()):
        inclname = js_names[filename]
        file_kind = FileKind.get(filename)
        if (
            file_kind == FileKind.C
            or file_kind == FileKind.CPP
            or file_kind == FileKind.H
            or file_kind == FileKind.INL_H
        ):
            included_h_inclnames = set()  # type: set(inclname)

            with open(filename, encoding="utf-8") as f:
                code = read_file(f)

            if enable_fixup:
                code = code.sorted(inclname)
                with open(filename, "w") as f:
                    f.write(code.to_source())

            check_file(
                filename, inclname, file_kind, code, all_inclnames, included_h_inclnames
            )

        edges[inclname] = included_h_inclnames

    find_cycles(all_inclnames, edges)

    # Compare expected and actual output.
    difflines = difflib.unified_diff(
        expected_output,
        actual_output,
        fromfile="check_spidermonkey_style.py expected output",
        tofile="check_spidermonkey_style.py actual output",
    )
    ok = True
    for diffline in difflines:
        ok = False
        print(diffline, end="")

    return ok


def module_name(name):
    """Strip the trailing .cpp, .h, inlines.h or -inl.h from a filename."""

    return (
        name.replace("inlines.h", "")
        .replace("-inl.h", "")
        .replace(".h", "")
        .replace(".cpp", "")
    )  # NOQA: E501


def is_module_header(enclosing_inclname, header_inclname):
    """Determine if an included name is the "module header", i.e. should be
    first in the file."""

    module = module_name(enclosing_inclname)

    # Normal case, for example:
    #   module == "vm/Runtime", header_inclname == "vm/Runtime.h".
    if module == module_name(header_inclname):
        return True

    # A public header, for example:
    #
    #   module == "vm/CharacterEncoding",
    #   header_inclname == "js/CharacterEncoding.h"
    #
    # or (for implementation files for js/public/*/*.h headers)
    #
    #   module == "vm/SourceHook",
    #   header_inclname == "js/experimental/SourceHook.h"
    m = re.match(r"js\/.*?([^\/]+)\.h", header_inclname)
    if m is not None and module.endswith("/" + m.group(1)):
        return True

    return False


class Include(object):
    """Important information for a single #include statement."""

    def __init__(self, include_prefix, inclname, line_suffix, linenum, is_system):
        self.include_prefix = include_prefix
        self.line_suffix = line_suffix
        self.inclname = inclname
        self.linenum = linenum
        self.is_system = is_system

    def is_style_relevant(self):
        # Includes are style-relevant; that is, they're checked by the pairwise
        # style-checking algorithm in check_file.
        return True

    def section(self, enclosing_inclname):
        """Identify which section inclname belongs to.

        The section numbers are as follows.
          0. Module header (e.g. jsfoo.h or jsfooinlines.h within jsfoo.cpp)
          1. mozilla/Foo.h
          2. <foo.h> or <foo>
          3. jsfoo.h, prmjtime.h, etc
          4. foo/Bar.h
          5. jsfooinlines.h
          6. foo/Bar-inl.h
          7. non-.h, e.g. *.tbl, *.msg (these can be scattered throughout files)
        """

        if self.is_system:
            return 2

        if not self.inclname.endswith(".h"):
            return 7

        # A couple of modules have the .h file in js/ and the .cpp file elsewhere and so need
        # special handling.
        if is_module_header(enclosing_inclname, self.inclname):
            return 0

        if "/" in self.inclname:
            if self.inclname.startswith("mozilla/"):
                return 1

            if self.inclname.endswith("-inl.h"):
                return 6

            return 4

        if self.inclname.endswith("inlines.h"):
            return 5

        return 3

    def quote(self):
        if self.is_system:
            return "<" + self.inclname + ">"
        else:
            return '"' + self.inclname + '"'

    def sort_key(self, enclosing_inclname):
        return (self.section(enclosing_inclname), self.inclname.lower())

    def to_source(self):
        return self.include_prefix + self.quote() + self.line_suffix + "\n"


class CppBlock(object):
    """C preprocessor block: a whole file or a single #if/#elif/#else block.

    A #if/#endif block is the contents of a #if/#endif (or similar) section.
    The top-level block, which is not within a #if/#endif pair, is also
    considered a block.

    Each kid is either an Include (representing a #include), OrdinaryCode, or
    a nested CppBlock."""

    def __init__(self, start_line=""):
        self.start = start_line
        self.end = ""
        self.kids = []

    def is_style_relevant(self):
        return True

    def append_ordinary_line(self, line):
        if len(self.kids) == 0 or not isinstance(self.kids[-1], OrdinaryCode):
            self.kids.append(OrdinaryCode())
        self.kids[-1].lines.append(line)

    def style_relevant_kids(self):
        """Return a list of kids in this block that are style-relevant."""
        return [kid for kid in self.kids if kid.is_style_relevant()]

    def sorted(self, enclosing_inclname):
        """Return a hopefully-sorted copy of this block. Implements --fixup.

        When in doubt, this leaves the code unchanged.
        """

        def pretty_sorted_includes(includes):
            """Return a new list containing the given includes, in order,
            with blank lines separating sections."""
            keys = [inc.sort_key(enclosing_inclname) for inc in includes]
            if sorted(keys) == keys:
                return includes  # if nothing is out of order, don't touch anything

            output = []
            current_section = None
            for (section, _), inc in sorted(zip(keys, includes)):
                if current_section is not None and section != current_section:
                    output.append(OrdinaryCode(["\n"]))  # blank line
                output.append(inc)
                current_section = section
            return output

        def should_try_to_sort(includes):
            if "tests/style/BadIncludes" in enclosing_inclname:
                return False  # don't straighten the counterexample
            if any(inc.inclname in oddly_ordered_inclnames for inc in includes):
                return False  # don't sort batches containing odd includes
            if includes == sorted(
                includes, key=lambda inc: inc.sort_key(enclosing_inclname)
            ):
                return False  # it's already sorted, avoid whitespace-only fixups
            return True

        # The content of the eventual output of this method.
        output = []

        # The current batch of includes to sort. This list only ever contains Include objects
        # and whitespace OrdinaryCode objects.
        batch = []

        def flush_batch():
            """Sort the contents of `batch` and move it to `output`."""

            assert all(
                isinstance(item, Include)
                or (isinstance(item, OrdinaryCode) and "".join(item.lines).isspace())
                for item in batch
            )

            # Here we throw away the blank lines.
            # `pretty_sorted_includes` puts them back.
            includes = []
            last_include_index = -1
            for i, item in enumerate(batch):
                if isinstance(item, Include):
                    includes.append(item)
                    last_include_index = i
            cutoff = last_include_index + 1

            if should_try_to_sort(includes):
                output.extend(pretty_sorted_includes(includes) + batch[cutoff:])
            else:
                output.extend(batch)
            del batch[:]

        for kid in self.kids:
            if isinstance(kid, CppBlock):
                flush_batch()
                output.append(kid.sorted(enclosing_inclname))
            elif isinstance(kid, Include):
                batch.append(kid)
            else:
                assert isinstance(kid, OrdinaryCode)
                if kid.to_source().isspace():
                    batch.append(kid)
                else:
                    flush_batch()
                    output.append(kid)
        flush_batch()

        result = CppBlock()
        result.start = self.start
        result.end = self.end
        result.kids = output
        return result

    def to_source(self):
        return self.start + "".join(kid.to_source() for kid in self.kids) + self.end


class OrdinaryCode(object):
    """A list of lines of code that aren't #include/#if/#else/#endif lines."""

    def __init__(self, lines=None):
        self.lines = lines if lines is not None else []

    def is_style_relevant(self):
        return False

    def to_source(self):
        return "".join(self.lines)


# A "snippet" is one of:
#
# *   Include - representing an #include line
# *   CppBlock - a whole file or #if/#elif/#else block; contains a list of snippets
# *   OrdinaryCode - representing lines of non-#include-relevant code


def read_file(f):
    block_stack = [CppBlock()]

    # Extract the #include statements as a tree of snippets.
    for linenum, line in enumerate(f, start=1):
        if line.lstrip().startswith("#"):
            # Look for a |#include "..."| line.
            m = re.match(r'(\s*#\s*include\s+)"([^"]*)"(.*)', line)
            if m is not None:
                prefix, inclname, suffix = m.groups()
                block_stack[-1].kids.append(
                    Include(prefix, inclname, suffix, linenum, is_system=False)
                )
                continue

            # Look for a |#include <...>| line.
            m = re.match(r"(\s*#\s*include\s+)<([^>]*)>(.*)", line)
            if m is not None:
                prefix, inclname, suffix = m.groups()
                block_stack[-1].kids.append(
                    Include(prefix, inclname, suffix, linenum, is_system=True)
                )
                continue

            # Look for a |#{if,ifdef,ifndef}| line.
            m = re.match(r"\s*#\s*(if|ifdef|ifndef)\b", line)
            if m is not None:
                # Open a new block.
                new_block = CppBlock(line)
                block_stack[-1].kids.append(new_block)
                block_stack.append(new_block)
                continue

            # Look for a |#{elif,else}| line.
            m = re.match(r"\s*#\s*(elif|else)\b", line)
            if m is not None:
                # Close the current block, and open an adjacent one.
                block_stack.pop()
                new_block = CppBlock(line)
                block_stack[-1].kids.append(new_block)
                block_stack.append(new_block)
                continue

            # Look for a |#endif| line.
            m = re.match(r"\s*#\s*endif\b", line)
            if m is not None:
                # Close the current block.
                block_stack.pop().end = line
                if len(block_stack) == 0:
                    raise ValueError("#endif without #if at line " + str(linenum))
                continue

        # Otherwise, we have an ordinary line.
        block_stack[-1].append_ordinary_line(line)

    if len(block_stack) > 1:
        raise ValueError("unmatched #if")
    return block_stack[-1]


def check_file(
    filename, inclname, file_kind, code, all_inclnames, included_h_inclnames
):
    def check_include_statement(include):
        """Check the style of a single #include statement."""

        if include.is_system:
            # Check it is not a known local file (in which case it's probably a system header).
            if (
                include.inclname in included_inclnames_to_ignore
                or include.inclname in all_inclnames
            ):
                error(
                    filename,
                    include.linenum,
                    include.quote() + " should be included using",
                    'the #include "..." form',
                )

        else:
            msg = deprecated_inclnames.get(include.inclname)
            if msg:
                error(
                    filename,
                    include.linenum,
                    include.quote() + " is deprecated: " + msg,
                )

            if file_kind == FileKind.H or file_kind == FileKind.INL_H:
                msg = deprecated_inclnames_in_header.get(include.inclname)
                if msg and filename not in deprecated_inclnames_in_header_excludes:
                    error(
                        filename,
                        include.linenum,
                        include.quote() + " is deprecated: " + msg,
                    )

            if include.inclname not in included_inclnames_to_ignore:
                included_kind = FileKind.get(include.inclname)

                # Check the #include path has the correct form.
                if include.inclname not in all_inclnames:
                    error(
                        filename,
                        include.linenum,
                        include.quote() + " is included using the wrong path;",
                        "did you forget a prefix, or is the file not yet committed?",
                    )

                # Record inclusions of .h files for cycle detection later.
                # (Exclude .tbl and .msg files.)
                elif included_kind == FileKind.H or included_kind == FileKind.INL_H:
                    included_h_inclnames.add(include.inclname)

                # Check a H file doesn't #include an INL_H file.
                if file_kind == FileKind.H and included_kind == FileKind.INL_H:
                    error(
                        filename,
                        include.linenum,
                        "vanilla header includes an inline-header file "
                        + include.quote(),
                    )

                # Check a file doesn't #include itself.  (We do this here because the cycle
                # detection below doesn't detect this case.)
                if inclname == include.inclname:
                    error(filename, include.linenum, "the file includes itself")

    def check_includes_order(include1, include2):
        """Check the ordering of two #include statements."""

        if (
            include1.inclname in oddly_ordered_inclnames
            or include2.inclname in oddly_ordered_inclnames
        ):
            return

        section1 = include1.section(inclname)
        section2 = include2.section(inclname)
        if (section1 > section2) or (
            (section1 == section2)
            and (include1.inclname.lower() > include2.inclname.lower())
        ):
            error(
                filename,
                str(include1.linenum) + ":" + str(include2.linenum),
                include1.quote() + " should be included after " + include2.quote(),
            )

    # Check the extracted #include statements, both individually, and the ordering of
    # adjacent pairs that live in the same block.
    def pair_traverse(prev, this):
        if isinstance(this, Include):
            check_include_statement(this)
            if isinstance(prev, Include):
                check_includes_order(prev, this)
        else:
            kids = this.style_relevant_kids()
            for prev2, this2 in zip([None] + kids[0:-1], kids):
                pair_traverse(prev2, this2)

    pair_traverse(None, code)


def find_cycles(all_inclnames, edges):
    """Find and draw any cycles."""

    SCCs = tarjan(all_inclnames, edges)

    # The various sorted() calls below ensure the output is deterministic.

    def draw_SCC(c):
        cset = set(c)
        drawn = set()

        def draw(v, indent):
            out("   " * indent + ("-> " if indent else "   ") + v)
            if v in drawn:
                return
            drawn.add(v)
            for succ in sorted(edges[v]):
                if succ in cset:
                    draw(succ, indent + 1)

        draw(sorted(c)[0], 0)
        out("")

    have_drawn_an_SCC = False
    for scc in sorted(SCCs):
        if len(scc) != 1:
            if not have_drawn_an_SCC:
                error("(multiple files)", None, "header files form one or more cycles")
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
    if sys.argv[1:] == ["--fixup"]:
        # Sort #include directives in-place.  Fixup mode doesn't solve
        # all possible silliness that the script checks for; it's just a
        # hack for the common case where renaming a header causes style
        # errors.
        fixup = True
    elif sys.argv[1:] == []:
        fixup = False
    else:
        print(
            "TEST-UNEXPECTED-FAIL | check_spidermonkey_style.py | unexpected command "
            "line options: " + repr(sys.argv[1:])
        )
        sys.exit(1)

    ok = check_style(fixup)

    if ok:
        print("TEST-PASS | check_spidermonkey_style.py | ok")
    else:
        print(
            "TEST-UNEXPECTED-FAIL | check_spidermonkey_style.py | "
            + "actual output does not match expected output;  diff is above."
        )
        print(
            "TEST-UNEXPECTED-FAIL | check_spidermonkey_style.py | "
            + "Hint: If the problem is that you renamed a header, and many #includes "
            + "are no longer in alphabetical order, commit your work and then try "
            + "`check_spidermonkey_style.py --fixup`. "
            + "You need to commit first because --fixup modifies your files in place."
        )

    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
