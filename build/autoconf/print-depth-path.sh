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
# This script will print the depth path for a mozilla directory based
# on the info in Makefile.in
#
# Its a hack.  Its brute force.  Its horrible.  
# It dont use Artificial Intelligence.  It dont use Virtual Reality.
# Its not perl.  Its not python.   But it works.
#
# Usage: print-depth-path.sh
#
# Send comments, improvements, bugs to ramiro@netscape.com
# 

# Make sure a Makefile.in exists
if [ ! -f Makefile.in ]
then
	echo
	echo "There ain't no 'Makefile.in' over here: $pwd, dude."
	echo

	exit
fi

# Use DEPTH in the Makefile.in to determine the depth
depth=`grep -w DEPTH Makefile.in  | grep -e "\.\." | awk -F"=" '{ print $2; }'`

# Determine the depth count
n=`echo $depth | tr '/' ' ' | wc -w`

# Determine the path (strip anything before the mozilla/ root)
path=`pwd | awk -v count=$n -F"/" '\
{ for(i=NF-count+0; i <= NF ; i++) \
{ \
if (i!=NF) \
  { printf "%s/", $i } \
else \
  { printf "%s", $i } \
} \
}'`

echo $path
