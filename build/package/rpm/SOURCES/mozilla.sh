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

cmdname=`basename $0`

## don't leave any core files around
ulimit -c 0

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

# Figure out if we need to ser LD_ASSUME_KERNEL for older versions of the JVM.

function set_jvm_vars() {

    if [ ! -L /usr/lib/mozilla/plugins/libjavaplugin_oji.so ]; then
	return;
    fi

    JVM_LINK=`perl -e 'print readlink("/usr/lib/mozilla/plugins/libjavaplugin_oji.so")'`

    # is it relative?  if so append the full path

    echo "${JVM_LINK}" | grep -e "^/" 2>&1 > /dev/null

    if [ "$?" -ne "0" ]; then
	JVM_LINK=/usr/lib/mozilla/plugins/${JVM_LINK}
    fi

    JVM_BASE=`basename $JVM_LINK`
    JVM_DIR=`echo $JVM_LINK | sed -e s/$JVM_BASE//g`
    JVM_DIR=${JVM_DIR}../../../bin/

    # make sure it was found
    if [ -z "$JVM_DIR" ]; then
        return
    fi

    # does it exist?
    if [ ! -d "$JVM_DIR" ]; then
        return
    fi

    JVM_COMMAND=$JVM_DIR/java
    # does the command exist?
    if [ ! -r "$JVM_COMMAND" ]; then
	return
    fi

    # export this temporarily - it seems to work with old and new
    # versions of the JVM.
    export LD_ASSUME_KERNEL=2.2.5

    # get the version
    JVM_VERSION=`$JVM_COMMAND -version 2>&1 | grep version | cut -f 3 -d " " | sed -e 's/\"//g'`

    unset LD_ASSUME_KERNEL

    case "$JVM_VERSION" in
	(1.3.0*)
	# bad JVM
	export LD_ASSUME_KERNEL=2.2.5
	;;
    esac
}

function check_running() {
    $MOZ_PROGRAM -remote 'ping()' 2>/dev/null >/dev/null
    RETURN_VAL=$?
    if [ "$RETURN_VAL" -eq "2" ]; then
      echo 0
      return 0
    else
      echo 1
      return 1
    fi
}

function open_mail() {
    if [ "${ALREADY_RUNNING}" -eq "1" ]; then
      exec $MOZ_PROGRAM -remote 'xfeDoCommand(openInbox)' \
        2>/dev/null >/dev/null
    else
      exec $MOZ_PROGRAM $*
    fi
}

function open_compose() {
    if [ "${ALREADY_RUNNING}" -eq "1" ]; then
      exec $MOZ_PROGRAM -remote 'xfeDoCommand(composeMessage)' \
        2>/dev/null >/dev/null
    else
      exec $MOZ_PROGRAM $*
    fi
}

# OK, here's where all the real work gets done

# set our JVM vars
set_jvm_vars

# check to see if there's an already running instance or not
ALREADY_RUNNING=`check_running`

# If there is no command line argument at all then try to open a new
# window in an already running instance.
if [ "${ALREADY_RUNNING}" -eq "1" ] && [ -z "$1" ]; then
  exec $MOZ_PROGRAM -remote "xfeDoCommand(openBrowser)" 2>/dev/null >/dev/null
fi

# if there's no command line argument and there's not a running
# instance then just fire up a new copy of the browser
if [ -z "$1" ]; then
  exec $MOZ_PROGRAM 2>/dev/null >/dev/null
fi

unset RETURN_VAL

# If there's a command line argument but it doesn't begin with a -
# it's probably a url.  Try to send it to a running instance.

USE_EXIST=0
opt="$1"
case "$opt" in
  -mail)
      open_mail ${1+"$@"}
      ;;
  -compose)
      open_compose ${1+"$@"}
      ;;
  -*) ;;
  *) USE_EXIST=1 ;;
esac

if [ "${USE_EXIST}" -eq "1" ] && [ "${ALREADY_RUNNING}" -eq "1" ]; then
  # check to make sure that the command contains at least a :/ in it.
  echo $opt | grep -e ':/' 2>/dev/null > /dev/null
  RETURN_VAL=$?
  if [ "$RETURN_VAL" -eq "1" ]; then
    # if it doesn't begin with a '/' and it exists when the pwd is
    # prepended to it then append the full path
    echo $opt | grep -e '^/' 2>/dev/null > /dev/null
    if [ "${RETURN_VAL}" -ne "0" ] && [ -e `pwd`/$opt ]; then
      opt="`pwd`/$opt"
    fi
    exec $MOZ_PROGRAM -remote "openurl($opt)" 2>/dev/null >/dev/null
  fi
  # just pass it off if it looks like a url
  exec $MOZ_PROGRAM -remote "openurl($opt,new-window)" 2>/dev/null >/dev/null
fi

exec $MOZ_PROGRAM ${1+"$@"}
