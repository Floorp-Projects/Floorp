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
#

INCLUDED_INSURE_MK = 1

INSURE_MATCH_SCRIPT=$(topsrcdir)/build/autoconf/match-dir.sh

INSURE_EXCLUDE=$(shell $(INSURE_MATCH_SCRIPT) $(MOZ_INSURE_EXCLUDE_DIRS))

INSURE_INCLUDE=$(shell $(INSURE_MATCH_SCRIPT) $(MOZ_INSURE_DIRS))

ifeq ($(INSURE_EXCLUDE),0)

ifeq ($(INSURE_INCLUDE),1)
CC		:= $(MOZ_INSURE)
CXX		:= $(MOZ_INSURE)
endif # INSURE_INCLUDE == 1

endif # INSURE_EXCLUDE == 0
