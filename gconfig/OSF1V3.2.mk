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
# On OSF1 V3.2, classic nspr is the default (and only) implementation
# strategy.
#

#
# Config stuff for DEC OSF/1 V3.2
#
include $(GDEPTH)/gconfig/OSF1.mk

ifeq ($(OS_RELEASE),V3.2)
	OS_CFLAGS += -DOSF1V3
endif
