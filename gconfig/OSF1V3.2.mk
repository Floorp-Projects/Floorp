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
