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



#-----------------------------------------------------------------------------
#    cxxlink-filter.sh
#
#    Created: David Williams <djw@netscape.com>, 18-Jul-1996
#
#    C++ Link line filter. This guy attempts to "fix" the arguments that
#    are passed to the linker. It should probably know what platform it
#    is on, but it doesn't need to (yet) because we only use it for
#    Cfront drivers, and the names are the same on all the Cfront platforms.
#    
#-----------------------------------------------------------------------------


ARGS_IN=$*
ARGS_OUT=
for arg in $ARGS_IN
do
	case X$arg in
		X-lC)	# convert to .a
				arg=/usr/lib/libC.a
				;;
		X-lC.ansi)	# convert to .a
				arg=/usr/lib/libC.ansi.a
				;;
		*)		;;
	esac
	ARGS_OUT="$ARGS_OUT $arg"
done
exec ld $ARGS_OUT
