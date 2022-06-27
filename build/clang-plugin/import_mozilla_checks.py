#!/usr/bin/python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import glob
import shutil
import errno

import ThirdPartyPaths
import ThreadAllows


def copy_dir_contents(src, dest):
    for f in glob.glob("%s/*" % src):
        try:
            destname = "%s/%s" % (dest, os.path.basename(f))
            if os.path.isdir(f):
                shutil.copytree(f, destname)
            else:
                shutil.copy2(f, destname)
        except OSError as e:
            if e.errno == errno.ENOTDIR:
                shutil.copy2(f, destname)
            elif e.errno == errno.EEXIST:
                if os.path.isdir(f):
                    copy_dir_contents(f, destname)
                else:
                    os.remove(destname)
                    shutil.copy2(f, destname)
            else:
                raise Exception("Directory not copied. Error: %s" % e)


def write_cmake(module_path, import_options):
    names = ["  " + os.path.basename(f) for f in glob.glob("%s/*.cpp" % module_path)]

    if import_options["external"]:
        names += [
            "  " + os.path.join("external", os.path.basename(f))
            for f in glob.glob("%s/external/*.cpp" % (module_path))
        ]

    if import_options["alpha"]:
        names += [
            "  " + os.path.join("alpha", os.path.basename(f))
            for f in glob.glob("%s/alpha/*.cpp" % (module_path))
        ]

    with open(os.path.join(module_path, "CMakeLists.txt"), "w") as f:
        f.write(
            """set(LLVM_LINK_COMPONENTS support)

add_definitions( -DCLANG_TIDY )

add_clang_library(clangTidyMozillaModule
  ThirdPartyPaths.cpp
%(names)s

  LINK_LIBS
  clangTidy
  clangTidyReadabilityModule
  clangTidyUtils
  clangTidyMPIModule
  )

clang_target_link_libraries(clangTidyMozillaModule
  PRIVATE
  clangAST
  clangASTMatchers
  clangBasic
  clangLex
  )"""
            % {"names": "\n".join(names)}
        )


def add_moz_module(cmake_path):
    with open(cmake_path, "r") as f:
        lines = f.readlines()
    f.close()

    try:
        idx = lines.index("set(ALL_CLANG_TIDY_CHECKS\n")
        lines.insert(idx + 1, "  clangTidyMozillaModule\n")

        with open(cmake_path, "w") as f:
            for line in lines:
                f.write(line)
    except ValueError:
        raise Exception("Unable to find ALL_CLANG_TIDY_CHECKS in {}".format(cmake_path))


def write_third_party_paths(mozilla_path, module_path):
    tpp_txt = os.path.join(mozilla_path, "../../tools/rewriting/ThirdPartyPaths.txt")
    generated_txt = os.path.join(mozilla_path, "../../tools/rewriting/Generated.txt")
    # Unvalidated.txt is used for rare cases where we don't want to validate that a given
    # path exists but still want it included in the ThirdPartyPaths check in the plugin.
    # For example, headers exported to dist/include that live elsewhere.
    unvalidated_txt = os.path.join(
        mozilla_path, "../../tools/rewriting/Unvalidated.txt"
    )
    with open(os.path.join(module_path, "ThirdPartyPaths.cpp"), "w") as f:
        ThirdPartyPaths.generate(f, tpp_txt, generated_txt, unvalidated_txt)


def generate_thread_allows(mozilla_path, module_path):
    names = os.path.join(mozilla_path, "../../build/clang-plugin/ThreadAllows.txt")
    files = os.path.join(mozilla_path, "../../build/clang-plugin/ThreadFileAllows.txt")
    with open(os.path.join(module_path, "ThreadAllows.h"), "w") as f:
        f.write(ThreadAllows.generate_allows({files, names}))


def do_import(mozilla_path, clang_tidy_path, import_options):
    module = "mozilla"
    module_path = os.path.join(clang_tidy_path, module)
    try:
        os.makedirs(module_path)
    except OSError as e:
        if e.errno != errno.EEXIST:
            raise

    copy_dir_contents(mozilla_path, module_path)
    write_third_party_paths(mozilla_path, module_path)
    generate_thread_allows(mozilla_path, module_path)
    write_cmake(module_path, import_options)
    add_moz_module(os.path.join(module_path, "..", "CMakeLists.txt"))
    with open(os.path.join(module_path, "..", "CMakeLists.txt"), "a") as f:
        f.write("add_subdirectory(%s)\n" % module)
    # A better place for this would be in `ClangTidyForceLinker.h` but `ClangTidyMain.cpp`
    # is also OK.
    with open(os.path.join(module_path, "..", "tool", "ClangTidyMain.cpp"), "a") as f:
        f.write(
            """
// This anchor is used to force the linker to link the MozillaModule.
extern volatile int MozillaModuleAnchorSource;
static int LLVM_ATTRIBUTE_UNUSED MozillaModuleAnchorDestination =
          MozillaModuleAnchorSource;
"""
        )


def main():
    import argparse

    parser = argparse.ArgumentParser(
        usage="import_mozilla_checks.py <mozilla-clang-plugin-path> <clang-tidy-path> [option]",
        description="Imports the Mozilla static analysis checks into a clang-tidy source tree.",
    )
    parser.add_argument(
        "mozilla_path", help="Full path to mozilla-central/build/clang-plugin"
    )
    parser.add_argument(
        "clang_tidy_path", help="Full path to llvm-project/clang-tools-extra/clang-tidy"
    )
    parser.add_argument(
        "--import-alpha",
        help="Enable import of in-tree alpha checks",
        action="store_true",
    )
    parser.add_argument(
        "--import-external",
        help="Enable import of in-tree external checks",
        action="store_true",
    )
    args = parser.parse_args()

    if not os.path.isdir(args.mozilla_path):
        print("Invalid path to mozilla clang plugin")

    if not os.path.isdir(args.clang_tidy_path):
        print("Invalid path to clang-tidy source directory")

    import_options = {"alpha": args.import_alpha, "external": args.import_external}

    do_import(args.mozilla_path, args.clang_tidy_path, import_options)


if __name__ == "__main__":
    main()
