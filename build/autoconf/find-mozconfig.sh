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

# find-mozconfig.sh - Loads options from .mozconfig onto configure's
#    command-line. The .mozconfig file is searched for in the 
#    order:
#       if $MOZCONFIG is set, use that.
#       Otherwise, use $TOPSRCDIR/.mozconfig
#       Otherwise, use $HOME/.mozconfig
#
topsrcdir=`cd \`dirname $0\`/../..; pwd`

for _config in $MOZCONFIG \
               $MOZ_MYCONFIG \
               $topsrcdir/.mozconfig \
               $topsrcdir/mozconfig \
               $topsrcdir/mozconfig.sh \
               $topsrcdir/myconfig.sh \
               $HOME/.mozconfig \
               $HOME/.mozconfig.sh \
               $HOME/.mozmyconfig.sh
do
  if test -f $_config; then
    echo $_config;
    exit 0
  fi
done
