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
# this is a hackish script used from config/rules.mk under BeOS
# to generate a stub for a library if it doesn't exist yet

apppath=$1

echo ${apppath}/_APP_

# switch to executable output path
cd ${apppath}

# check if the app stub exists
if test -e _APP_ ; then
	exit ;
fi

# generate stub

echo "main(int, char**){return(0);}">_APP_.c
c++ _APP_.c
rm _APP_.c

