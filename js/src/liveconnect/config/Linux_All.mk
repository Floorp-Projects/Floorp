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

# JDK_DIR should be the directory you put the JDK in, and should have
# the appropriate lib/ and include/ dirs on it.
# If you're not using the `Blackdown' JDK, try changing the following line:
JDK=/usr/lib/jdk-1.1.5

INCLUDES   += -I$(JDK)/include -I$(JDK)/include/md \
	      -I$(JDK)/include/genunix

OTHER_LIBS += -L$(JDK)/lib/i386/green_threads -ljava
