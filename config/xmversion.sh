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
## Name:		xmversion.sh - a fast way to get a motif lib's version
##
## Description:	Print the major version number for motif libs on the
##				system that executes the script.  Can be tweaked to output
##				more info.  (such as cmd line args to print major/minor
##				version numbers).  Currently prints only the major number.
##
##              Also, more checks need to be added for more platforms.  
##              Currently this script is only usefull in the Linux Universe
##              where there are a many versions of motif.
##
## Author:		Ramiro Estrugo <ramiro@netscape.com>
##
##############################################################################

# The string build into the motif libs
MOTIF_VERSION_STRING="Motif Version"

# Make motif 1.x the default
MOTIF_DEFAULT_VERSION=1

# Reasonable defaults for motif lib locations
MOTIF_LIB_DIR=/usr/lib
MOTIF_STATIC_LIB=libXm.a
MOTIF_SHARED_LIB=libXm.so

os=`uname`

case `uname`
in
	Linux)
		case `uname -m`
		in
			ppc)
				MOTIF_LIB_DIR=/usr/local/motif-1.2.4/lib
				;;

			i386|i486|i586|i686)
				MOTIF_LIB_DIR=/usr/X11R6/lib
				;;
		esac
		;;

	SunOS)
		case `uname -r`
		in
			5.3|5.4|5.5.1|5.6)
				MOTIF_LIB_DIR=/usr/dt/lib
			;;
		esac
		;;

	HP-UX)
		MOTIF_LIB_DIR=/usr/lib/Motif1.2
		MOTIF_SHARED_LIB=libXm.sl
		;;

	AIX)
		MOTIF_LIB_DIR=/usr/lib
		MOTIF_SHARED_LIB=libXm.a
		;;

esac

# Look for the shared one first
MOTIF_LIB=

if [ -f $MOTIF_LIB_DIR/$MOTIF_SHARED_LIB ]
then
	MOTIF_LIB=$MOTIF_LIB_DIR/$MOTIF_SHARED_LIB
else
	if [ -f $MOTIF_LIB_DIR/$MOTIF_STATIC_LIB ]
	then
		MOTIF_LIB=$MOTIF_LIB_DIR/$MOTIF_STATIC_LIB
	else
		echo $MOTIF_DEFAULT_VERSION
		exit
	fi
fi

VERSION=`strings $MOTIF_LIB | grep "Motif Version" | awk '{ print $3;}'`

MAJOR=`echo $VERSION | awk -F"." '{ print $1; }'`

echo $MAJOR
