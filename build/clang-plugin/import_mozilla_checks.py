#!/usr/bin/python2.7
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import re
import sys
import glob
import shutil
import errno

import ThirdPartyPaths

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

def write_cmake(module_path):
  names = map(lambda f: '  ' + os.path.basename(f),
              glob.glob("%s/*.cpp" % module_path))
  with open(os.path.join(module_path, 'CMakeLists.txt'), 'wb') as f:
    f.write("""set(LLVM_LINK_COMPONENTS support)

add_definitions( -DCLANG_TIDY )
add_definitions( -DHAVE_NEW_ASTMATCHER_NAMES )

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
  libs = sorted(libs, key = lambda s: s.lower())

  with open(cmake_path, 'wb') as f:
    seen_target_libs = False
    for line in lines:
      if line.find(section) > -1:
        seen_target_libs = True
        f.write(line)
        f.writelines(map(lambda p: '  ' + p + '\n', libs))
        continue
      elif seen_target_libs:
        if line.find(')') > -1:
          seen_target_libs = False
        else:
          continue
      f.write(line)

  f.close()


def write_third_party_paths(mozilla_path, module_path):
  tpp_txt = os.path.join(mozilla_path, '../../tools/rewriting/ThirdPartyPaths.txt')
  with open(os.path.join(module_path, 'ThirdPartyPaths.cpp'), 'w') as f:
    ThirdPartyPaths.generate(f, tpp_txt)


def do_import(mozilla_path, clang_tidy_path):
  module = 'mozilla'
  module_path = os.path.join(clang_tidy_path, module)
  if not os.path.isdir(module_path):
      os.mkdir(module_path)

  copy_dir_contents(mozilla_path, module_path)
  write_third_party_paths(mozilla_path, module_path)
  write_cmake(module_path)
  add_item_to_cmake_section(os.path.join(module_path, '..', 'plugin',
                                         'CMakeLists.txt'),
                            'LINK_LIBS', 'clangTidyMozillaModule')
  add_item_to_cmake_section(os.path.join(module_path, '..', 'tool',
                                         'CMakeLists.txt'),
                            'target_link_libraries', 'clangTidyMozillaModule')
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
  if len(sys.argv) != 3:
    print """\
Usage: import_mozilla_checks.py <mozilla-clang-plugin-path> <clang-tidy-path>
Imports the Mozilla static analysis checks into a clang-tidy source tree.
"""

    return

  mozilla_path = sys.argv[1]
  if not os.path.isdir(mozilla_path):
      print "Invalid path to mozilla clang plugin"

  clang_tidy_path = sys.argv[2]
  if not os.path.isdir(mozilla_path):
      print "Invalid path to clang-tidy source directory"

  do_import(mozilla_path, clang_tidy_path)


if __name__ == '__main__':
  main()
