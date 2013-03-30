#!/bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Warning
# =======
# As of ICU 51.1, ICU as obtained from the ICU repository does not
# build with the Mozilla build tools for Windows. Check
# http://bugs.icu-project.org/trac/ticket/9985
# whether this has been addressed in the version you're updating to.
# If not, obtain the patch "Make ICU build with Mozilla build for Windows" from
# https://bugzilla.mozilla.org/show_bug.cgi?id=724533
# and reapply it after running update-icu.sh (additional updates may be needed).
# If the bug has been addressed, please delete this warning.

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

hg addremove ${icu_dir}
