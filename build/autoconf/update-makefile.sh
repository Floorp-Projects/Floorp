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

# Make sure a Makefile.in exists
if [ ! -f Makefile.in ]
then
	echo
	echo "There ain't no 'Makefile.in' over here: $pwd"
	echo

	exit
fi

# Use DEPTH in the Makefile.in to determine the depth
depth=`grep -w DEPTH Makefile.in  | grep -e "\.\." | awk -F"=" '{ print $2; }'`

# Determine the depth count
n=`echo $depth | tr '/' ' ' | wc -w`

# Determine the path (strip anything before the mozilla/ root)
path=`pwd | awk -v count=$n -F"/" '\
{ for(i=NF-count+1; i <= NF ; i++) \
{ \
if (i!=NF) \
  { printf "%s/", $i } \
else \
  { printf "%s", $i } \
} \
}'`

dir=$path

# Add a slash only to dirs where depth >= mozilla_root
if [ $n -gt 0 ]
then
	dir=${dir}"/"
fi

back=`pwd`

makefile=${dir}"Makefile"

cd $depth

# Make sure config.status exists
if [ -f config.status ]
else
	CONFIG_FILES=$makefile ./config.status
then
	echo
	echo "There ain't no 'config.status' over here: $pwd"
	echo
fi

cd $back
