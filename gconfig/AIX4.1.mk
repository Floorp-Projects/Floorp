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
# Config stuff for AIX4.1
#

include $(GDEPTH)/gconfig/AIX.mk

#
# Special link info for constructing AIX programs. On AIX we have to
# statically link programs that use NSPR into a single .o, rewriting the
# calls to select to call "aix". Once that is done we then can
# link that .o with a .o built in nspr which implements the system call.
#

AIX_LINK_OPTS  += -bnso -berok
#AIX_LINK_OPTS += -bnso -berok -brename:.select,.wrap_select -brename:.poll,.wrap_poll -bI:/usr/lib/syscalls.exp

# The AIX4.1 linker had a bug which always looked for a dynamic library
# with an extension of .a.  AIX4.2 fixed this problem
DLL_SUFFIX = a

OS_LIBS		+= -lsvld
