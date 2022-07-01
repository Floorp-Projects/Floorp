# vim: set ts=8 sts=4 et sw=4 tw=99:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# ----------------------------------------------------------------------------
# This script checks that SpiderMonkey MacroAssembler methods are properly
# annotated.
#
# The MacroAssembler has one interface for all platforms, but it might have one
# definition per platform. The code of the MacroAssembler use a macro to
# annotate the method declarations, in order to delete the function if it is not
# present on the current platform, and also to locate the files in which the
# methods are defined.
#
# This script scans the MacroAssembler.h header, for method declarations.
# It also scans MacroAssembler-/arch/.cpp, MacroAssembler-/arch/-inl.h, and
# MacroAssembler-inl.h for method definitions. The result of both scans are
# uniformized, and compared, to determine if the MacroAssembler.h header as
# proper methods annotations.
# ----------------------------------------------------------------------------

from __future__ import absolute_import
from __future__ import print_function

import difflib
import os
import re
import sys

architecture_independent = set(["generic"])
all_unsupported_architectures_names = set(["mips32", "mips64", "mips_shared"])
all_architecture_names = set(["x86", "x64", "arm", "arm64", "loong64", "wasm32"])
all_shared_architecture_names = set(["x86_shared", "arm", "arm64", "loong64", "wasm32"])

reBeforeArg = "(?<=[(,\s])"
reArgType = "(?P<type>[\w\s:*&<>]+)"
reArgName = "(?P<name>\s\w+)"
reArgDefault = "(?P<default>(?:\s=(?:(?:\s[\w:]+\(\))|[^,)]+))?)"
reAfterArg = "(?=[,)])"
reMatchArg = re.compile(reBeforeArg + reArgType + reArgName + reArgDefault + reAfterArg)


def get_normalized_signatures(signature, fileAnnot=None):
    # Remove static
    signature = signature.replace("static", "")
    # Remove semicolon.
    signature = signature.replace(";", " ")
    # Normalize spaces.
    signature = re.sub(r"\s+", " ", signature).strip()
    # Remove new-line induced spaces after opening braces.
    signature = re.sub(r"\(\s+", "(", signature).strip()
    # Match arguments, and keep only the type.
    signature = reMatchArg.sub("\g<type>", signature)
    # Remove class name
    signature = signature.replace("MacroAssembler::", "")

    # Extract list of architectures
    archs = ["generic"]
    if fileAnnot:
        archs = [fileAnnot["arch"]]

    if "DEFINED_ON(" in signature:
        archs = re.sub(
            r".*DEFINED_ON\((?P<archs>[^()]*)\).*", "\g<archs>", signature
        ).split(",")
        archs = [a.strip() for a in archs]
        signature = re.sub(r"\s+DEFINED_ON\([^()]*\)", "", signature)

    elif "PER_ARCH" in signature:
        archs = all_architecture_names
        signature = re.sub(r"\s+PER_ARCH", "", signature)

    elif "PER_SHARED_ARCH" in signature:
        archs = all_shared_architecture_names
        signature = re.sub(r"\s+PER_SHARED_ARCH", "", signature)

    elif "OOL_IN_HEADER" in signature:
        assert archs == ["generic"]
        signature = re.sub(r"\s+OOL_IN_HEADER", "", signature)

    else:
        # No signature annotation, the list of architectures remains unchanged.
        pass

    # Extract inline annotation
    inline = False
    if fileAnnot:
        inline = fileAnnot["inline"]

    if "inline " in signature:
        signature = re.sub(r"inline\s+", "", signature)
        inline = True

    inlinePrefx = ""
    if inline:
        inlinePrefx = "inline "
    signatures = [{"arch": a, "sig": inlinePrefx + signature} for a in archs]

    return signatures


file_suffixes = set(
    [
        a.replace("_", "-")
        for a in all_architecture_names.union(all_shared_architecture_names).union(
            all_unsupported_architectures_names
        )
    ]
)


def get_file_annotation(filename):
    origFilename = filename
    filename = filename.split("/")[-1]

    inline = False
    if filename.endswith(".cpp"):
        filename = filename[: -len(".cpp")]
    elif filename.endswith("-inl.h"):
        inline = True
        filename = filename[: -len("-inl.h")]
    elif filename.endswith(".h"):
        # This allows the definitions block in MacroAssembler.h to be
        # style-checked.
        inline = True
        filename = filename[: -len(".h")]
    else:
        raise Exception("unknown file name", origFilename)

    arch = "generic"
    for suffix in file_suffixes:
        if filename == "MacroAssembler-" + suffix:
            arch = suffix
            break

    return {"inline": inline, "arch": arch.replace("-", "_")}


def get_macroassembler_definitions(filename):
    try:
        fileAnnot = get_file_annotation(filename)
    except Exception:
        return []

    style_section = False
    lines = ""
    signatures = []
    with open(filename, encoding="utf-8") as f:
        for line in f:
            if "//{{{ check_macroassembler_style" in line:
                if style_section:
                    raise "check_macroassembler_style section already opened."
                style_section = True
                braces_depth = 0
            elif "//}}} check_macroassembler_style" in line:
                style_section = False
            if not style_section:
                continue

            # Ignore preprocessor directives.
            if line.startswith("#"):
                continue

            # Remove comments from the processed line.
            line = re.sub(r"//.*", "", line)

            # Locate and count curly braces.
            open_curly_brace = line.find("{")
            was_braces_depth = braces_depth
            braces_depth = braces_depth + line.count("{") - line.count("}")

            # Raise an error if the check_macroassembler_style macro is used
            # across namespaces / classes scopes.
            if braces_depth < 0:
                raise "check_macroassembler_style annotations are not well scoped."

            # If the current line contains an opening curly brace, check if
            # this line combines with the previous one can be identified as a
            # MacroAssembler function signature.
            if open_curly_brace != -1 and was_braces_depth == 0:
                lines = lines + line[:open_curly_brace]
                if "MacroAssembler::" in lines:
                    signatures.extend(get_normalized_signatures(lines, fileAnnot))
                lines = ""
                continue

            # We do not aggregate any lines if we are scanning lines which are
            # in-between a set of curly braces.
            if braces_depth > 0:
                continue
            if was_braces_depth != 0:
                line = line[line.rfind("}") + 1 :]

            # This logic is used to remove template instantiation, static
            # variable definitions and function declaration from the next
            # function definition.
            last_semi_colon = line.rfind(";")
            if last_semi_colon != -1:
                lines = ""
                line = line[last_semi_colon + 1 :]

            # Aggregate lines of non-braced text, which corresponds to the space
            # where we are expecting to find function definitions.
            lines = lines + line

    return signatures


def get_macroassembler_declaration(filename):
    style_section = False
    lines = ""
    signatures = []
    with open(filename, encoding="utf-8") as f:
        for line in f:
            if "//{{{ check_macroassembler_decl_style" in line:
                style_section = True
            elif "//}}} check_macroassembler_decl_style" in line:
                style_section = False
            if not style_section:
                continue

            # Ignore preprocessor directives.
            if line.startswith("#"):
                continue

            line = re.sub(r"//.*", "", line)
            if len(line.strip()) == 0 or "public:" in line or "private:" in line:
                lines = ""
                continue

            lines = lines + line

            # Continue until we have a complete declaration
            if ";" not in lines:
                continue

            # Skip member declarations: which are lines ending with a
            # semi-colon without any list of arguments.
            if ")" not in lines:
                lines = ""
                continue

            signatures.extend(get_normalized_signatures(lines))
            lines = ""

    return signatures


def append_signatures(d, sigs):
    for s in sigs:
        if s["sig"] not in d:
            d[s["sig"]] = []
        d[s["sig"]].append(s["arch"])
    return d


def generate_file_content(signatures):
    output = []
    for s in sorted(signatures.keys()):
        archs = set(sorted(signatures[s]))
        archs -= all_unsupported_architectures_names
        if len(archs.symmetric_difference(architecture_independent)) == 0:
            output.append(s + ";\n")
            if s.startswith("inline"):
                # TODO, bug 1432600: This is mistaken for OOL_IN_HEADER
                # functions.  (Such annotation is already removed by the time
                # this function sees the signature here.)
                output.append("    is defined in MacroAssembler-inl.h\n")
            else:
                output.append("    is defined in MacroAssembler.cpp\n")
        else:
            if len(archs.symmetric_difference(all_architecture_names)) == 0:
                output.append(s + " PER_ARCH;\n")
            elif len(archs.symmetric_difference(all_shared_architecture_names)) == 0:
                output.append(s + " PER_SHARED_ARCH;\n")
            else:
                output.append(s + " DEFINED_ON(" + ", ".join(sorted(archs)) + ");\n")
            for a in sorted(archs):
                a = a.replace("_", "-")
                masm = "%s/MacroAssembler-%s" % (a, a)
                if s.startswith("inline"):
                    output.append("    is defined in %s-inl.h\n" % masm)
                else:
                    output.append("    is defined in %s.cpp\n" % masm)
    return output


def check_style():
    # We read from the header file the signature of each function.
    decls = dict()  # type: dict(signature => ['x86', 'x64'])

    # We infer from each file the signature of each MacroAssembler function.
    defs = dict()  # type: dict(signature => ['x86', 'x64'])

    root_dir = os.path.join("js", "src", "jit")
    for dirpath, dirnames, filenames in os.walk(root_dir):
        for filename in filenames:
            if "MacroAssembler" not in filename:
                continue

            filepath = os.path.join(dirpath, filename).replace("\\", "/")

            if filepath.endswith("MacroAssembler.h"):
                decls = append_signatures(
                    decls, get_macroassembler_declaration(filepath)
                )
            defs = append_signatures(defs, get_macroassembler_definitions(filepath))

    if not decls or not defs:
        raise Exception("Did not find any definitions or declarations")

    # Compare declarations and definitions output.
    difflines = difflib.unified_diff(
        generate_file_content(decls),
        generate_file_content(defs),
        fromfile="check_macroassembler_style.py declared syntax",
        tofile="check_macroassembler_style.py found definitions",
    )
    ok = True
    for diffline in difflines:
        ok = False
        print(diffline, end="")

    return ok


def main():
    ok = check_style()

    if ok:
        print("TEST-PASS | check_macroassembler_style.py | ok")
    else:
        print(
            "TEST-UNEXPECTED-FAIL | check_macroassembler_style.py | actual output does not match expected output;  diff is above"  # noqa: E501
        )

    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
