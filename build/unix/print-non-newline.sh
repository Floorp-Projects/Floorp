#!/bin/sh
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is test-ending-newline.sh
#
# The Initial Developer of the Original Code is Christopher
# Seawood <cls@seawood.org>. Portions created by Christopher Seawood 
# are Copyright (C) 2000 Christopher Seawood. All Rights Reserved.

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
