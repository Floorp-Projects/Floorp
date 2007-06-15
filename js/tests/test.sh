#!/usr/local/bin/bash
# -*- Mode: Shell-script; tab-width: 4; indent-tabs-mode: nil; -*-

TEST_DIR=${TEST_DIR:-/work/mozilla/mozilla.com/test.mozilla.com/www}
TEST_BIN=${TEST_BIN:-$TEST_DIR/bin}
source ${TEST_BIN}/library.sh

#
# options processing
#
options=":d:"
usage()
{
    cat <<EOF
usage: $SCRIPT -d datafiles

This script is used to dispatch to either test-browser.sh
for browser based JavaScript tests or test-shell.js for 
shell based JavaScript tests. It ignores all input arguments
except -d datafiles which it uses to pass data to the 
appropriate script.

variable            description
===============     ============================================================
-d datafiles        optional. one or more filenames of files containing 
                    environment variable definitions to be included.
if an argument contains more than one value, it must be quoted.
EOF
    exit 2
}

unset datafiles

while getopts $options optname ; 
do 
    case $optname in
        d) datafiles=$OPTARG;;
    esac
done

if [[ -z "$datafiles" ]]; then
    usage
fi

for data in $datafiles; do
    source $data
done

case "$product" in
    firefox) testscript=./test-browser.sh;;
    js) testscript=./test-shell.sh;;
    *) echo "unknown product [$product]"
        exit 2
        ;;
esac

$testscript -d "$datafiles"