#!/bin/sh
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   jim_nance@yahoo.com
#
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

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

