#!/bin/sh
#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "NPL"); you may not use this file except in
# compliance with the NPL.  You may obtain a copy of the NPL at
# http://www.mozilla.org/NPL/
#
# Software distributed under the NPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
# for the specific language governing rights and limitations under the
# NPL.
#
# The Initial Developer of this code under the NPL is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
# Reserved.
#
#

here=`pwd`

dist=/tmp/dist-$$.tmp
list=/tmp/list-$$.txt
list2=/tmp/list2-$$.txt

rm -rf $dist $list $list2

make -s DIST=$dist XPDIST=$dist PUBLIC=$dist/include EXTRA_DEPS= >/dev/null 2>&1

cd $dist

find -type l | cut -b3- > $list

touch $list2

for i in `cat $list`
do
	skip="false"

	dir=`echo $i | awk -F"/" '{ print $1; }'`

	# "lib" voodoo
	if [ "$dir" = "lib" ]
	then
		rest=`echo $i | cut -b5-`

		bin_dup="bin/$rest"

		# Dont output files in "lib/" that are duplicated in "bin/"
		if [ -f "$bin_dup" ]
		then
			#echo "skipping $i"
			skip="true"
		fi


		# Test for files that need to be skipped
		file=`basename $i`

		case "$file"
		in
			# Dont skip util .a files
			*util_s.a)
				:
			;;

			# Dont skip nspr .a files
			libnspr*.a|libplc*|libplds*)
				:
			;;

			# Skip all .a files
			*.a)
				skip="true"
			;;

			# Skip .so files
			*.so)
				skip="true"
			;;
		esac
	fi

	if [ "$skip" != "true" ]
	then
		echo $i >> $list2
	fi
done

cat $list2

#rm -rf $dist $list $list2

cd $here

