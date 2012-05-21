#!/bin/sh
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# This script will print the depth path for a mozilla directory based
# on the info in Makefile
#
# It's a hack.  It's brute force.  It's horrible.  
# It don't use Artificial Intelligence.  It don't use Virtual Reality.
# It's not perl.  It's not python.   But it works.
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

