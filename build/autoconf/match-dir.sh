#!/bin/sh
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 
#

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
