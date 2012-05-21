#!/bin/sh
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# This script will match a dir with a set of dirs.
#
# Usage: match-dir.sh match [dir1 dir2 ... dirn]
#
# Send comments, improvements, bugs to ramiro@netscape.com
# 

if [ -f Makefile ]; then
	MAKEFILE="Makefile"
else
	if [ -f Makefile.in ]; then
		MAKEFILE="Makefile.in"
	else
		echo
		echo "There ain't no 'Makefile' or 'Makefile.in' over here: $pwd, dude."
		echo
		exit 1
	fi
fi

# Use DEPTH in the Makefile.in to determine the depth
depth=`grep -w DEPTH ${MAKEFILE}  | grep "\.\." | awk -F"=" '{ print $2; }'`
cwd=`pwd`

# Determine the depth count
n=`echo $depth | tr '/' ' ' | wc -w`

cd $depth
objdir=`pwd`

path=`echo $cwd | sed "s|^${objdir}/||"`

match=$path

for i in $*
do
#	echo "Looking for $match in $i"

	echo $i | grep -q -x $match

	if [ $? -eq 0 ]
	then
		echo "1"

		exit 0
	fi

#	echo "Looking for $i in $match"

	echo $match | grep -q $i

	if [ $? -eq 0 ]
	then
		echo "1"

		exit 0
	fi
done

echo "0"

exit 0
