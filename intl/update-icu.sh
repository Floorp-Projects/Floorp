#!/bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set -e

# Usage: update-icu.sh <URL of ICU SVN with release>
# E.g., for ICU 55.1: update-icu.sh http://source.icu-project.org/repos/icu/icu/tags/release-55-1/

if [ $# -lt 1 ]; then
  echo "Usage: update-icu.sh <URL of ICU SVN with release>"
  exit 1
fi

# Ensure that $Date$ in the checked-out svn files expands timezone-agnostically,
# so that this script's behavior is consistent when run from any time zone.
export TZ=UTC

icu_dir=`dirname $0`/icu

# Remove intl/icu/source, then replace it with a clean export.
rm -rf ${icu_dir}/source
svn export $1/source/ ${icu_dir}/source

# remove layout, tests, and samples, but leave makefiles in place
find ${icu_dir}/source/layout -name '*Makefile.in' -prune -or -type f -print | xargs rm
find ${icu_dir}/source/layoutex -name '*Makefile.in' -prune -or -type f -print | xargs rm
find ${icu_dir}/source/test -name '*Makefile.in' -prune -or -type f -print | xargs rm
find ${icu_dir}/source/samples -name '*Makefile.in' -prune -or -type f -print | xargs rm

# remove data that we currently don't need
rm ${icu_dir}/source/data/brkitr/*
rm ${icu_dir}/source/data/lang/*.mk
rm ${icu_dir}/source/data/lang/*.txt
rm ${icu_dir}/source/data/mappings/*.mk
find ${icu_dir}/source/data/mappings \
    -name ibm-37_P100-1995.ucm -prune -or \
    -name ibm-1047_P100-1995.ucm -prune -or \
    -name '*.ucm' -print | xargs rm
rm ${icu_dir}/source/data/rbnf/*
rm ${icu_dir}/source/data/region/*.mk
rm ${icu_dir}/source/data/region/*.txt
rm ${icu_dir}/source/data/translit/*
rm ${icu_dir}/source/data/unit/*.mk
rm ${icu_dir}/source/data/unit/*.txt

# Record `svn info`, eliding the line that changes every time the entire ICU
# repository (not just the path within it we care about) receives a commit.
# (This ensures that if ICU modifications are performed properly, it's always
# possible to run the command at the top of this script and make no changes to
# the tree.)
svn info $1 | grep -v '^Revision: [[:digit:]]\+$' > ${icu_dir}/SVN-INFO

patch -d ${icu_dir}/../../ -p1 < ${icu_dir}/../icu-patches/bug-915735
patch -d ${icu_dir}/../../ -p1 < ${icu_dir}/../icu-patches/suppress-warnings.diff
patch -d ${icu_dir}/../../ -p1 < ${icu_dir}/../icu-patches/pkgdata-large-buffer.diff

# NOTE: If you're updating this script for a new ICU version, you have to rerun
# js/src/tests/ecma_6/String/make-normalize-generateddata-input.py for any
# normalization changes the new ICU implements.

hg addremove ${icu_dir}
