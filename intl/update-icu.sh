#!/bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Usage: update-icu.sh <URL of ICU SVN with release>
# E.g., for ICU 50.1.1: update-icu.sh http://source.icu-project.org/repos/icu/icu/tags/release-50-1-1/

if [ $# -lt 1 ]; then
  echo "Usage: update-icu.sh <URL of ICU SVN with release>"
  exit 1
fi

icu_dir=`dirname $0`/icu
rm -rf ${icu_dir}
svn export $1 ${icu_dir}

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

# Record `svn info`
svn info $1 > ${icu_dir}/SVN-INFO

patch -d ${icu_dir}/../../ -p1 < ${icu_dir}/../icu-patches/bug-724533
patch -d ${icu_dir}/../../ -p1 < ${icu_dir}/../icu-patches/bug-853706
patch -d ${icu_dir}/../../ -p1 < ${icu_dir}/../icu-patches/bug-899722
patch -d ${icu_dir}/../../ -p1 < ${icu_dir}/../icu-patches/bug-899722-2
patch -d ${icu_dir}/../../ -p1 < ${icu_dir}/../icu-patches/bug-899722-4

hg addremove ${icu_dir}
