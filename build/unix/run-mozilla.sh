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
## 
## Usage:
##
## $ run-mozilla.sh [options] [program] [program arguments]
##
## This script is meant to run a mozilla program from the mozilla
## source tree.  This is mostly useful to folks hacking on mozilla.
##
## A mozilla program is currently either viewer or mozilla-bin.  The
## default is viewer.
##
## The script will setup all the environment voodoo needed to make
## mozilla work.
#
##
## Standard shell script disclaimer blurb thing:
##
## This script is a hack.  It's brute force.  It's horrible.  
## It doesn't use Artificial Intelligence.  It doesn't use Virtual Reality.
## It's not perl.  It's not python.  It probably won't work unchanged on
## the "other" thousands of unices.  But it worksforme.  --ramiro
##
## If you have an improvement, patch, idea, whatever, on how to make this
## script better, post it here:
##
## news://news.mozilla.org/netscape.public.mozilla.patches
## news://news.mozilla.org/netscape.public.mozilla.unix
## 
#
##
## Potential improvements:
##
## + Run from anywhere in the tree.
## + Run ldd on the program and report missing dlls
## + Deal with NSPR in the tree
## + All the other unices
##
#
cmdname=`basename $0`
MOZ_DIST_BIN=`dirname $0`
MOZ_APPRUNNER_NAME="./mozilla-bin"
MOZ_VIEWER_NAME="./viewer"
MOZ_PROGRAM=""
#
##
## Functions
##
##########################################################################
moz_usage()
{
    cat << EOF

Usage:  ${cmdname} [options] [program]

  options:

    -g                   Run in debugger.
    --debug

    -d debugger          Debugger to use.
    --debugger debugger

  Examples:

  Run the viewer

    ${cmdname} viewer

  Run the mozilla-bin binary

    ${cmdname} mozilla-bin

  Debug the viewer in a debugger

    ${cmdname} -g viewer

  Debug the mozilla-bin binary in gdb

    ${cmdname} -g mozilla-bin -d gdb

EOF
	return 0
}
##########################################################################
moz_bail()
{
	message=$1
	echo
	echo "$cmdname: $message"
	echo
	exit 1
}
##########################################################################
moz_test_binary()
{
	binary=$1
	if [ -f "$binary" ]
	then
		if [ -x "$binary" ]
		then
			return 1
		fi
	fi
	return 0
}
##########################################################################
moz_get_debugger()
{
	debuggers="ddd gdb dbx bdb"
	debugger="notfound"
	done="no"
	for d in $debuggers
	do
		dpath=`which ${d}`
		if [ -x "$dpath" ]
		then
			debugger=$dpath
			break
		fi
	done
	echo $debugger
	return 0
}
##########################################################################
moz_run_program()
{
	prog=$MOZ_PROGRAM
	##
	## Make sure the program is executable
	##
	if [ ! -x "$prog" ]
	then
		moz_bail "Cannot execute $prog."
	fi
	##
	## Use md5sum to crc a core file.  If md5sum is not found on the system,
	## then dont debug core files.
	##
	crc_prog=`which md5sum`
	if [ -x "$crc_prog" ]
	then
		DEBUG_CORE_FILES=1
	fi
	if [ "$DEBUG_CORE_FILES" ]
	then
		crc_old=
		if [ -f core ]
		then
			crc_old=`$crc_prog core | awk '{print $1;}' `
		fi
	fi
	##
	## Run the program
	##
	$prog ${1+"$@"}
	if [ "$DEBUG_CORE_FILES" ]
	then
		if [ -f core ]
		then
			crc_new=`$crc_prog core | awk '{print $1;}' `
		fi
	fi
	if [ "$crc_old" != "$crc_new" ]
	then
		printf "\n\nOh no!  %s just dumped a core file.\n\n" $prog
		printf "Do you want to debug this ? "
		printf "You need a lot of memory for this, so watch out ? [y/n] "
		read ans
		if [ "$ans" = "y" ]
		then
			debugger=`moz_get_debugger`
			if [ -x "$debugger" ]
			then
				echo "$debugger $prog core"

				# See http://www.mozilla.org/unix/debugging-faq.html
				# For why LD_BIND_NOW is needed
				LD_BIND_NOW=1; export LD_BIND_NOW

				$debugger $prog core
			else
				echo "Could not find a debugger on your system."
			fi
		fi
	fi
}
##########################################################################
moz_debug_program()
{
	prog=$MOZ_PROGRAM
	##
	## Make sure the program is executable
	##
	if [ ! -x "$prog" ]
	then
		moz_bail "Cannot execute $prog."
	fi
	if [ -n "$moz_debugger" ]
	then
		debugger=`which $moz_debugger`
	else
		debugger=`moz_get_debugger`
	fi
    if [ -x "$debugger" ] 
    then
        echo "set args ${1+"$@"}" > /tmp/mozargs$$ 
# If you are not using ddd, gdb and know of a way to convey the arguments 
# over to the prog then add that here- Gagan Saksena 03/15/00
        case `basename $debugger` in
            gdb) echo "$debugger $prog -x /tmp/mozargs$$"
                $debugger $prog -x /tmp/mozargs$$
                ;;
            ddd) echo "$debugger --debugger \"gdb -x /tmp/mozargs$$\" $prog"
                $debugger --debugger "gdb -x /tmp/mozargs$$" $prog
                ;;
            *) echo "$debugger $prog ${1+"$@"}"
                $debugger $prog ${1+"$@"}
                ;;
        esac
        /bin/rm /tmp/mozargs$$
    else
        echo "Could not find a debugger on your system." 
    fi
}
##########################################################################
##
## Command line arg defaults
##
moz_debug=0
moz_debugger=""
#
##
## Parse the command line
##
while [ $# -gt 0 ]
do
  case $1 in
    -g | --debug)
      moz_debug=1
      shift
      ;;
    -d | --debugger)
      moz_debugger=$2;
      shift 2
      ;;
    *)
      break;
      ;;
  esac
done
#
##
## Program name given in $1
##
if [ $# -gt 0 ]
then
	MOZ_PROGRAM=$1
	shift
fi
##
## Program not given, try to guess a default
##
if [ -z "$MOZ_PROGRAM" ]
then
	##
	## Try viewer
	## 
	moz_test_binary $MOZ_VIEWER_NAME
	if [ $? -eq 1 ]
	then
		MOZ_PROGRAM=$MOZ_VIEWER_NAME
	##
	## Try mozilla-bin
	## 
	else
		moz_test_binary $MOZ_APPRUNNER_NAME
		if [ $? -eq 1 ]
		then
			MOZ_PROGRAM=$MOZ_APPRUNNER_NAME
		fi
	fi
fi
#
#
##
## Make sure the program is executable
##
if [ ! -x "$MOZ_PROGRAM" ]
then
	moz_bail "Cannot execute $MOZ_PROGRAM."
fi
#
##
## Set MOZILLA_FIVE_HOME
##
MOZILLA_FIVE_HOME=$MOZ_DIST_BIN
#
##
## Set LD_LIBRARY_PATH
LD_LIBRARY_PATH=${MOZ_DIST_BIN}${LD_LIBRARY_PATH+":$LD_LIBRARY_PATH"}
#
## Set SHLIB_PATH for HPUX
SHLIB_PATH=${MOZ_DIST_BIN}${SHLIB_PATH+":$SHLIB_PATH"}
#
## Set LIBPATH for AIX
LIBPATH=${MOZ_DIST_BIN}${LIBPATH+":$LIBPATH"}
#
## Set LIBPATH for BeOS
LIBRARY_PATH=${MOZ_DIST_BIN}:${MOZ_DIST_BIN}/components${LIBRARY_PATH+":$LIBRARY_PATH"}
#
## Set ADDON_PATH for BeOS
ADDON_PATH=${MOZ_DIST_BIN}${ADDON_PATH+":$ADDON_PATH"}

echo "MOZILLA_FIVE_HOME=$MOZILLA_FIVE_HOME"
echo "  LD_LIBRARY_PATH=$LD_LIBRARY_PATH"
echo "     LIBRARY_PATH=$LIBRARY_PATH"
echo "       SHLIB_PATH=$SHLIB_PATH"
echo "          LIBPATH=$LIBPATH"
echo "       ADDON_PATH=$ADDON_PATH"
echo "      MOZ_PROGRAM=$MOZ_PROGRAM"
echo "      MOZ_TOOLKIT=$MOZ_TOOLKIT"
echo "        moz_debug=$moz_debug"
echo "     moz_debugger=$moz_debugger"
#
export MOZILLA_FIVE_HOME LD_LIBRARY_PATH
export SHLIB_PATH LIBPATH LIBRARY_PATH ADDON_PATH

if [ $moz_debug -eq 1 ]
then
	moz_debug_program ${1+"$@"}
else
	moz_run_program ${1+"$@"}
fi
