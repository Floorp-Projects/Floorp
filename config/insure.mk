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

INSURE_MATCH_SCRIPT=$(topsrcdir)/build/autoconf/match-dir.sh

INSURE_EXCLUDE=$(shell $(INSURE_MATCH_SCRIPT) $(MOZ_INSURE_EXCLUDE_DIRS))

INSURE_INCLUDE=$(shell $(INSURE_MATCH_SCRIPT) $(MOZ_INSURE_DIRS))

ifeq ($(INSURE_EXCLUDE),0)

ifeq ($(INSURE_INCLUDE),1)
CC		= $(MOZ_INSURE) gcc
CXX		= $(MOZ_INSURE) g++
endif # INSURE_INCLUDE == 1

endif # INSURE_EXCLUDE == 0
