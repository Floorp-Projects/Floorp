#!gmake
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

#######################################################################
# (1) Directory specific info                                         #
#######################################################################

DEPTH = ../..

CPPSRCS = 	efmain.cpp			\
		$(NULL)

PROGRAM = sajava

include $(DEPTH)/config/config.mk

CFLAGS += -DIMPORTING_VM_FILES

LDFLAGS += $(NSPR_LIBS) $(EF_LIBS)

include $(DEPTH)/config/rules.mk

program:: $(PROGRAM)

ifdef GENERATE_BROWSE_INFO
program:: $(BROWSE_INFO_FILE)
endif

$(PROGRAM): $(EF_LIB_FILES)


