#!/usr/local/bin/bash
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

TEST_JSEACH_TIMEOUT=${TEST_JSEACH_TIMEOUT:-900}
TEST_JSEACH_PAGE_TIMEOUT=${TEST_JSEACH_PAGE_TIMEOUT:-900}

TEST_WWW_JS=`echo $TEST_JSDIR|sed "s|$TEST_DIR||"`
#
# options processing
#
options="p:b:x:N:d:"
usage()
{
    cat <<EOF
usage: $SCRIPT -p product -b branch -x executablepath -N profilename

variable            description
===============     ============================================================
-p product          required. firefox|thunderbird
-b branch           required. 1.8.0|1.8.1|1.9.0
-x executablepath   required. directory-tree containing executable 'product'
-N profilename      required. profile name 

-d datafiles        optional. one or more filenames of files containing 
                    environment variable definitions to be included.

                    note that the environment variables should have the same 
                    names as in the "variable" column.

if an argument contains more than one value, it must be quoted.
EOF
    exit 2
}

unset product branch profilename executablepath datafiles

while getopts $options optname ; 
do 
    case $optname in
        p) product=$OPTARG;;
        b) branch=$OPTARG;;
        N) profilename=$OPTARG;;
        x) executablepath=$OPTARG;;
        d) datafiles=$OPTARG;;
    esac
done

# include environment variables
if [[ -n "$datafiles" ]]; then
    for datafile in $datafiles; do 
        source $datafile
    done
fi

if [[ -z "$product" || -z "$branch" || -z "$executablepath" || -z "$profilename" ]]; then
    usage
fi

executable=`get_executable $product $branch $executablepath`

case "$branch" in 
    1.8.0)
        list=1.8.0-list.txt
        ;;
    1.8.1)
        list=1.8.1-list.txt
        ;;
    1.9.0)
        list=1.9.0-list.txt
        ;;
esac

pushd $TEST_JSDIR

# clock skew causes failures. clean first
make clean
make

cat "$list" | while read url; do 
	edit-talkback.sh -p "$product" -b "$branch" -x "$executablepath" -i "$url"
	time timed_run.py $TEST_JSEACH_TIMEOUT "$url" \
		"$executable" -P "$profilename" \
		-spider -start -quit \
		-uri "$url" \
		-depth 0 -timeout "$TEST_JSEACH_PAGE_TIMEOUT" \
		-hook "http://$TEST_HTTP$TEST_WWW_JS/userhookeach.js"; 
done

popd
