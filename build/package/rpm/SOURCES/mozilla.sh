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

## 
## Usage:
##
## $ mozilla
##
## This script is meant to run a mozilla program from the mozilla
## rpm installation.
##
## The script will setup all the environment voodoo needed to make
## mozilla work.

##
## Standard shell script disclaimer blurb thing:
##
## This script is a hcak.  Its brute force.  Its horrible.  
## It doesnt use Artificial Intelligence.  It doesnt use Virtual Reality.
## Its not perl.  Its not python.  It probably wont work unchanged on
## the "other" thousands of unices.  But it worksforme.
##
## If you have an improvement, patch, idea, whatever, on how to make this
## script better, post it here:
##
## news://news.mozilla.org/netscape.public.mozilla.patches
## news://news.mozilla.org/netscape.public.mozilla.unix
##
## 

##
## Potential improvements:
##
## + Run ldd on the program and report missing dlls
## + All the "other" unices/packages
##

cmdname=`basename $0`

##
## Variables
##
MOZ_DIST_BIN="/usr/lib/mozilla"
MOZ_PROGRAM="/usr/lib/mozilla/mozilla-bin"

##
## Set MOZILLA_FIVE_HOME
##
MOZILLA_FIVE_HOME="/usr/lib/mozilla"

export MOZILLA_FIVE_HOME

##
## Set LD_LIBRARY_PATH
##
if [ "$LD_LIBRARY_PATH" ]
then
  LD_LIBRARY_PATH=/usr/lib/mozilla:/usr/lib/mozilla/plugins:$LD_LIBRARY_PATH
else
  LD_LIBRARY_PATH=/usr/lib/mozilla:/usr/lib/mozilla/plugins
fi

export LD_LIBRARY_PATH

# tell the glibc for 7.1 that we need to use the old thread stack size
# model

export LD_ASSUME_KERNEL=2.2.5

# If there is no command line argument at all then try to open a new
# window in an already running instance.

if [ -z "$1" ]; then
  $MOZ_PROGRAM -remote "openurl(about:blank,new-window)" 2>/dev/null >/dev/null
  # no window found?
  RETURN_VAL=$?
  if [ "$RETURN_VAL" -eq "2" ]; then
    exec $MOZ_PROGRAM ${1+"$@"}
  fi
  if [ "$RETURN_VAL" -eq "0" ]; then
    exit 0;
  fi
  echo "Error sending command."
  exit $RETURN_VAL
fi

unset RETURN_VAL

# If there's a command line argument but it doesn't begin with a -
# it's probably a url.  Try to send it to a running instance.

USE_EXIST=0
opt="$1"
case "$opt" in
  -*) ;;
  *) USE_EXIST=1 ;;
esac

if [ "$USE_EXIST" -eq "1" ]; then
  # check to make sure that the command contains at least a :/ in it.
  echo $opt | grep -e ':/' 2>/dev/null > /dev/null
  RETURN_VAL=$?
  if [ "$RETURN_VAL" -eq "1" ]; then
    # does it begin with a / ?
    echo $opt | grep -e '^/' 2>/dev/null > /dev/null
    RETURN_VAL=$?
    if [ "$RETURN_VAL" -eq "0" ]; then
      opt="file:$opt"
    elif [ -e `pwd`/$opt ]; then
      opt="file://`pwd`/$opt"
    else
      opt="http://$opt"
    fi
  fi
  # ok, send it
  $MOZ_PROGRAM -remote "openurl($opt)" 2>/dev/null > /dev/null
  RETURN_VAL=$?
  if [ "$RETURN_VAL" -eq "2" ]; then
    exec $MOZ_PROGRAM ${1+"$@"}
  fi
  if [ "$RETURN_VAL" -eq "0" ]; then
    exit 0;
  fi
  echo "Error sending command."
  exit $RETURN_VAL
fi

exec $MOZ_PROGRAM ${1+"$@"}
