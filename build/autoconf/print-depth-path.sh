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
#     jim_nance@yahoo.com
#

#
# This script will print the depth path for a mozilla directory based
# on the info in Makefile
#
# Its a hack.  Its brute force.  Its horrible.  
# It dont use Artificial Intelligence.  It dont use Virtual Reality.
# Its not perl.  Its not python.   But it works.
#
# Usage: print-depth-path.sh
#
# Send comments, improvements, bugs to jim_nance@yahoo.com
# 

# Make sure a Makefile exists
if [ ! -f Makefile ]
then
	echo
	echo "There ain't no 'Makefile' over here: $pwd, dude."
	echo

	exit
fi

# awk can be quite primitave.  Try enhanced versions first
for AWK in gawk nawk awk; do
    if type $AWK 2>/dev/null 1>/dev/null; then
        break;
    fi
done

$AWK -v PWD=`pwd` '
{
    if($1 == "DEPTH") {
        DEPTH=$0
    }
}

END {
    sub("^.*DEPTH.*=[ \t]*", "", DEPTH)
    dlen = split(DEPTH, darray, "/")
    plen = split(PWD,   parray, "/")

    fsep=""
    for(i=plen-dlen; i<=plen; i++) {
        printf("%s%s", fsep, parray[i])
        fsep="/"
    }
    printf("\n")
}' Makefile

