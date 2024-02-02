# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import functools
import os
import shutil
import subprocess
import sys

import buildconfig


def relativize(path, base=None):
    # For absolute path in Unix builds, we need relative paths because
    # Windows programs run via Wine don't like these Unix absolute paths
    # (they look like command line arguments).
    if path.startswith("/"):
        return os.path.relpath(path, base)
    # For Windows absolute paths, we can just use the unmodified path.
    # And if the path starts with '-', it's a command line argument.
    if os.path.isabs(path) or path.startswith("-"):
        return path
    # Remaining case is relative paths, which may be relative to a different
    # directory (os.getcwd()) than the needed `base`, so we "rebase" it.
    return os.path.relpath(path, base)


@functools.lru_cache(maxsize=None)
def files_in(path):
    return {p.lower(): os.path.join(path, p) for p in os.listdir(path)}


def search_path(paths, path):
    for p in paths:
        f = os.path.join(p, path)
        if os.path.isfile(f):
            return f
        # try an case-insensitive match
        maybe_match = files_in(p).get(path.lower())
        if maybe_match:
            return maybe_match
    raise RuntimeError(f"Cannot find {path}")


# Filter-out -std= flag from the preprocessor command, as we're not preprocessing
# C or C++, and the command would fail with the flag.
def filter_preprocessor(cmd):
    prev = None
    for arg in cmd:
        if arg == "-Xclang":
            prev = arg
            continue
        if not arg.startswith("-std="):
            if prev:
                yield prev
            yield arg
        prev = None


# Preprocess all the direct and indirect inputs of midl, and put all the
# preprocessed inputs in the given `base` directory. Returns a tuple containing
# the path of the main preprocessed input, and the modified flags to use instead
# of the flags given as argument.
def preprocess(base, input, flags):
    import argparse
    import re
    from collections import deque

    IMPORT_RE = re.compile(r'import\s*"([^"]+)";')

    parser = argparse.ArgumentParser()
    parser.add_argument("-I", action="append")
    parser.add_argument("-D", action="append")
    parser.add_argument("-acf")
    args, remainder = parser.parse_known_args(flags)
    preprocessor = (
        list(filter_preprocessor(buildconfig.substs["CXXCPP"]))
        # Ideally we'd use the real midl version, but querying it adds a
        # significant overhead to configure. In practice, the version number
        # doesn't make a difference at the moment.
        + ["-D__midl=801"]
        + [f"-D{d}" for d in args.D or ()]
        + [f"-I{i}" for i in args.I or ()]
    )
    includes = ["."] + buildconfig.substs["INCLUDE"].split(";") + (args.I or [])
    seen = set()
    queue = deque([input])
    if args.acf:
        queue.append(args.acf)
    output = os.path.join(base, os.path.basename(input))
    while True:
        try:
            input = queue.popleft()
        except IndexError:
            break
        if os.path.basename(input) in seen:
            continue
        seen.add(os.path.basename(input))
        input = search_path(includes, input)
        # If there is a .acf file corresponding to the .idl we're processing,
        # we also want to preprocess that file because midl might look for it too.
        if input.lower().endswith(".idl"):
            try:
                acf = search_path(
                    [os.path.dirname(input)], os.path.basename(input)[:-4] + ".acf"
                )
                if acf:
                    queue.append(acf)
            except RuntimeError:
                pass
        command = preprocessor + [input]
        preprocessed = os.path.join(base, os.path.basename(input))
        subprocess.run(command, stdout=open(preprocessed, "wb"), check=True)
        # Read the resulting file, and search for imports, that we'll want to
        # preprocess as well.
        with open(preprocessed, "r") as fh:
            for line in fh:
                if not line.startswith("import"):
                    continue
                m = IMPORT_RE.match(line)
                if not m:
                    continue
                imp = m.group(1)
                queue.append(imp)
    flags = []
    # Add -I<base> first in the flags, so that midl resolves imports to the
    # preprocessed files we created.
    for i in [base] + (args.I or []):
        flags.extend(["-I", i])
    # Add the preprocessed acf file if one was given on the command line.
    if args.acf:
        flags.extend(["-acf", os.path.join(base, os.path.basename(args.acf))])
    flags.extend(remainder)
    return output, flags


def midl(out, input, *flags):
    out.avoid_writing_to_file()
    midl_flags = buildconfig.substs["MIDL_FLAGS"]
    base = os.path.dirname(out.name) or "."
    tmpdir = None
    try:
        # If the build system is asking to not use the preprocessor to midl,
        # we need to do the preprocessing ourselves.
        if "-no_cpp" in midl_flags:
            # Normally, we'd use tempfile.TemporaryDirectory, but in this specific
            # case, we actually want a deterministic directory name, because it's
            # recorded in the code midl generates.
            tmpdir = os.path.join(base, os.path.basename(input) + ".tmp")
            os.makedirs(tmpdir, exist_ok=True)
            try:
                input, flags = preprocess(tmpdir, input, flags)
            except subprocess.CalledProcessError as e:
                return e.returncode
        midl = buildconfig.substs["MIDL"]
        wine = buildconfig.substs.get("WINE")
        if midl.lower().endswith(".exe") and wine:
            command = [wine, midl]
        else:
            command = [midl]
        command.extend(midl_flags)
        command.extend([relativize(f, base) for f in flags])
        command.append("-Oicf")
        command.append(relativize(input, base))
        print("Executing:", " ".join(command))
        result = subprocess.run(command, cwd=base)
        return result.returncode
    finally:
        if tmpdir:
            shutil.rmtree(tmpdir)


# midl outputs dlldata to a single dlldata.c file by default. This prevents running
# midl in parallel in the same directory for idl files that would generate dlldata.c
# because of race conditions updating the file. Instead, we ask midl to create
# separate files, and we merge them manually.
def merge_dlldata(out, *inputs):
    inputs = [open(i) for i in inputs]
    read_a_line = [True] * len(inputs)
    while True:
        lines = [
            f.readline() if read_a_line[n] else lines[n] for n, f in enumerate(inputs)
        ]
        unique_lines = set(lines)
        if len(unique_lines) == 1:
            # All the lines are identical
            if not lines[0]:
                break
            out.write(lines[0])
            read_a_line = [True] * len(inputs)
        elif (
            len(unique_lines) == 2
            and len([l for l in unique_lines if "#define" in l]) == 1
        ):
            # Most lines are identical. When they aren't, it's typically because some
            # files have an extra #define that others don't. When that happens, we
            # print out the #define, and get a new input line from the files that had
            # a #define on the next iteration. We expect that next line to match what
            # the other files had on this iteration.
            # Note: we explicitly don't support the case where there are different
            # defines across different files, except when there's a different one
            # for each file, in which case it's handled further below.
            a = unique_lines.pop()
            if "#define" in a:
                out.write(a)
            else:
                out.write(unique_lines.pop())
            read_a_line = ["#define" in l for l in lines]
        elif len(unique_lines) != len(lines):
            # If for some reason, we don't get lines that are entirely different
            # from each other, we have some unexpected input.
            print(
                "Error while merging dlldata. Last lines read: {}".format(lines),
                file=sys.stderr,
            )
            return 1
        else:
            for line in lines:
                out.write(line)
            read_a_line = [True] * len(inputs)

    return 0
