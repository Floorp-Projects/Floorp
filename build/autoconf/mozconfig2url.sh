#! /bin/sh
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
# Copyright (C) 1999 Netscape Communications Corporation.  All Rights
# Reserved.
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
    # Escape shell characters, space, tab, dollar, quote, backslash,
    # and substitute '@<word>@' with '$(<word)'.
    _opt=`echo $_opt | sed -e 's/\([\ \	\$\"\\]\)/\\\\\1/g;'`
    case "$_opt" in
      --*-*= ) query_string="$query_string$_opt&"     ;;
      --*-* )  query_string="$query_string$_opt=yes&" ;;
    esac
  done
}

mk_add_options() {
  for _opt
  do
    # Escape shell characters, space, tab, dollar, quote, backslash,
    # and substitute '@<word>@' with '$(<word)'.
    query_string=$query_string`echo "$_opt&" | sed -e 's/\([\ \	\$\"\\]\)/\\\\\1/g;'`
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

