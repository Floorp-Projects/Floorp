#!/bin/bash
# -*- Mode: Shell-script; tab-width: 4; indent-tabs-mode: nil; -*-

# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla JavaScript Testing Utilities
#
# The Initial Developer of the Original Code is
# Mozilla Corporation.
# Portions created by the Initial Developer are Copyright (C) 2007
# the Initial Developer. All Rights Reserved.
#
# Contributor(s): Bob Clary <bclary@bclary.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

TEST_DIR=${TEST_DIR:-/work/mozilla/mozilla.com/test.mozilla.com/www}
TEST_BIN=${TEST_BIN:-$TEST_DIR/bin}
TEST_JSDIR=${TEST_JSDIR:-$TEST_DIR/tests/mozilla.org/js}

if [[ ! -e $TEST_BIN/library.sh ]]; then
    echo "TEST_DIR=$TEST_DIR"
    echo ""
    echo "This script requires the Sisyphus testing framework. Please "
    echo "cvs check out the Sisyphys framework from mozilla/testing/sisyphus"
    echo "and set the environment variable TEST_DIR to the directory where it"
    echo "located."
    echo ""
fi

if [[ ! -e $TEST_JSDIR/runtests.sh ]]; then
    echo "TEST_JSDIR=$TEST_JSDIR"
    echo ""
    echo "If the TEST_JSDIR environment variable is not set, this script "
    echo "assumes the JavaScript Tests live in \${TEST_DIR}/www/tests/mozilla.org/js"
    echo "If this is not correct, please set the TEST_JSDIR environment variable"
    echo "to point to the directory containing the JavaScript Test Library."
    echo ""
fi

if [[ ! -e $TEST_BIN/library.sh || ! -e $TEST_JSDIR/runtests.sh ]]; then
    exit 2
fi

source ${TEST_BIN}/library.sh

TEST_JSSHELL_TIMEOUT=${TEST_JSSHELL_TIMEOUT:-900}

#
# options processing
#
options="b:s:T:d:"
usage()
{
    cat <<EOF
usage: $SCRIPT -b branch -s sourcepath -T buildtype [-d datafiles]

variable            description
===============     ===========================================================
-p product          required. js
-b branch           required. 1.8.0|1.8.1|1.9.0
-s sourcepath       required. path to js shell source directory mozilla/js/src
-T buildtype        required. one of opt debug
-d datafiles        optional. one or more filenames of files containing 
                    environment variable definitions to be included.

EOF
    exit 2
}

while getopts $options optname ; 
do 
    case $optname in
        b) branch=$OPTARG;;
        s) sourcepath=$OPTARG;;
        T) buildtype=$OPTARG;;
        d) datafiles=$OPTARG;;
    esac
done

# include environment variables
if [[ -n "$datafiles" ]]; then
    for datafile in $datafiles; do 
        source $datafile
    done
fi

if [[ -z "$branch" || -z "$sourcepath" || -z "$buildtype" ]]; then
    usage
fi

pushd $TEST_JSDIR

. config.sh

executable="$sourcepath/$JS_OBJDIR/js$EXE_EXT"

case "$branch" in 
    1.8.0)
        included="ecma ecma_2 ecma_3 js1_1 js1_2 js1_3 js1_4 js1_5 js1_6 e4x"
        ;;
    1.8.1)
        included="ecma ecma_2 ecma_3 js1_1 js1_2 js1_3 js1_4 js1_5 js1_6 js1_7 e4x"
        ;;
    1.9.0)
        included="ecma ecma_2 ecma_3 js1_1 js1_2 js1_3 js1_4 js1_5 js1_6 js1_7 js1_8 e4x"
        ;;
esac

TEST_HTML_LOG="/dev/null"

# clock skew causes failures. clean first
make clean
make

time perl jsDriver.pl \
    -l $included \
	-L excluded-n.tests \
	-s $executable -e sm$buildtype \
	-o '-S 524288' \
	-R \
	-T $TEST_JSSHELL_TIMEOUT \
	-f $TEST_HTML_LOG \
    -Q

popd
