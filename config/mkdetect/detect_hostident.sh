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
## Name: detect_hostident.sh - Print a uniq ident that describes this host
##
## Description:	Used by other scripts/makefiles.  Its simple, but will get
##              complicated when new platforms are added.
##
## Author: Ramiro Estrugo <ramiro@netscape.com>
##
##############################################################################

HOSTIDENT_ARCH=`uname -s`

# Determine the host name
if [ "$HOSTIDENT_ARCH" = "Linux" ]
then
	HOSTIDENT_HOSTNAME=`hostname -s`
else
	HOSTIDENT_HOSTNAME=`hostname`
fi

# A uniq hostidentifier that describes this host
echo $HOSTIDENT_HOSTNAME
