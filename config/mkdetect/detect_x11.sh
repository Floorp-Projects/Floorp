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

##############################################################################
##
## Name: detect_x11.sh - Get X11 lib location, version and other info.
##
## Description:	Artificial intelligence to figure out:
##
##              + Where the X11 headers/lib are located.
##              + The version of X11 being used.
##              + The compile and link flags needed to build X11 apps.
##
## Author: Ramiro Estrugo <ramiro@netscape.com>
##
##############################################################################


## This script looks in the space sepeared list of directories 
## (X11_SEARCH_PATH) for x11 headers and libraries.
##
## The search path can be overrided by the user by setting:
##
## MOZILLA_X11_SEARCH_PATH
##
## To a space delimeted list of directories to search for x11 paths.
##
## For example, if you have many different versions of x11 installed,
## and you want the current build to use headers in /foo/x11/include
## and libraries in /foo/x11/lib, then do this:
##
## export MOZILLA_X11_SEARCH_PATH="/foo/x11"
##
## The script also generates and builds a program 'detect_x11-OBJECT_NAME'
## which prints out info on the x11 detected on the system.
##
## This information is munged into useful strings that can be printed
## through the various command line flags decribed bellow.  This script
## can be invoked from Makefiles or other scripts in order to set
## flags needed to build x11 program.
##
## The get 'detect_x11-OBJECT_NAME' program is generated/built only
## once and reused.  Because of the OBJECT_NAME suffix, it will work on
## multiple platforms at the same time.
##
## The generated files can be wiped by the --cleanup flag.
##

##
## Command Line Flags Supported:
##
##  -v | --print-version:		Print XlibSpecificationRelease.
##  -r | --print-revision:		Print XmREVISION.
##  -u | --print-update-level:		Print XmUPDATE_LEVEL.
##  -s | --print-version-string:	Print XmVERSION_STRING.
##
##  -id | --print-include-dir:		Print dir of x11 includes.
##  -sd | --print-static-dir:		Print dir of x11 static libs.
##  -dd | --print-dynamic-dir:		Print dir of x11 dynamic libs.
##
##  -sl | --print-static-lib:		Print static lib.
##  -dl | --print-dynamic-lib:		Print dynamic lib.
##
##  -if | --print-include-flags:	Print cc flags needed to build x11 apps.
##  -sf | --print-static-flags:		Print ld flags for linking statically.
##  -df | --print-dynamic-flags:	Print ld flags for linking dynamically.
##  -dp | --print-dynamic-paths:	Print ld paths for linking dynamically.
##
##  -de | --dynamic-ext:		Set extension used on dynamic libs.
##  -se | --static-ext:			Set extension used on static libs.
##
##  -o  | --set-object-name:		Set object name for current system.
##  -cc | --set-compiler:		Set compiler for building test program.
##
##  -c | --cleanup:			Clean up any generated files.
##

##
## Look for x11 stuff in the following places:
##
DEFAULT_X11_SEARCH_PATH="\
/usr/local \
/usr/openwin \
/usr/X11R6 \
/usr \
"

##
## The user can override the default share path by setting MOZILLA_X11_SEARCH
##
if [ -n "$MOZILLA_X11_SEARCH_PATH" ]
then
    X11_SEARCH_PATH=$MOZILLA_X11_SEARCH_PATH
else
    X11_SEARCH_PATH=$DEFAULT_X11_SEARCH_PATH
fi

##
## Constants
##
X11_PROG_PREFIX=./detect_x11

##
## Defaults
##
X11_DYNAMIC_EXT=so
X11_STATIC_EXT=a

X11_PRINT_VERSION=False
X11_PRINT_REVISION=False
X11_PRINT_UPDATE_LEVEL=False

X11_PRINT_INCLUDE_DIR=False
X11_PRINT_STATIC_DIR=False
X11_PRINT_DYNAMIC_DIR=False

X11_PRINT_STATIC_LIB=False
X11_PRINT_DYNAMIC_LIB=False

X11_PRINT_INCLUDE_FLAGS=False
X11_PRINT_STATIC_FLAGS=False
X11_PRINT_DYNAMIC_FLAGS=False
X11_PRINT_DYNAMIC_PATHS=False

X11_PRINT_EVERYTHING=False

X11_OBJECT_NAME=`uname`-`uname -r`
X11_CC=cc

X11_CLEANUP=False

##
## Stuff we need to figure out
##
X11_VERSION_RESULT=unknown
X11_REVISION_RESULT=unknown
X11_UPDATE_RESULT=unknown
X11_VERSION_REVISION_RESULT=unknown
X11_VERSION_REVISION_UPDATE_RESULT=unknown
X11_VERSION_STRING_RESULT=unknown

X11_INCLUDE_DIR=unknown
X11_STATIC_LIB=unknown
X11_DYNAMIC_LIB=unknown

X11_STATIC_DIR=unknown
X11_DYNAMIC_DIR=unknown

X11_INCLUDE_FLAGS=unknown
X11_STATIC_FLAGS=unknown
X11_DYNAMIC_FLAGS=unknown
X11_DYNAMIC_PATHS=unknown

x11_usage()
{
echo
echo "Usage:   `basename $0` [options]"
echo
echo "  -v,  --print-version:         Print XlibSpecificationRelease."
echo "  -r,  --print-revision:        Print XmREVISION."
echo "  -u,  --print-update-level:    Print XmUPDATE_LEVEL."
echo "  -s,  --print-version-string:  Print XmVERSION_STRING."
echo
echo "  -id, --print-include-dir:     Print dir of x11 includes."
echo "  -sd, --print-static-dir:      Print dir of x11 static libs."
echo "  -dd, --print-dynamic-dir:     Print dir of x11 dynamic libs."
echo
echo "  -sl, --print-static-lib:      Print static lib."
echo "  -dl  --print-dynamic-lib:     Print dynamic lib."
echo
echo "  -if, --print-include-flags:   Print cc flags needed to compile."
echo "  -sf, --print-static-flags:    Print ld flags for linking statically."
echo "  -df, --print-dynamic-flags:   Print ld flags for linking dynamically."
echo "  -dp, --print-dynamic-paths:   Print ld paths for linking dynamically."
echo
echo "  -e,  --print-everything:      Print everything that is known."
echo
echo "  -de, --dynamic-ext:           Set extension used on dynamic libs."
echo "  -se, --static-ext:            Set extension used on static libs."
echo
echo "  -o,  --set-object-name:       Set object name for current system."
echo "  -cc, --set-compiler:          Set compiler for building test program."
echo
echo "  -h,  --help:                  Print this blurb."
echo
echo "  -c, --cleanup:                Clean up any generated files."
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
            x11_usage
			exit 0
            ;;

        -v | --print-version)
            shift
            X11_PRINT_VERSION=True
            ;;

        -r | --print-revision)
            shift
            X11_PRINT_REVISION=True
            ;;

        -u | --print-update-level)
            shift
            X11_PRINT_UPDATE_LEVEL=True
            ;;

        -s | --print-version-string)
            shift
            X11_PRINT_VERSION_STRING=True
            ;;

        -id | --print-include-dir)
            shift
            X11_PRINT_INCLUDE_DIR=True
            ;;

        -sd | --print-static-dir)
            shift
            X11_PRINT_STATIC_DIR=True
            ;;

        -dd | --print-dynamic-dir)
            shift
            X11_PRINT_DYNAMIC_DIR=True
            ;;

        -sl | --print-static-lib)
            shift
            X11_PRINT_STATIC_LIB=True
            ;;

        -dl | --print-dynamic-lib)
            shift
            X11_PRINT_DYNAMIC_LIB=True
            ;;

        -if | --print-include-flags)
            shift
            X11_PRINT_INCLUDE_FLAGS=True
            ;;

        -sf | --print-static-flags)
            shift
            X11_PRINT_STATIC_FLAGS=True
            ;;

        -df | --print-dynamic-flags)
            shift
            X11_PRINT_DYNAMIC_FLAGS=True
            ;;

        -dp | --print-dynamic-paths)
            shift
            X11_PRINT_DYNAMIC_PATHS=True
            ;;

        -e | --print-everything)
            shift
            X11_PRINT_EVERYTHING=True
            ;;

        -de | --dynamic-ext)
            shift
            X11_DYNAMIC_EXT="$1"
            shift
            ;;

        -se | --static-ext)
            shift
            X11_STATIC_EXT="$1"
            shift
            ;;

        -o | --set-object-name)
            shift
            X11_OBJECT_NAME="$1"
            shift
            ;;

        -cc | --set-compiler)
            shift
            X11_CC="$1"
            shift
            ;;

        -c | --cleanup)
            shift
            X11_CLEANUP=True
            ;;

        -*)
            echo "`basename $0`: invalid option '$1'"
            shift
            x11_usage
			exit 0
            ;;
    esac
done

##
## X11 info program name
##
X11_PROG="$X11_PROG_PREFIX"_"$X11_OBJECT_NAME"
X11_SRC="$X11_PROG_PREFIX"_"$X11_OBJECT_NAME.c"

##
## The library names
##
X11_DYNAMIC_LIB_NAME=libX11.$X11_DYNAMIC_EXT
X11_STATIC_LIB_NAME=libX11.$X11_STATIC_EXT

##
## Cleanup the dummy test source/program
##
x11_cleanup()
{
	rm -f $X11_PROG $X11_SRC
}

##
## -c | --cleanup
##
if [ "$X11_CLEANUP" = "True" ]
then
	x11_cleanup

	exit 0
fi

##
## Look for <X11/Xlib.h>
##
for d in $X11_SEARCH_PATH
do
	if [ -d $d/include/X11 -a -f $d/include/X11/Xlib.h ]
	then
		X11_INCLUDE_DIR=$d/include
		break;
	fi
done

##
## Make sure the <X11/Xlib.h> header was found.
##
if [ -z $X11_INCLUDE_DIR ]
then
	echo
	echo "Could not find <X11/Xlib.h> anywhere on your system."
	echo

	exit 1
fi

##
## Generate the dummy test program if needed
##
if [ ! -f $X11_SRC ]
then
cat << EOF > $X11_SRC
#include <stdio.h>
#include <X11/Xlib.h>

int
main(int argc,char ** argv)
{
#if 0
    /* XmVERSION:XmREVISION:XmUPDATE_LEVEL:XmVERSION_STRING:IsLesstif */
	fprintf(stdout,"%d:%d:%d:%s:%s\n",
		XmVERSION,
		XmREVISION,
		XmUPDATE_LEVEL,
		XmVERSION_STRING,
		lesstif);
#else
    /* XlibSpecificationRelease */
	fprintf(stdout,"%d\n",
		XlibSpecificationRelease);
#endif
	return 0;
}
EOF
fi

##
## Make sure code was created
##
if [ ! -f $X11_SRC ]
then
	echo
	echo "Could not create or read test program source $X11_SRC."
	echo

	exit 1
fi

##
## Set flags needed to Compile the dummy test program
##
X11_INCLUDE_FLAGS=-I$X11_INCLUDE_DIR

##
## Compile the dummy test program if needed
##
if [ ! -x $X11_PROG ]
then
	$X11_CC $X11_INCLUDE_FLAGS -o $X11_PROG $X11_SRC
fi

##
## Make sure it compiled
##
if [ ! -x $X11_PROG ]
then
	echo
	echo "Could not create or execute test program $X11_PROG."
	echo

	exit 1
fi

##
## Execute the dummy test program
##
X11_PROG_OUTPUT=`$X11_PROG`

##
## Output has the following format:
##
## 1         2          3              4                5
## XlibSpecificationRelease
##
X11_VERSION_RESULT=`echo $X11_PROG_OUTPUT | awk -F":" '{ print $1; }'`
#X11_REVISION_RESULT=`echo $X11_PROG_OUTPUT | awk -F":" '{ print $2; }'`
#X11_UPDATE_RESULT=`echo $X11_PROG_OUTPUT | awk -F":" '{ print $3; }'`
#X11_VERSION_REVISION_RESULT=$X11_VERSION_RESULT.$X11_REVISION_RESULT
#X11_VERSION_REVISION_UPDATE_RESULT=$X11_VERSION_REVISION_RESULT.$X11_UPDATE_RESULT
#X11_VERSION_STRING_RESULT=`echo $X11_PROG_OUTPUT | awk -F":" '{ print $4; }'`

##
## There could be up to 4 dyanmic libs and/or links.
##
## libX11.so
## libX11.so.6
## libX11.so.6.1
## libX11.so.6.1.x
##
X11_DYNAMIC_SEARCH_PATH="\
$X11_DYNAMIC_LIB_NAME \
$X11_DYNAMIC_LIB_NAME.$X11_VERSION_RESULT \
$X11_DYNAMIC_LIB_NAME.$X11_VERSION_REVISION_RESULT \
$X11_DYNAMIC_LIB_NAME.$X11_VERSION_REVISION_UPDATE_RESULT \
"

##
## Look for static library
##
for d in $X11_SEARCH_PATH
do
	if [ -f $d/lib/$X11_STATIC_LIB_NAME ]
	then
		X11_STATIC_DIR=$d/lib

		X11_STATIC_LIB=$X11_STATIC_DIR/$X11_STATIC_LIB_NAME

		X11_STATIC_FLAGS=$X11_STATIC_LIB

		break
	fi
done


##
## Look for dyanmic libraries
##
for d in $X11_SEARCH_PATH
do
	for l in $X11_DYNAMIC_SEARCH_PATH
	do
		if [ -r $d/lib/$l ]
		then
			X11_DYNAMIC_DIR=$d/lib

			X11_DYNAMIC_LIB=$d/lib/$l

			X11_DYNAMIC_PATHS="-L$X11_DYNAMIC_DIR"

			X11_DYNAMIC_FLAGS="-lX11"

			break 2
		fi
	done
done

##
## If the static library directory is different than the dynamic one, it 
## is possible that the system contains two incompatible installations of
## x11.  For example, a hacked x11 could be installed in /foo/X11.HACKED
## and the real x11 could be installed in /usr/X11R6.  This would cause
## outofwhackage later in the build.
##
## Need to handle this one.  Maybe we should just ignore the x11 static
## libs and just use the hacked ones ?  This is probably what the "user"
## wants anyway.  For instance, a "user" could be testing whether mozilla
## works with X11.HACKED without erasing the real x11 libs.
##
## Also, some x11 installations only have dynamic libraries.

##
## -e | --print-everything
##
if [ "$X11_PRINT_EVERYTHING" = "True" ]
then
	echo
	echo "XlibSpecificationRelease:          $X11_VERSION_RESULT"
#	echo "XmREVISION:         $X11_REVISION_RESULT"
#	echo "XmUPDATE_LEVEL:     $X11_UPDATE_RESULT"
#	echo "XmVERSION_STRING:   $X11_VERSION_STRING_RESULT"
#	echo
	echo "Include dir:        $X11_INCLUDE_DIR"
	echo "Static lib dir:     $X11_STATIC_DIR"
	echo "Dynamic lib dir:    $X11_DYNAMIC_DIR"
	echo
	echo "Static lib:         $X11_STATIC_LIB"
	echo "Dynamic lib:        $X11_DYNAMIC_LIB"
	echo
	echo "Include flags:      $X11_INCLUDE_FLAGS"
	echo "Static flags:       $X11_STATIC_FLAGS"
	echo "dynamic paths:      $X11_DYNAMIC_PATHS"
	echo "dynamic flags:      $X11_DYNAMIC_FLAGS"
	echo
	echo "OBJECT_NAME:        $X11_OBJECT_NAME"
	echo "Test program:       $X11_PROG"
	echo

	exit 0
fi

##
## -id | --print-include-dir
##
if [ "$X11_PRINT_INCLUDE_DIR" = "True" ]
then
	echo $X11_INCLUDE_DIR

	exit 0
fi

##
## -dd | --print-dynamic-dir
##
if [ "$X11_PRINT_DYNAMIC_DIR" = "True" ]
then
	echo $X11_DYNAMIC_DIR

	exit 0
fi

##
## -sd | --print-static-dir
##
if [ "$X11_PRINT_STATIC_DIR" = "True" ]
then
	echo $X11_STATIC_DIR

	exit 0
fi

##
## -dl | --print-dynamic-lib
##
if [ "$X11_PRINT_DYNAMIC_LIB" = "True" ]
then
	echo $X11_DYNAMIC_LIB

	exit 0
fi

##
## -sl | --print-static-lib
##
if [ "$X11_PRINT_STATIC_LIB" = "True" ]
then
	echo $X11_STATIC_LIB

	exit 0
fi

##
## -if | --print-include-flags
##
if [ "$X11_PRINT_INCLUDE_FLAGS" = "True" ]
then
	echo $X11_INCLUDE_FLAGS

	exit 0
fi

##
## -df | --print-dynamic-flags
##
if [ "$X11_PRINT_DYNAMIC_FLAGS" = "True" ]
then
	echo $X11_DYNAMIC_FLAGS

	exit 0
fi

##
## -dp | --print-dynamic-paths
##
if [ "$X11_PRINT_DYNAMIC_PATHS" = "True" ]
then
	echo $X11_DYNAMIC_PATHS

	exit 0
fi

##
## -sf | --print-static-flags
##
if [ "$X11_PRINT_STATIC_FLAGS" = "True" ]
then
	echo $X11_STATIC_FLAGS

	exit 0
fi

num=

##
## -v | --print-version
##
if [ "$X11_PRINT_VERSION" = "True" ]
then
	num=$X11_VERSION_RESULT

	##
	## -r | --print-revision
	##
	if [ "$X11_PRINT_REVISION" = "True" ]
	then
		num="$num".
		num="$num"$X11_REVISION_RESULT

		##
		## -u | --print-update-level
		##
		if [ "$X11_PRINT_UPDATE_LEVEL" = "True" ]
		then
			num="$num".
			num="$num"$X11_UPDATE_RESULT
		fi
	fi

	echo $num

	exit 0
fi


##
## -s | --print-version-string
##
if [ "$X11_PRINT_VERSION_STRING" = "True" ]
then
	echo $X11_VERSION_STRING_RESULT

	exit 0
fi

##
## Default: Print XmVERSION
##
echo $X11_VERSION_RESULT

exit 0
