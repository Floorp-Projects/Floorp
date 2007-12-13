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
TEST_JSDIR=${TEST_JSDIR:-${TEST_DIR}/tests/mozilla.org/js}

if [[ ! -e $TEST_BIN/library.sh ]]; then
    echo "TEST_DIR=$TEST_DIR"
    echo ""
    echo "This script requires the Sisyphus testing framework. Please "
    echo "cvs check out the Sisyphys framework from mozilla/testing/sisyphus"
    echo "and set the environment variable TEST_DIR to the directory where it"
    echo "located."
    echo ""

    exit 2
fi

source $TEST_BIN/library.sh

usage()
{
    cat <<EOF
usage: process-logs.sh.sh -l testlogfiles -A arch -K kernel
usage: process-logs.sh.sh -l $testlogfiles -A $arch -K $kernel

variable            description
===============     ============================================================
testlogfiles        The test log to be processed. If testlogfiles is a file 
                    pattern it must be single quoted to prevent the shell from
                    expanding it before it is passed to the script.

arch                optional. The machine architecture as specified by uname -p
                    If not specified, the script will attempt to determine the 
                    value from the TEST_PROCESSORTYPE line in each log.
                    'all'     - do not filter on machine architecture. Use this 
                                for Windows.
                    'i686'    - Linux distros such as Fedora Core or RHEL or CentOS.
                    'i386'    - Mac Intel
                    'powerpc' - Mac PowerPC

kernel              optional. The machine kernel as specified by uname -r
                    If not specified, the script will attempt to determine the 
                    value from the TEST_KERNEL line in the log. 
                    'all'     - do not filter on machine kernel. Use this for
                                Windows.
                    For Linux distros, use the value of uname -r 
                    and replace the minor version numbers with .* as in 
                    2.6.23.1-21.fc7 -> 2.6.23.*fc7
EOF
    exit 2
}

while getopts "l:A:K:" optname; 
  do
  case $optname in
      l) testlogfiles=$OPTARG;;
      A) optarch=$OPTARG;;
      K) optkernel=$OPTARG;;
  esac
done

if [[ -z "$testlogfiles" ]]; then
    usage
fi

for testlogfile in `ls $testlogfiles`; do

    case "$testlogfile" in
        *,js,*) testtype=shell;;
        *,firefox,*) testtype=browser;;
        *) error "unknown testtype in logfile $testlogfile";;
    esac

    case "$testlogfile" in
        *,nightly,*) buildtype=nightly;;
        *,opt,*) buildtype=opt;;
        *,debug,*) buildtype=debug;;
        *) error "unknown buildtype in logfile $testlogfile";;
    esac

    case "$testlogfile" in
        *,1.8.0*) branch=1.8.0;;
        *,1.8.1*) branch=1.8.1;;
        *,1.9.0*) branch=1.9.0;;
        *) 
            branch=`grep TEST_BRANCH= $testlogfile | sed 's|.*TEST_BRANCH=\(.*\)|\1|'`
            if [[ -z "$branch" ]]; then
                error "unknown branch in logfile $testlogfile"
            fi
            ;;
    esac

    case "$testlogfile" in 
        *,win32,*) OSID=win32;;
        *,linux,*) OSID=linux;;
        *,mac,*) OSID=mac;;
        *) 
            OSID=`grep OSID= $testlogfile | sed 's|.*OSID=\(.*\)|\1|'`
            if [[ -z "$OSID" ]]; then
                error "unknown OS in logfile $testlogfile"
            fi
            ;;
    esac

    if [[ -n "$optkernel" ]]; then
        kernel="$optkernel"
    else
        if [[ "$OSID" == "win32" ]]; then
            kernel=all
        else
            kernel=`grep TEST_KERNEL $testlogfile | sed 's|.*TEST_KERNEL=\(.*\)|\1|'`
            kernel=`echo $kernel | sed 's|\([0-9]*\)\.\([0-9]*\)\.\([0-9]*\)[-.0-9]*\.\([a-zA-Z0-9]*\)|\1.\2.\3.*\4|'`
        fi
    fi

    if [[ -n "$optarch" ]]; then
        arch="$optarch"
    else
        if [[ "$OSID" == "win32" ]]; then
            arch=all
        else
            arch=`grep TEST_PROCESSORTYPE $testlogfile | sed 's|.*TEST_PROCESSORTYPE=\(.*\)|\1|'`
        fi
    fi

    timezone=`echo $testlogfile | sed 's|^[-0-9]*\([-+]\)\([0-9]\{4,4\}\),.*|\1\2|'`

    outputprefix=$testlogfile

    if [[ -n "$DEBUG" ]]; then
        dumpvars branch buildtype testtype OSID timezone testlogfile arch kernel 
    fi

    $TEST_DIR/tests/mozilla.org/js/known-failures.pl -b "$branch" -T "$buildtype" -t "$testtype" -o "$OSID" -z "$timezone" -l "$testlogfile" -A "$arch" -K "$kernel" -r "$TEST_JSDIR/failures.txt" -O "$outputprefix"

done
