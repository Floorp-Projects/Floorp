# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


IGNORED_FILE_PREFIXES = ["."]
IGNORED_FILE_SUFFIXES = ["~", ".swp"]
IGNORED_DIRS = [".git", ".svn", ".hg"]

def filter_filenames(filenames, ignored_files=[".hgignore"]):
    for filename in filenames:
        if filename in ignored_files:
            continue
        if any([filename.startswith(suffix)
                for suffix in IGNORED_FILE_PREFIXES]):
            continue
        if any([filename.endswith(suffix)
                for suffix in IGNORED_FILE_SUFFIXES]):
            continue
        yield filename

def filter_dirnames(dirnames):
    return [dirname for dirname in dirnames if dirname not in IGNORED_DIRS]
