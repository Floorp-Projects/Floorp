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

sopath=$1
libname=$2;

# output the library name
echo -l${libname}

# check if the requested lib is in the LIBRARY_PATH search path
for dir in $(echo $LIBRARY_PATH | sed 's/:/ /g') ;
do
	if test -e ${dir}/lib${libname}.so ; then
		exit ;
	fi
done

# switch to executable output path
cd ${sopath}

# check if the requested lib is in the executable output path
if test -e lib${libname}.so ; then
	exit ;
fi

# generate stub

echo "stub(){return(0);}">${libname}.c
c++ -nostart ${libname}.c -olib${libname}.so
rm ${libname}.c

