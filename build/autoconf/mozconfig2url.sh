#! /bin/sh
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
# Copyright (C) 1999 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 
#

# mozconfig2defs.sh - Translates .mozconfig into options for client.mk.
#    Prints defines to stdout.
#
# See load-mozconfig.sh for more details
#
# Send comments, improvements, bugs to Steve Lamm (slamm@netscape.com).

ac_add_options() {
  for _opt
  do
    # Escape special url characters
    _opt=`echo $_opt | sed -e 's/%/%25/g;s/ /%20/g;s/&/%26/g;'`
    case "$_opt" in
      --*-*= ) query_string="$query_string$_opt&"     ;;
      --*-* )  query_string="$query_string$_opt=yes&" ;;
    esac
  done
}

mk_add_options() {
  for _opt
  do
    # Escape special url characters
    # Escape special url characters
    _opt=`echo $_opt | sed -e 's/%/%25/g;s/ /%20/g;s/&/%26/g;'`
    query_string="$query_string$_opt&"
  done
}

#
# main
#

# find-mozconfig.sh 
#   In params:   $MOZCONFIG $HOME ($MOZ_MYCONFIG)
scriptdir=`dirname $0`
find_mozconfig="$scriptdir/find-mozconfig.sh"
if [ ! -f $find_mozconfig ]
then
  (cd $scriptdir/../../..; cvs co mozilla/build/autoconf/find-mozconfig.sh)
fi

MOZCONFIG=`$find_mozconfig`

if [ "$MOZCONFIG" ]
then
  query_string="?"
  . $MOZCONFIG

  # Drop the last character of $query_string
  echo `expr "$query_string" : "\(.*\)."`
fi

