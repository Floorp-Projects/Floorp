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

