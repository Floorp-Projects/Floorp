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
## Name: detect_motif.sh - Get motif lib location, version and other info.
##
## Description:	Artificial intelligence to figure out:
##
##              + Where the motif headers/lib are located.
##              + Whether lesstif is being used.
##              + The version of motif being used.
##              + The compile and link flags needed to build motif apps.
##
## Author: Ramiro Estrugo <ramiro@netscape.com>
##
##############################################################################


## This script looks in the space sepeared list of directories 
## (MOTIF_SEARCH_PATH) for motif headers and libraries.
##
## The search path can be overrided by the user by setting:
##
## MOZILLA_MOTIF_SEARCH_PATH
##
## To a space delimeted list of directories to search for motif paths.
##
## For example, if you have many different versions of motif installed,
## and you want the current build to use headers in /foo/motif/include
## and libraries in /foo/motif/lib, then do this:
##
## export MOZILLA_MOTIF_SEARCH_PATH="/foo/motif"
##
## The script also generates and builds a program 'detect_motif-OBJECT_NAME'
## which prints out info on the motif detected on the system.
##
## This information is munged into useful strings that can be printed
## through the various command line flags decribed bellow.  This script
## can be invoked from Makefiles or other scripts in order to set
## flags needed to build motif program.
##
## The 'detect_motif-OBJECT_NAME' program is generated/built only
## once and reused.  Because of the OBJECT_NAME suffix, it will work on
## multiple platforms at the same time.
##
## The generated files can be wiped by the --cleanup flag.
##

##
## Command Line Flags Supported:
##
##  -l   | --is-lesstif:			Print True/False if usign lesstif.
##
##  -v   | --print-version:			Print XmVERSION.
##  -r   | --print-revision:		Print XmREVISION.
##  -u   | --print-update-level:	Print XmUPDATE_LEVEL.
##  -s   | --print-version-string:	Print XmVERSION_STRING.
##
##  -id  | --print-include-dir:		Print dir of motif includes.
##  -sd  | --print-static-dir:		Print dir of motif static libs.
##  -dd  | --print-dynamic-dir:		Print dir of motif dynamic libs.
##
##  -sl  | --print-static-lib:		Print static lib.
##  -dl  | --print-dynamic-lib:		Print dynamic lib.
##
##  -if  | --print-include-flags:	Print cc flags needed to build motif apps.
##  -sf  | --print-static-flags:	Print ld flags for linking statically.
##  -df  | --print-dynamic-flags:	Print ld flags for linking dynamically.
##  -dp  | --print-dynamic-paths:	Print ld paths for linking dynamically.
##
##  -de  | --dynamic-ext:			Set extension used on dynamic libs.
##  -se  | --static-ext:			Set extension used on static libs.
##
##  -o   | --set-object-name:		Set object name for current system.
##  -cc  | --set-compiler:			Set compiler for building test program.
##
##  -xif | --set-x11-include-flags:	Set X11 include flags.
##
##  -xpm | --requires-xpm:			Print {True,False} if we need libXpm.
##
##  -c   | --cleanup:				Clean up any generated files.
##

##
## Look for motif headers in the following places:
##
DEFAULT_MOTIF_INCLUDE_SEARCH_PATH="\
/usr/lesstif/include \
/usr/local/include \
/usr/dt/include \
/usr/X11R6/include \
/usr/X/include \
/usr/include \
/usr/include/Motif1.2 \
"

##
## Look for motif libraries in the following places:
##
DEFAULT_MOTIF_LIB_SEARCH_PATH="\
/usr/lesstif/lib \
/usr/local/lib \
/usr/dt/lib \
/usr/X11R6/lib \
/usr/X/lib \
/usr/lib \
/usr/lib/Motif1.2 \
/usr/lib/Motif1.2_R6 \
"

##
## The user can override the default share path by setting MOZILLA_MOTIF_SEARCH
##
if [ -n "$MOZILLA_MOTIF_INCLUDE_SEARCH_PATH" ]
then
    MOTIF_INCLUDE_SEARCH_PATH=$MOZILLA_MOTIF_INCLUDE_SEARCH_PATH
else
    MOTIF_INCLUDE_SEARCH_PATH=$DEFAULT_MOTIF_INCLUDE_SEARCH_PATH
fi

if [ -n "$MOZILLA_MOTIF_LIB_SEARCH_PATH" ]
then
    MOTIF_LIB_SEARCH_PATH=$MOZILLA_MOTIF_LIB_SEARCH_PATH
else
    MOTIF_LIB_SEARCH_PATH=$DEFAULT_MOTIF_LIB_SEARCH_PATH
fi

##
## Constants
##
MOTIF_PROG_PREFIX=./detect_motif

##
## Defaults
##
MOTIF_DYNAMIC_EXT=so
MOTIF_STATIC_EXT=a

MOTIF_PRINT_IS_LESSTIF=False

MOTIF_PRINT_VERSION=False
MOTIF_PRINT_REVISION=False
MOTIF_PRINT_UPDATE_LEVEL=False

MOTIF_PRINT_INCLUDE_DIR=False
MOTIF_PRINT_STATIC_DIR=False
MOTIF_PRINT_DYNAMIC_DIR=False

MOTIF_PRINT_STATIC_LIB=False
MOTIF_PRINT_DYNAMIC_LIB=False

MOTIF_PRINT_INCLUDE_FLAGS=False
MOTIF_PRINT_STATIC_FLAGS=False
MOTIF_PRINT_DYNAMIC_FLAGS=False
MOTIF_PRINT_DYNAMIC_PATHS=False

MOTIF_PRINT_EVERYTHING=False

MOTIF_OBJECT_NAME=`uname`-`uname -r`
MOTIF_OBJECT_NAME=`echo $MOTIF_OBJECT_NAME|sed -e 's:/:_:g'`
MOTIF_CC=cc

MOTIF_X11_INCLUDE_FLAGS=

MOTIF_CLEANUP=False

MOTIF_CHECK_XPM=False

##
## Stuff we need to figure out
##
MOTIF_VERSION_RESULT=unknown
MOTIF_REVISION_RESULT=unknown
MOTIF_UPDATE_RESULT=unknown
MOTIF_VERSION_REVISION_RESULT=unknown
MOTIF_VERSION_REVISION_UPDATE_RESULT=unknown
MOTIF_VERSION_STRING_RESULT=unknown
MOTIF_IS_LESSTIF_RESULT=unknown

MOTIF_INCLUDE_DIR=unknown
MOTIF_STATIC_LIB=unknown
MOTIF_DYNAMIC_LIB=unknown

MOTIF_STATIC_DIR=unknown
MOTIF_DYNAMIC_DIR=unknown

MOTIF_INCLUDE_FLAGS=unknown
MOTIF_STATIC_FLAGS=unknown
MOTIF_DYNAMIC_FLAGS=unknown
MOTIF_DYNAMIC_PATHS=unknown

motif_usage()
{
echo
echo "Usage:   `basename $0` [options]"
echo
echo "  -l,   --is-lesstif:            Print {True,False} if using lesstif."
echo
echo "  -v,   --print-version:         Print XmVERSION."
echo "  -r,   --print-revision:        Print XmREVISION."
echo "  -u,   --print-update-level:    Print XmUPDATE_LEVEL."
echo "  -s,   --print-version-string:  Print XmVERSION_STRING."
echo
echo "  -id,  --print-include-dir:     Print dir of motif includes."
echo "  -sd,  --print-static-dir:      Print dir of motif static libs."
echo "  -dd,  --print-dynamic-dir:     Print dir of motif dynamic libs."
echo
echo "  -sl,  --print-static-lib:      Print static lib."
echo "  -dl   --print-dynamic-lib:     Print dynamic lib."
echo
echo "  -if,  --print-include-flags:   Print cc flags needed to compile."
echo "  -sf,  --print-static-flags:    Print ld flags for linking statically."
echo "  -df,  --print-dynamic-flags:   Print ld flags for linking dynamically."
echo "  -dp,  --print-dynamic-paths:   Print ld paths for linking dynamically."
echo
echo "  -e,   --print-everything:      Print everything that is known."
echo
echo "  -de,  --dynamic-ext:           Set extension used on dynamic libs."
echo "  -se,  --static-ext:            Set extension used on static libs."
echo
echo "  -o,   --set-object-name:       Set object name for current system."
echo "  -cc,  --set-compiler:          Set compiler for building test program."
echo
echo "  -xif, --set-x11-include-flags: Set X11 include flags."
echo
echo "  -xpm, --requires-xpm:          Print {True,False} if we need libXpm."
echo
echo "  -h,   --help:                  Print this blurb."
echo
echo "  -c,   --cleanup:               Clean up any generated files."
echo
echo "The default is '-v -r' if no options are given."
echo
}

##
## Parse the command line
##
while [ "$*" ]; do
    case $1 in
        -h | --help)
            shift
            motif_usage
			exit 0
            ;;

        -l | --is-lesstif)
            shift
            MOTIF_PRINT_IS_LESSTIF=True
            ;;

        -v | --print-version)
            shift
            MOTIF_PRINT_VERSION=True
            ;;

        -r | --print-revision)
            shift
            MOTIF_PRINT_REVISION=True
            ;;

        -u | --print-update-level)
            shift
            MOTIF_PRINT_UPDATE_LEVEL=True
            ;;

        -s | --print-version-string)
            shift
            MOTIF_PRINT_VERSION_STRING=True
            ;;

        -id | --print-include-dir)
            shift
            MOTIF_PRINT_INCLUDE_DIR=True
            ;;

        -sd | --print-static-dir)
            shift
            MOTIF_PRINT_STATIC_DIR=True
            ;;

        -dd | --print-dynamic-dir)
            shift
            MOTIF_PRINT_DYNAMIC_DIR=True
            ;;

        -sl | --print-static-lib)
            shift
            MOTIF_PRINT_STATIC_LIB=True
            ;;

        -dl | --print-dynamic-lib)
            shift
            MOTIF_PRINT_DYNAMIC_LIB=True
            ;;

        -if | --print-include-flags)
            shift
            MOTIF_PRINT_INCLUDE_FLAGS=True
            ;;

        -sf | --print-static-flags)
            shift
            MOTIF_PRINT_STATIC_FLAGS=True
            ;;

        -df | --print-dynamic-flags)
            shift
            MOTIF_PRINT_DYNAMIC_FLAGS=True
            ;;

        -dp | --print-dynamic-paths)
            shift
            MOTIF_PRINT_DYNAMIC_PATHS=True
            ;;

        -e | --print-everything)
            shift
            MOTIF_PRINT_EVERYTHING=True
            ;;

        -de | --dynamic-ext)
            shift
            MOTIF_DYNAMIC_EXT="$1"
            shift
            ;;

        -se | --static-ext)
            shift
            MOTIF_STATIC_EXT="$1"
            shift
            ;;

        -o | --set-object-name)
            shift
            MOTIF_OBJECT_NAME="$1"
            shift
            ;;

        -cc | --set-compiler)
            shift
            MOTIF_CC="$1"
            shift
            ;;

        -xif | --set-x11-include-flags)
            shift
            MOTIF_X11_INCLUDE_FLAGS="$1"
            shift
            ;;

        -c | --cleanup)
            shift
            MOTIF_CLEANUP=True
            ;;

        -xpm | --requires-xpm)
            shift
            MOTIF_CHECK_XPM=True
            ;;

        -*)
            echo "`basename $0`: invalid option '$1'"
            shift
            motif_usage
			exit 0
            ;;
    esac
done

##
## Motif info program name
##
MOTIF_PROG="$MOTIF_PROG_PREFIX"_"$MOTIF_OBJECT_NAME"
MOTIF_SRC="$MOTIF_PROG_PREFIX"_"$MOTIF_OBJECT_NAME.c"

##
## The library names
##
MOTIF_DYNAMIC_LIB_NAME=libXm.$MOTIF_DYNAMIC_EXT
MOTIF_STATIC_LIB_NAME=libXm.$MOTIF_STATIC_EXT

##
## Cleanup the dummy test source/program
##
motif_cleanup()
{
	rm -f $MOTIF_PROG $MOTIF_SRC
}

##
## Check whether the motif libs need Xpm
##
motif_check_xpm()
{
	_lib=
	_count=0

	if [ -f $MOTIF_DYNAMIC_LIB ]
	then
		_lib=$MOTIF_DYNAMIC_LIB
	else
		if [ -f $MOTIF_STATIC_LIB ]
		then
			_lib=$MOTIF_STATIC_LIB
		fi
	fi

	if [ -n "$_lib" ]
	then
		if [ -f $_lib ]
		then
			# Solaris 2.6's motif has builtin Xpm support.
			# Its a dumbass hack...  Why not ship libXpm to avoid
			# the confusion ?  Anyway, do 'grep -v XmXpm' to catch this
			# problem.  In this case Xpm is not needed, since the symbols
			# are builtin to the libXm library.
			_count=`strings $_lib | grep Xpm | grep -v XmXpm | wc -l`
		fi
	fi

	if [ $_count -gt 0 ]
	then
		echo True
	else
		echo False
	fi

	unset _count
	unset _lib
}

##
## -c | --cleanup
##
if [ "$MOTIF_CLEANUP" = "True" ]
then
	motif_cleanup

	exit 0
fi

##
## Look for <Xm/Xm.h>
##
for d in $MOTIF_INCLUDE_SEARCH_PATH
do
	# Check for $d that exists and is readable
	if [ -d $d -a -r $d ]
	then
		if [ -d $d/Xm -a -f $d/Xm/Xm.h ]
		then
			MOTIF_INCLUDE_DIR=$d
			break;
		fi
	fi
done

##
## Make sure the <Xm/Xm.h> header was found.
##
if [ "$MOTIF_INCLUDE_DIR" = "unknown" ]
then
	echo
	echo "Could not find <Xm/Xm.h> anywhere on your system."
	echo

	exit 1
fi

##
## Generate the dummy test program if needed
##
if [ ! -f $MOTIF_SRC ]
then
cat << EOF > $MOTIF_SRC
#include <stdio.h>
#include <Xm/Xm.h>

int
main(int argc,char ** argv)
{
	char * lesstif =
#ifdef LESSTIF_VERSION
	"True"
#else
	"False"
#endif
	;

    /* XmVERSION:XmREVISION:XmUPDATE_LEVEL:XmVERSION_STRING:IsLesstif */
	fprintf(stdout,"%d:%d:%d:%s:%s\n",
		XmVERSION,
		XmREVISION,
		XmUPDATE_LEVEL,
		XmVERSION_STRING,
		lesstif);

	return 0;
}
EOF
fi

##
## Make sure code was created
##
if [ ! -f $MOTIF_SRC ]
then
	echo
	echo "Could not create or read test program source $MOTIF_SRC."
	echo

	exit 1
fi

##
## Set flags needed to Compile the dummy test program
##
MOTIF_INCLUDE_FLAGS=-I$MOTIF_INCLUDE_DIR

##
## Compile the dummy test program if needed
##
if [ ! -x $MOTIF_PROG ]
then
	$MOTIF_CC $MOTIF_INCLUDE_FLAGS $MOTIF_X11_INCLUDE_FLAGS -o $MOTIF_PROG $MOTIF_SRC
fi

##
## Make sure it compiled
##
if [ ! -x $MOTIF_PROG ]
then
	echo
	echo "Could not create or execute test program $MOTIF_PROG."
	echo

	exit 1
fi

##
## Execute the dummy test program
##
MOTIF_PROG_OUTPUT=`$MOTIF_PROG`

##
## Output has the following format:
##
## 1         2          3              4                5
## XmVERSION:XmREVISION:XmUPDATE_LEVEL:XmVERSION_STRING:IsLesstif
##
MOTIF_VERSION_RESULT=`echo $MOTIF_PROG_OUTPUT | awk -F":" '{ print $1; }'`
MOTIF_REVISION_RESULT=`echo $MOTIF_PROG_OUTPUT | awk -F":" '{ print $2; }'`
MOTIF_UPDATE_RESULT=`echo $MOTIF_PROG_OUTPUT | awk -F":" '{ print $3; }'`
MOTIF_VERSION_REVISION_RESULT=$MOTIF_VERSION_RESULT.$MOTIF_REVISION_RESULT
MOTIF_VERSION_REVISION_UPDATE_RESULT=$MOTIF_VERSION_REVISION_RESULT.$MOTIF_UPDATE_RESULT
MOTIF_VERSION_STRING_RESULT=`echo $MOTIF_PROG_OUTPUT | awk -F":" '{ print $4; }'`
MOTIF_IS_LESSTIF_RESULT=`echo $MOTIF_PROG_OUTPUT | awk -F":" '{ print $5; }'`

##
## There could be up to 4 dyanmic libs and/or links.
##
## libXm.so
## libXm.so.1
## libXm.so.1.2
## libXm.so.1.2.4
##
MOTIF_DYNAMIC_SEARCH_PATH="\
$MOTIF_DYNAMIC_LIB_NAME \
$MOTIF_DYNAMIC_LIB_NAME.$MOTIF_VERSION_RESULT \
$MOTIF_DYNAMIC_LIB_NAME.$MOTIF_VERSION_REVISION_RESULT \
$MOTIF_DYNAMIC_LIB_NAME.$MOTIF_VERSION_REVISION_UPDATE_RESULT \
"

##
## Look for static library
##
for d in $MOTIF_LIB_SEARCH_PATH
do
	if [ -f $d/$MOTIF_STATIC_LIB_NAME ]
	then
		MOTIF_STATIC_DIR=$d

		MOTIF_STATIC_LIB=$MOTIF_STATIC_DIR/$MOTIF_STATIC_LIB_NAME

		MOTIF_STATIC_FLAGS=$MOTIF_STATIC_LIB

		break
	fi
done


##
## Look for dyanmic libraries
##
for d in $MOTIF_LIB_SEARCH_PATH
do
	for l in $MOTIF_DYNAMIC_SEARCH_PATH
	do
		if [ -r $d/$l ]
		then
			MOTIF_DYNAMIC_DIR=$d

			MOTIF_DYNAMIC_LIB=$d/$l

			MOTIF_DYNAMIC_PATHS="-L$MOTIF_DYNAMIC_DIR"

			MOTIF_DYNAMIC_FLAGS="-lXm"

			break 2
		fi
	done
done

##
## If the static library directory is different than the dynamic one, it 
## is possible that the system contains two incompatible installations of
## motif/lesstif.  For example, lesstif could be installed in /usr/lesstif
## and the real motif could be installed in /usr/X11R6.  This would cause
## outofwhackage later in the build.
##
## Need to handle this one.  Maybe we should just ignore the motif static
## libs and just use the lesstif ones ?  This is probably what the "user"
## wants anyway.  For instance, a "user" could be testing whether mozilla
## works with lesstif without erasing the real motif libs.
##
## Also, by default the lesstif build system only creates dynamic libraries.
## So this problem will always exist when both motif and lesstif are installed
## in the system.
##
if [ "$MOTIF_STATIC_DIR" != "$MOTIF_DYNAMIC_DIR" ]
then
	MOTIF_STATIC_DIR=unknown
	MOTIF_STATIC_LIB=unknown
	MOTIF_STATIC_FLAGS=unknown
fi

##
## -l | --is-lesstif
##
if [ "$MOTIF_PRINT_IS_LESSTIF" = "True" ]
then
	echo $MOTIF_IS_LESSTIF_RESULT

	exit 0
fi

##
## -e | --print-everything
##
if [ "$MOTIF_PRINT_EVERYTHING" = "True" ]
then
	echo
	echo "XmVERSION:          $MOTIF_VERSION_RESULT"
	echo "XmREVISION:         $MOTIF_REVISION_RESULT"
	echo "XmUPDATE_LEVEL:     $MOTIF_UPDATE_RESULT"
	echo "XmVERSION_STRING:   $MOTIF_VERSION_STRING_RESULT"
	echo
	echo "Lesstif ?:          $MOTIF_IS_LESSTIF_RESULT"
	echo
	echo "Include dir:        $MOTIF_INCLUDE_DIR"
	echo "Static lib dir:     $MOTIF_STATIC_DIR"
	echo "Dynamic lib dir:    $MOTIF_DYNAMIC_DIR"
	echo
	echo "Static lib:         $MOTIF_STATIC_LIB"
	echo "Dynamic lib:        $MOTIF_DYNAMIC_LIB"
	echo
	echo "Include flags:      $MOTIF_INCLUDE_FLAGS"
	echo "Static fags:        $MOTIF_STATIC_FLAGS"
	echo "Dynamic paths:      $MOTIF_DYNAMIC_PATHS"
	echo "Dynamic flags:      $MOTIF_DYNAMIC_FLAGS"
	echo
	echo "OBJECT_NAME:        $MOTIF_OBJECT_NAME"
	echo "Test program:       $MOTIF_PROG"
	echo

	exit 0
fi

##
## -xpm | --requires-xpm
##
if [ "$MOTIF_CHECK_XPM" = "True" ]
then
	motif_check_xpm

	exit 0
fi

##
## -id | --print-include-dir
##
if [ "$MOTIF_PRINT_INCLUDE_DIR" = "True" ]
then
	echo $MOTIF_INCLUDE_DIR

	exit 0
fi

##
## -dd | --print-dynamic-dir
##
if [ "$MOTIF_PRINT_DYNAMIC_DIR" = "True" ]
then
	echo $MOTIF_DYNAMIC_DIR

	exit 0
fi

##
## -sd | --print-static-dir
##
if [ "$MOTIF_PRINT_STATIC_DIR" = "True" ]
then
	echo $MOTIF_STATIC_DIR

	exit 0
fi

##
## -dl | --print-dynamic-lib
##
if [ "$MOTIF_PRINT_DYNAMIC_LIB" = "True" ]
then
	echo $MOTIF_DYNAMIC_LIB

	exit 0
fi

##
## -sl | --print-static-lib
##
if [ "$MOTIF_PRINT_STATIC_LIB" = "True" ]
then
	echo $MOTIF_STATIC_LIB

	exit 0
fi

##
## -if | --print-include-flags
##
if [ "$MOTIF_PRINT_INCLUDE_FLAGS" = "True" ]
then
	echo $MOTIF_INCLUDE_FLAGS

	exit 0
fi

##
## -df | --print-dynamic-flags
##
if [ "$MOTIF_PRINT_DYNAMIC_FLAGS" = "True" ]
then
	echo $MOTIF_DYNAMIC_FLAGS

	exit 0
fi

##
## -dp | --print-dynamic-paths
##
if [ "$MOTIF_PRINT_DYNAMIC_PATHS" = "True" ]
then
	echo $MOTIF_DYNAMIC_PATHS

	exit 0
fi

##
## -sf | --print-static-flags
##
if [ "$MOTIF_PRINT_STATIC_FLAGS" = "True" ]
then
	echo $MOTIF_STATIC_FLAGS

	exit 0
fi


num=

##
## -v | --print-version
##
if [ "$MOTIF_PRINT_VERSION" = "True" ]
then
	num=$MOTIF_VERSION_RESULT

	##
	## -r | --print-revision
	##
	if [ "$MOTIF_PRINT_REVISION" = "True" ]
	then
		num="$num".
		num="$num"$MOTIF_REVISION_RESULT

		##
		## -u | --print-update-level
		##
		if [ "$MOTIF_PRINT_UPDATE_LEVEL" = "True" ]
		then
			num="$num".
			num="$num"$MOTIF_UPDATE_RESULT
		fi
	fi

	echo $num

	exit 0
fi


##
## -s | --print-version-string
##
if [ "$MOTIF_PRINT_VERSION_STRING" = "True" ]
then
	echo $MOTIF_VERSION_STRING_RESULT

	exit 0
fi

##
## Default: Print XmVERSION.XmREVISION
##
echo $MOTIF_VERSION_RESULT.$MOTIF_REVISION_RESULT

exit 0
