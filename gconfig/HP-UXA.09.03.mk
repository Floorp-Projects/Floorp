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
# On HP-UX 9, the default (and only) implementation strategy is
# classic nspr.
#
ifeq ($(OS_RELEASE),A.09.03)
	DEFAULT_IMPL_STRATEGY	 = _CLASSIC
endif

#
# Config stuff for HP-UXA.09.03
#
include $(GDEPTH)/gconfig/HP-UXA.09.mk
