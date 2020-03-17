#!/usr/bin/python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys
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
                raise Exception('Directory not copied. Error: %s' % e)


def write_cmake(module_path, import_alpha):
    names = ['  ' + os.path.basename(f) for f in glob.glob("%s/*.cpp" % module_path)]
    names += ['  ' + os.path.basename(f) for f in glob.glob("%s/external/*.cpp" % module_path)]
    if import_alpha:
        alpha_names = ['  ' + os.path.join("alpha", os.path.basename(f))
                       for f in glob.glob("%s/alpha/*.cpp" % module_path)]
        names += alpha_names
    with open(os.path.join(module_path, 'CMakeLists.txt'), 'w') as f:
        f.write("""set(LLVM_LINK_COMPONENTS support)

add_definitions( -DCLANG_TIDY )

add_clang_library(clangTidyMozillaModule
  ThirdPartyPaths.cpp
%(names)s

  LINK_LIBS
  clangAST
  clangASTMatchers
  clangBasic
  clangLex
  clangTidy
  clangTidyReadabilityModule
  clangTidyUtils
  clangTidyMPIModule
  )""" % {'names': "\n".join(names)})


def add_item_to_cmake_section(cmake_path, section, library):
    with open(cmake_path, 'r') as f:
        lines = f.readlines()
    f.close()

    libs = []
    seen_target_libs = False
    for line in lines:
        if line.find(section) > -1:
            seen_target_libs = True
        elif seen_target_libs:
            if line.find(')') > -1:
                break
            else:
                libs.append(line.strip())
    libs.append(library)
    libs = sorted(libs, key=lambda s: s.lower())

    with open(cmake_path, 'w') as f:
        seen_target_libs = False
        for line in lines:
            if line.find(section) > -1:
                seen_target_libs = True
                f.write(line)
                f.writelines(['  ' + p + '\n' for p in libs])
                continue
            elif seen_target_libs:
                if line.find(')') > -1:
                    seen_target_libs = False
                else:
                    continue
            f.write(line)

    f.close()


def write_third_party_paths(mozilla_path, module_path):
    tpp_txt = os.path.join(
        mozilla_path, '../../tools/rewriting/ThirdPartyPaths.txt')
    generated_txt = os.path.join(
        mozilla_path, '../../tools/rewriting/Generated.txt')
    with open(os.path.join(module_path, 'ThirdPartyPaths.cpp'), 'w') as f:
        ThirdPartyPaths.generate(f, tpp_txt, generated_txt)


def generate_thread_allows(mozilla_path, module_path):
    names = os.path.join(
        mozilla_path, '../../build/clang-plugin/ThreadAllows.txt'
    )
    files = os.path.join(
        mozilla_path, '../../build/clang-plugin/ThreadFileAllows.txt'
    )
    with open(os.path.join(module_path, 'ThreadAllows.h'), 'w') as f:
        f.write(ThreadAllows.generate_allows({files, names}))


def do_import(mozilla_path, clang_tidy_path, import_alpha):
    module = 'mozilla'
    module_path = os.path.join(clang_tidy_path, module)
    try:
        os.makedirs(module_path)
    except OSError as e:
        if e.errno != errno.EEXIST:
            raise

    copy_dir_contents(mozilla_path, module_path)
    write_third_party_paths(mozilla_path, module_path)
    generate_thread_allows(mozilla_path, module_path)
    write_cmake(module_path, import_alpha)
    add_item_to_cmake_section(os.path.join(module_path, '..', 'plugin',
                                           'CMakeLists.txt'),
                              'LINK_LIBS', 'clangTidyMozillaModule')
    add_item_to_cmake_section(os.path.join(module_path, '..', 'tool',
                                           'CMakeLists.txt'),
                              'PRIVATE', 'clangTidyMozillaModule')
    with open(os.path.join(module_path, '..', 'CMakeLists.txt'), 'a') as f:
        f.write('add_subdirectory(%s)\n' % module)
    with open(os.path.join(module_path, '..', 'tool', 'ClangTidyMain.cpp'), 'a') as f:
        f.write('''
// This anchor is used to force the linker to link the MozillaModule.
extern volatile int MozillaModuleAnchorSource;
static int LLVM_ATTRIBUTE_UNUSED MozillaModuleAnchorDestination =
          MozillaModuleAnchorSource;
''')


def main():
    if len(sys.argv) < 3 or len(sys.argv) > 4:
        print("""\
Usage: import_mozilla_checks.py <mozilla-clang-plugin-path> <clang-tidy-path> [import_alpha]
Imports the Mozilla static analysis checks into a clang-tidy source tree.
If `import_alpha` is specified then in-tree alpha checkers will be also imported.
""")

        return

    mozilla_path = sys.argv[1]
    if not os.path.isdir(mozilla_path):
        print("Invalid path to mozilla clang plugin")

    clang_tidy_path = sys.argv[2]
    if not os.path.isdir(mozilla_path):
        print("Invalid path to clang-tidy source directory")

    import_alpha = False

    if len(sys.argv) == 4 and sys.argv[3] == 'import_alpha':
        import_alpha = True

    do_import(mozilla_path, clang_tidy_path, import_alpha)


if __name__ == '__main__':
    main()
