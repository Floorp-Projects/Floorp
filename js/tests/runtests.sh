#!/bin/bash -e
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

usage()
{
    cat <<EOF
usage: runtests.sh -p products -b branches -T  buildtypes -B buildcommands  [-e extra]
usage: runtests.sh -p "$products" -b "$branches" -T  "$buildtypes" -B "$buildcommands" -e "$extra"

variable            description
===============     ============================================================
-p products         space separated list of js, firefox
-b branches         space separated list of branches 1.8.1, 1.9.0
-T buildtypes       space separated list of build types opt debug
-e extra            optional. extra qualifier to pick build tree and mozconfig.
-B buildcommands    optional space separated list of build commands
                    clean, checkout, build. If not specified, defaults to
                    'clean checkout build'. 
                    If you wish to skip any build steps, simply specify a value
                    not containing any of the build commands, e.g. 'none'.

if an argument contains more than one value, it must be quoted.
EOF
    exit 2
}

while getopts "p:b:T:B:e:" optname; 
  do
  case $optname in
      p) products=$OPTARG;;
      b) branches=$OPTARG;;
      T) buildtypes=$OPTARG;;
      e) extra="$OPTARG"
          extraflag="-e $OPTARG";;
      B) buildcommands=$OPTARG;;
  esac
done

if [[ -z "$products" || -z "$branches" || -z "$buildtypes" ]]; then
    usage
fi

if [[ -z "$buildcommands" ]]; then
    buildcommands="clean checkout build"
fi

case $buildtypes in
    nightly)
        buildtypes="nightly-$OSID"
        ;;
    opt|debug|opt*debug)
        if [[ -n "$buildcommands" ]]; then
            builder.sh -p "$products" -b "$branches" $extraflag -B "$buildcommands" -T "$buildtypes"
        fi
        ;;
esac

testlogfilelist=`mktemp /tmp/TESTLOGFILES.XXXX`

export testlogfiles
export testlogfile

if [[ -z "$extra" ]]; then
    if tester.sh -t $TEST_JSDIR/test.sh "$products" "$branches" "$buildtypes" | tee -a $testlogfilelist; then
        testlogfiles="`cat $testlogfilelist`"
    fi
else
    if tester.sh -t $TEST_JSDIR/test.sh "$products" "$branches" "$extra" "$buildtypes" | tee -a $testlogfilelist; then
        testlogfiles="`cat $testlogfilelist`"
    fi
fi

rm $testlogfilelist

case "$OSID" in
    win32)
        arch=all
        kernel=all
        ;;
    linux)
        arch="`uname -p`"
        kernel="`uname -r`"
        case "$kernel" in
            *el5PAE)
                kernel='.*el5PAE'
                ;;
            *ELsmp)
                kernel='.*ELsmp'
                ;;
            *fc*)
                kernel='.*fc.'
                ;;
            *)
                kernel='[.][*]'
                ;;
        esac
        ;;
    mac)
        arch="`uname -p`"
        kernel=all
        ;;
    *)
        error "$OSID not supported"
        ;;
esac

for testlogfile in $testlogfiles; do
    case "$testlogfile" in
        *,js,*) testtype=shell;;
        *,firefox,*) testtype=browser;;
        *) error "unknown testtype in logfile $testlogfile";;
    esac
    case "$testlogfile" in
        *,opt,*) buildtype=opt;;
        *,debug,*) buildtype=debug;;
        *) error "unknown buildtype in logfile $testlogfile";;
    esac
    case "$testlogfile" in
        *,1.8.1,*) branch=1.8.1;;
        *,1.9.0,*) branch=1.9.0;;
        *) error "unknown branch in logfile $testlogfile";;
    esac
    outputprefix=$testlogfile

    if [[ -n "$DEBUG" ]]; then
        dumpvars branch buildtype testtype OSID testlogfile arch kernel outputprefix
    fi
    $TEST_DIR/tests/mozilla.org/js/known-failures.pl -b $branch -T $buildtype -t $testtype -o "$OSID" -z `date +%z` -l $testlogfile -A "$arch" -K "$kernel" -r $TEST_JSDIR/failures.txt -O $outputprefix
done


