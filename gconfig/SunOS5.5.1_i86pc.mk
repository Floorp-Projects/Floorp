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
# Config stuff for Solaris 2.5.1 on x86
# 

SOL_CFLAGS	= -D_SVID_GETTOD

include $(GDEPTH)/gconfig/SunOS5.mk

CPU_ARCH		= x86
OS_DEFINES		+= -Di386

ifeq ($(OS_RELEASE),5.5.1_i86pc)
	OS_DEFINES += -DSOLARIS2_5
endif
