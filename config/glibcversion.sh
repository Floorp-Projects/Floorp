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

##############################################################################
##
## Name:		glibcversion.sh - Print __GLIBC__ version if gnu libc 2 is 
##              found.
##
## Description:	This script is needed by the mozilla build system.  It needs
##              to determine whether the current platform (mostly the 
##              various linux "platforms") are based on the gnu libc2.  This
##              information is later used in mozilla to determine whether 
##              gnu libc 2 specific "features" need to be handled, such
##              as broken locales.
##
## Author:		Ramiro Estrugo <ramiro@netscape.com>
##
##############################################################################

##
## Command Line Flags Supported:
##
##  -g  | --is-glibc2:				Print True/False if detected __GLIBC__.
##
##  -v  | --print-version:			Print value of __GLIBC__ if found, or none.
##
##  -o  | --set-object-name:		Set object name for current system.
##  -cc | --set-compiler:			Set compiler for building test program.
##


##
## Constants
##
GLIBC_PROG_PREFIX=./get_glibc_info

##
## Defaults
##
GLIBC_PRINT_IS_GLIBC2=False

GLIBC_PRINT_VERSION=False

GLIBC_OBJECT_NAME=`uname`-`uname -r`
GLIBC_CC=cc

function glibc_usage()
{
echo
echo "Usage:   `basename $0` [options]"
echo
echo "  -g,  --is-glibc2:          Print True/False if detected __GLIBC__."
echo
echo "  -v,  --print-version:      Print value of __GLIBC__ if found, or none."
echo
echo "  -o,  --set-object-name:    Set object name for current system."
echo "  -cc, --set-compiler:       Set compiler for building test program."
echo
echo "  -h,  --help:               Print this blurb."
echo
echo "The default is '-v' if no options are given."
echo
}

##
## Parse the command line
##
while [ "$*" ]; do
    case $1 in
        -h | --help)
            shift
            glibc_usage
			exit 0
            ;;

        -g | --is-glibc2)
            shift
            GLIBC_PRINT_IS_GLIBC2=True
            ;;

        -v | --print-version)
            shift
            GLIBC_PRINT_VERSION=True
            ;;

        -o | --set-object-name)
            shift
            GLIBC_OBJECT_NAME="$1"
            shift
            ;;

        -cc | --set-compiler)
            shift
            GLIBC_CC="$1"
            shift
            ;;

        -*)
            echo "`basename $0`: invalid option '$1'"
            shift
            glibc_usage
			exit 0
            ;;
    esac
done

##
## Motif info program name
##
GLIBC_PROG="$GLIBC_PROG_PREFIX"_"$GLIBC_OBJECT_NAME"
GLIBC_SRC="$GLIBC_PROG_PREFIX"_"$GLIBC_OBJECT_NAME.c"

##
## Cleanup the dummy test source/program
##
function glibc_cleanup()
{
	true

#	rm -f $GLIBC_PROG
#	rm -f $GLIBC_SRC

}

glibc_cleanup

if [ ! -f $GLIBC_SRC ]
then
cat << EOF > $GLIBC_SRC
#include <stdio.h>

int main(int argc,char ** argv) 
{
#ifdef 	__GLIBC__
	fprintf(stdout,"%d\n",__GLIBC__);
#else
	fprintf(stdout,"none\n");
#endif

	return 0;
}
EOF
fi

if [ ! -f $GLIBC_SRC ]
then
	echo
	echo "Could not create test program source $GLIBC_SRC."
	echo

	glibc_cleanup

	exit
fi

##
## Compile the dummy test program if needed
##
if [ ! -x $GLIBC_PROG ]
then
	$GLIBC_CC -o $GLIBC_PROG $GLIBC_SRC
fi

if [ ! -x $GLIBC_PROG ]
then
	echo
	echo "Could not create test program $GLIBC_PROG."
	echo

	glibc_cleanup

	exit
fi

##
## Execute the dummy test program
##
GLIBC_PROG_OUTPUT=`$GLIBC_PROG`

##
## -g | --is-glibc2
##
if [ "$GLIBC_PRINT_IS_GLIBC2" = "True" ]
then
	if [ "$GLIBC_PROG_OUTPUT" = "2" ]
	then
		echo True
	else
		echo False
	fi

	glibc_cleanup

	exit 0
fi

echo $GLIBC_PROG_OUTPUT

glibc_cleanup
