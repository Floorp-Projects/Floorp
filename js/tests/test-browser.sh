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

if [[ -z "$TEST_DIR" ]]; then
  cat <<EOF
`basename $0`: error

TEST_DIR, the location of the Sisyphus framework, 
is required to be set prior to calling this script.
EOF
  exit 2
fi

if [[ ! -e $TEST_DIR/bin/library.sh ]]; then
    echo "TEST_DIR=$TEST_DIR"
    echo ""
    echo "This script requires the Sisyphus testing framework. Please "
    echo "cvs check out the Sisyphys framework from mozilla/testing/sisyphus"
    echo "and set the environment variable TEST_DIR to the directory where it"
    echo "located."
    echo ""

    exit 2
fi

source $TEST_DIR/bin/library.sh

TEST_JSDIR=`dirname $0`

TEST_JSEACH_TIMEOUT=${TEST_JSEACH_TIMEOUT:-485}
TEST_JSEACH_PAGE_TIMEOUT=${TEST_JSEACH_PAGE_TIMEOUT:-480}
TEST_JSALL_TIMEOUT=${TEST_JSALL_TIMEOUT:-21600}
TEST_WWW_JS=`echo $TEST_JSDIR|sed "s|$TEST_DIR||"`

#
# options processing
#
usage()
{
    cat <<EOF
usage: $SCRIPT -p product -b branch -T buildtype -x executablepath -N profilename \\
               -R -X exclude -I include -c -t -F [-d datafiles]

variable            description
===============     ============================================================
-p product          required. firefox|thunderbird
-b branch           required. 1.8.0|1.8.1|1.9.0
-T buildtype        required. one of opt debug
-x executablepath   required. directory-tree containing executable 'product'
-N profilename      required. profile name 
-R                  optional. by default the browser test will start Firefox
                    spider and execute the tests one after another in the same
                    process. -R will start an new instance of Firefox for each
                    test.
-X exclude          optional. By default the test will exclude the 
                    tests listed in spidermonkey-n-\$branch.tests, 
                    performance-\$branch.tests. exclude is a list of either
                    individual tests, manifest files or sub-directories which 
                    will override the default exclusion list.
-I include          optional. By default the test will include the 
                    JavaScript tests appropriate for the branch. include is a
                    list of either individual tests, manifest files or 
                    sub-directories which will override the default inclusion 
                    list.
-Z n                Set gczeal to n. Only valid for Gecko 1.9.0 and later.
-c                  optional. By default the test will exclude tests 
                    which crash on this branch, test type, build type and 
                    operating system. -c will include tests which crash. 
                    Typically this should only be used in combination with -R. 
                    This has no effect on shell based tests which execute crash
                    tests regardless.
-t                  optional. By default the test will exclude tests 
                    which time out on this branch, test type, build type and 
                    operating system. -t will include tests which timeout.
-F                  optional. Just generate file lists without running any tests.
-d datafiles        optional. one or more filenames of files containing 
                    environment variable definitions to be included.

                    note that the environment variables should have the same 
                    names as in the "variable" column.

if an argument contains more than one value, it must be quoted.
EOF
    exit 2
}

while getopts "p:b:T:x:N:d:X:I:Z:RctF" optname
do 
    case $optname in
        p) product=$OPTARG;;
        b) branch=$OPTARG;;
        T) buildtype=$OPTARG;;
        N) profilename=$OPTARG;;
        x) executablepath=$OPTARG;;
        R) restart=1;;
        X) exclude=$OPTARG;;
        I) include=$OPTARG;;
        c) crashes=1;;
        t) timeouts=1;;
        F) filesonly=1;;
        Z) gczeal=";gczeal=$OPTARG";;
        d) datafiles=$OPTARG;;
    esac
done

# include environment variables
if [[ -n "$datafiles" ]]; then
    for datafile in $datafiles; do 
        source $datafile
    done
fi

dumpvars product branch buildtype profilename executablepath restart exclude include crashes timeouts filesonly gczeal datafiles | sed "s|^|arguments: |"

if [[ -z "$product" || -z "$branch" || -z "$executablepath" || -z "$profilename" ]]; then
    usage
fi

executable=`get_executable $product $branch $executablepath`

pushd $TEST_JSDIR

rm -f finished-$branch-browser-$buildtype

if ! make failures.txt; then
    error "during make failures.txt" $LINENO
fi

includetests="included-$branch-browser-$buildtype.tests"
rm -f $includetests
touch $includetests

if [[ -z "$include" ]]; then
    # by default include tests appropriate for the branch
    include="e4x ecma ecma_2 ecma_3 js1_1 js1_2 js1_3 js1_4 js1_5 js1_6"

    case "$branch" in 
        1.8.0)
            ;;
        1.8.1)
            include="$include js1_7"
            ;;
        1.9.0)
            include="$include js1_7 js1_8"
            ;;
    esac
fi

for i in $include; do
    if [[ -f "$i" ]]; then
        echo "# including $i" >> $includetests
        if echo $i | grep -q '\.js$'; then
            echo $i >> $includetests
        else
            cat $i >> $includetests
        fi
    elif [[ -d "$i" ]]; then
        find $i -name '*.js' -print | egrep -v '(shell|browser|template|jsref|userhook.*|\.#.*)\.js' | sed 's/^\.\///' | sort >> $includetests
    fi
done

excludetests="excluded-$branch-browser-$buildtype.tests"
rm -f $excludetests
touch $excludetests

if [[ -z "$exclude" ]]; then
    exclude="spidermonkey-n-$branch.tests performance-$branch.tests"
fi

for e in $exclude; do
    if [[ -f "$e" ]]; then
        echo "# excluding $e" >> $excludetests
        if echo $e | grep -q '\.js$'; then
            echo $e >> $excludetests
        else
            cat $e >> $excludetests
        fi
    elif [[ -d "$e" ]]; then
        find $e -name '*.js' -print | egrep -v '(shell|browser|template|jsref|userhook.*|\.#.*)\.js' | sed 's/^\.\///' | sort >> $excludetests
    fi
done

case "$OSID" in
    win32)
        arch='.*'
        kernel='.*'
        ;;
    linux)
        arch="`uname -p`"
        kernel="`uname -r | sed 's|\([0-9]*\)\.\([0-9]*\)\.\([0-9]*\)[-.0-9]*\.\([a-zA-Z0-9]*\)|\1.\2.\3.*\4|'`"
        ;;
    mac)
        arch="`uname -p`"
        kernel='[^,]*'
        ;;
    *)
        error "$OSID not supported" $LINENO
        ;;
esac

if [[ -z "$timeouts" ]]; then
    echo "# exclude tests that time out" >> $excludetests
    egrep "TEST_BRANCH=([^,]*$branch[^,]*|[.][*]), TEST_RESULT=FAILED, TEST_BUILDTYPE=([^,]*$buildtype[^,]*|[.][*]), TEST_TYPE=([^,]*browser[^,]*|[.][*]), TEST_OS=([^,]*$OSID[^,]*|[.][*]), .*, TEST_PROCESSORTYPE=([^,]*$arch[^,]*|[.][*]), TEST_KERNEL=([^,]*$kernel[^,]*|[.][*]), .*, TEST_DESCRIPTION=.*EXIT STATUS: TIMED OUT" \
        failures.txt  | sed 's/TEST_ID=\([^,]*\),.*/\1/' | sort | uniq >> $excludetests
fi

if [[ -z "$crashes" ]]; then
    echo "# exclude tests that crash" >> $excludetests
    pattern="TEST_BRANCH=([^,]*$branch[^,]*|[.][*]), TEST_RESULT=FAILED, TEST_BUILDTYPE=([^,]*$buildtype[^,]*|[.][*]), TEST_TYPE=([^,]*browser[^,]*|[.][*]), TEST_OS=([^,]*$OSID[^,]*|[.][*]), .*, TEST_PROCESSORTYPE=([^,]*$arch[^,]*|[.][*]), TEST_KERNEL=([^,]*$kernel[^,]*|[.][*]), .*, TEST_DESCRIPTION=.*"
    case "$buildtype" in
        opt)
            pattern="${pattern}EXIT STATUS: CRASHED"
            ;;
        debug)
            pattern="${pattern}(EXIT STATUS: CRASHED|Assertion failure:)"
            ;;
    esac
    egrep "$pattern" failures.txt  | sed 's/TEST_ID=\([^,]*\),.*/\1/' | sort | uniq >> $excludetests

fi

urllist="urllist-$branch-browser-$buildtype.tests"
urlhtml="urllist-$branch-browser-$buildtype.html"

rm -f $urllist $urlhtml

cat > $urlhtml <<EOF
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<title>JavaScript Tests</title>
</head>
<body>
<ul>
EOF

cat $includetests | while read jsfile
do
    if echo $jsfile | grep -q '^#'; then
        continue
    fi

    if ! grep -q $jsfile $excludetests; then

        result=`echo $jsfile | sed 's/.*js\([0-9]\)_\([0-9]\).*/\1.\2/'`

        case $result in
            1.5) version=";version=1.5";;
            1.6) version=";version=1.6";;
            1.7) version=";version=1.7";;
            1.8) version=";version=1.8";;
            1.9) version=";version=1.9";;
            2.0) version=";version=2.0";;
            *) version="";;
        esac
        
        echo "http://$TEST_HTTP/$TEST_WWW_JS/js-test-driver-standards.html?test=$jsfile;language=type;text/javascript$version$gczeal" >> $urllist
        echo "<li><a href='http://$TEST_HTTP/$TEST_WWW_JS/js-test-driver-standards.html?test=$jsfile;language=type;text/javascript$version$gczeal'>$jsfile</a></li>" >> $urlhtml
    fi
done

cat >> $urlhtml <<EOF
</ul>
</body>
</html>
EOF

chmod a+r $urlhtml

cat $includetests | sed 's|^|include: |'
cat $excludetests | sed 's|^|exclude: |'

if [[ -z "$filesonly" ]]; then
    if [[ "$restart" == "1" ]]; then
        cat "$urllist" | while read url; 
        do 
	        edit-talkback.sh -p "$product" -b "$branch" -x "$executablepath" -i "$url"
	        if time timed_run.py $TEST_JSEACH_TIMEOUT "$url" \
		        "$executable" -P "$profilename" \
		        -spider -start -quit \
		        -uri "$url" \
		        -depth 0 -timeout "$TEST_JSEACH_PAGE_TIMEOUT" \
		        -hook "http://$TEST_HTTP/$TEST_WWW_JS/userhookeach.js"; then
                true;
            fi

        done
    else
	    edit-talkback.sh -p "$product" -b "$branch" -x "$executablepath" -i "http://$TEST_HTTP/$TEST_WWW_JS/$urlhtml"
	    if ! time timed_run.py $TEST_JSALL_TIMEOUT "http://$TEST_HTTP/$TEST_WWW_JS/$urlhtml" \
		    "$executable" -P "$profilename" \
		    -spider -start -quit \
		    -uri "http://$TEST_HTTP/$TEST_WWW_JS/$urlhtml" \
		    -depth 1 -timeout "$TEST_JSEACH_PAGE_TIMEOUT" \
		    -hook "http://$TEST_HTTP/$TEST_WWW_JS/userhookeach.js"; then
            error "timed_run.py ended abnormally: $?" $LINENO
        fi
    fi
fi

popd
