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


# The way this thing works:
#
# + a phony $(DIST) is created: /tmp/foo
#
# + The module is instaled there so that it can be isolated and
#   catalogued.
#
# + A bunch of case statements determine what gets ignored
#   and what is otherwise echoed as a file or dir member for
#   the module
#
# + Most of this hacks are a result of the totally messed
#   way in which mozilla pretentsto do "make install"

here=`pwd`

dist=/tmp/dist-$$.tmp

raw_file_list=/tmp/raw-file-list-$$.txt
file_list=/tmp/file-list-$$.txt

raw_dir_list=/tmp/raw-dir-list-$$.txt
dir_list=/tmp/dir-list-$$.txt

rm -rf $dist $raw_file_list $file_list $raw_dir_list $dir_list

# Need to mkdir include or else "make export" in mozilla gets confused
mkdir -p $dist
mkdir -p $dist/include

make -s DIST=$dist XPDIST=$dist PUBLIC=$dist/include EXTRA_DEPS= >/dev/null 2>&1

cd $dist

find -type l | cut -b3- > $raw_file_list
find -type d | cut -b3- | grep -v -e "^[ \t]*$" > $raw_dir_list

touch $file_list

for i in `cat $raw_file_list`
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
		echo $i >> $file_list
#	else
#		echo "skipping $i"
	fi
done

touch $dir_list

for i in `cat $raw_dir_list`
do
	skip="false"

	# Skip directories that are shared across all of mozilla's components
	case $i in
		# level 1
		include|idl|lib|bin)
			skip="true"
		;;

		# level 2
		lib/components|bin/components|bin/chrome|bin/res|bin/defaults|bin/plugins)
			skip="true"
		;;

		# level 3
		bin/defaults/pref)
			skip="true"
		;;
	esac

	if [ "$skip" != "true" ]
	then
		echo DIR:$i >> $dir_list
#	else
#		echo "skipping $i"
	fi
done

cat $file_list
cat $dir_list

rm -rf $dist $raw_file_list $file_list $raw_dir_list $dir_list

cd $here

