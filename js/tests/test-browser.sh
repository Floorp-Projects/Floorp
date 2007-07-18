#!/usr/local/bin/bash
# -*- Mode: Shell-script; tab-width: 4; indent-tabs-mode: nil; -*-

TEST_DIR=${TEST_DIR:-/work/mozilla/mozilla.com/test.mozilla.com/www}
TEST_BIN=${TEST_BIN:-$TEST_DIR/bin}
source ${TEST_BIN}/library.sh

TEST_JSDIR=${TEST_JSDIR:-$TEST_DIR/tests/mozilla.org/js}

TEST_JSEACH_TIMEOUT=${TEST_JSEACH_TIMEOUT:-240}
TEST_JSEACH_PAGE_TIMEOUT=${TEST_JSEACH_PAGE_TIMEOUT:-240}

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
