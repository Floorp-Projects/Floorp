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

##############################################################################
##
## Name:		xmversion.sh - a fast way to get a motif lib's version
##
## Description:	Print the major and minor version numbers for motif libs on
##				the system that executes the script.  Could be tweaked
##				to output more info.
##
##              More checks need to be added for more platforms.  
##              Currently this script is only useful in Linux Universe
##              where there are a many versions of motif.
##
## Author:		Ramiro Estrugo <ramiro@netscape.com>
##
##############################################################################

TEST_GLIBC_PROG=./test_glibc
TEST_GLIBC_SRC=test_glibc.c

rm -f $TEST_GLIBC_PROG
rm -f $TEST_GLIBC_SRC

cat << EOF > test_glibc.c
#include <stdio.h>

int main(int argc,char ** argv) 
{
#ifdef 	__GLIBC__
	fprintf(stdout,"%d\n",__GLIBC__);
#else
	fprintf(stdout,"none\n");
#endif

	return 0;
}
EOF

if [ ! -f $TEST_GLIBC_SRC ]
then
	echo
	echo "Could not create test program source $TEST_GLIBC_SRC."
	echo

	exit
fi

cc -o $TEST_GLIBC_PROG $TEST_GLIBC_SRC

if [ ! -x $TEST_GLIBC_PROG ]
then
	echo
	echo "Could not create test program $TEST_GLIBC_PROG."
	echo

	exit
fi

$TEST_GLIBC_PROG

rm -f $TEST_GLIBC_PROG
rm -f $TEST_GLIBC_SRC

