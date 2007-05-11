#!/usr/local/bin/bash
# -*- Mode: Shell-script; tab-width: 4; indent-tabs-mode: nil; -*-

TEST_DIR=${TEST_DIR:-/work/mozilla/mozilla.com/test.mozilla.com/www}
TEST_BIN=${TEST_BIN:-$TEST_DIR/bin}
source ${TEST_BIN}/library.sh

TEST_JSSHELL_TIMEOUT=${TEST_JSSHELL_TIMEOUT:-240}

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

TEST_HTML_LOG="${TEST_DIR}/results/mozilla.org/js/${TEST_DATE},js,$branch,$buildtype,$OSID,${MACHINE},$TEST_ID-shell.html"

time perl jsDriver.pl \
    -l $included \
	-L excluded-n.tests \
	-s $executable -e sm$buildtype \
	-o '-S 524288' \
	-K \
	-T $TEST_JSSHELL_TIMEOUT \
	-f $TEST_HTML_LOG \
    -Q

