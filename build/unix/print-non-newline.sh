#!/bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# The purpose of this file is to find the files that do not end with a
# newline.  Some compilers fail if the source files do not end with a
# newline.
#

#
test_file=newline_test
test_dummy=newline_testee
inlist="$*"
broken_list=

if test "$inlist" = ""; then
    echo "Usage: $0 *.c *.cpp";
    exit 0;
fi

echo "" > $test_file

for f in $inlist; do
    if test -f $f; then
	tail -c 1 $f > $test_dummy
	if ! `cmp -s $test_file $test_dummy`; then
	    broken_list="$broken_list $f"
        fi
    fi
done

rm -f $test_file $test_dummy
echo $broken_list
