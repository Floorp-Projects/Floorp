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
# This script will generate a single Makefile from a Makefile.in using
# the config.status script.
#
# The config.status script is generated the first time you run 
# ./configure.
#
#
# Usage: update-makefile.sh
#
# Send comments, improvements, bugs to ramiro@netscape.com
# 

root_path=`pwd`

# Make sure a Makefile exists
basemakefile=$root_path/Makefile

if [ -f $basemakefile ]
then
    makefile=$basemakefile
elif [ -f $root_path/Makefile.in ]
then
    makefile=$root_path/Makefile.in
else
	echo
	echo "There ain't no 'Makefile' or 'Makefile.in' over here: $pwd"
	echo

	exit
fi

# Use DEPTH in the Makefile to determine the depth
depth=`egrep '^DEPTH[ 	]*=[ 	]*\.' $makefile | awk -F= '{ print $2; }'`

# 'cd' to the root of the tree
cd $depth

root_path=`pwd`
# Strip the tree root off the Makefile's path
basemakefile=`expr $basemakefile : $root_path'/\(.*\)'`

# Make sure config.status exists
if [ -f config.status ]
then
	CONFIG_FILES=$basemakefile ./config.status
else
	echo
	echo "There ain't no 'config.status' over here: $pwd"
	echo
fi
