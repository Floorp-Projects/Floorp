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

update_makefile_usage() {
  _progname=`expr //$0 : '.*/\(.*\)'`
  cat <<END_USAGE 2>&1

Usage: $_progname [-h -u] [<keyword>]
     -d <dir>     Subdir to update
     -h           Print usage
END_USAGE
}

# Parse the command-line options
#
subdir=
while getopts d:h OPT; do
    case $OPT in
       d) # Make sure "subdir" has exactly one ending slash
          subdir=`echo $OPTARG | sed 's/\/$//;'`"/" ;;
    \?|h) update_makefile_usage
          exit 1
          ;;
    esac
done


# find_depth: Pull the value of DEPTH out of Makefile (or Makefile.in)
find_depth() {
  egrep '^DEPTH[ 	]*=[ 	]*\.' $1 | awk -F= '{ print $2; }'
}

# The Makefile to create
target_makefile=`pwd`"/${subdir}Makefile"

# Use $(DEPTH) in the Makefile or Makefile.in to determine the depth
if [ -f Makefile.in ]
then
    depth=`find_depth Makefile.in`
elif [ -f Makefile ]
then
    depth=`find_depth Makefile`
elif [ -f ../Makefile ]
then
    depth="../"`find_depth Makefile`
else
    echo
    echo "There ain't no 'Makefile' or 'Makefile.in' over here: $pwd"
    echo
    exit
fi

# 'cd' to the root of the tree to run "config.status" there
cd $depth

# Strip the tree root off the Makefile's path
#
root_path=`pwd`
target_makefile=`expr $target_makefile : $root_path'/\(.*\)'`

# Make sure config.status exists
#
if [ -f config.status ]
then
    CONFIG_FILES=$target_makefile ./config.status
else
    echo
    echo "There ain't no 'config.status' over here: $pwd"
    echo
fi
