#! gmake
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

include $(CORE_DEPTH)/coreconf/ruleset.mk

# Set TARGETS to null so that the coreconf rules work 
TARGETS =

# This is actually our modified version of 
# $(CORE_DEPTH)/coreconf/rules.mk, and should
# hopefully be temporary until we can get the
# coreconf people to merge our changes with theirs
include $(DEPTH)/config/corerules.mk

ifneq ($(MODULE_NAME),)

# OBJFILTERS is neccesary for the ifneq check below 
# since OBJS seems to have an undetermined number of blanks in it
OBJFILTERS = $(filter %$(OBJ_SUFFIX),$(OBJS))

ifneq ($(OBJFILTERS),)
MODULE_FILE = $(OBJDIR)/moduleFile
$(MODULE_FILE): $(OBJS)
	@$(MKDIR)  $(DEPTH)/$(OBJDIR)/$(MODULE_NAME)
	@$(MAKE_OBJDIR)
	cp -f $? $(DEPTH)/$(OBJDIR)/$(MODULE_NAME)
	touch $(MODULE_FILE)

libs:: $(MODULE_FILE)

clean::
	rm -f $(MODULE_FILE)
endif

endif


ifneq ($(HEADER_GEN),)
$(HEADER_INCLUDES): 
	@$(MKDIR) $(HEADER_GEN_DIR)
	$(JAVAH) -classpath $(DEPTH)/../dist/classes -d $(HEADER_GEN_DIR) $(HEADER_GEN)

$(OBJS) : $(HEADER_INCLUDES)	
endif


ifneq ($(filter %.h,$(LOCAL_EXPORTS)),)
LOCAL_EXPORT_FILES = $(addprefix $(LOCAL_EXPORT_DIR)/,$(LOCAL_EXPORTS))

$(LOCAL_EXPORT_DIR)/%.h : %.h
	@$(MKDIR) $(LOCAL_EXPORT_DIR)
	@rm -f $(LOCAL_EXPORT_DIR)/$<
	$(LN) $(CUR_DIR)/$< $(LOCAL_EXPORT_DIR)/$<

export:: $(LOCAL_EXPORT_FILES)
endif


ifneq ($(filter %.h,$(LOCAL_MD_EXPORTS_x86)),)
LOCAL_MD_x86_EXPORT_FILES = $(addprefix $(LOCAL_EXPORT_DIR)/md/x86/,$(LOCAL_MD_EXPORTS_x86))

$(LOCAL_EXPORT_DIR)/md/x86/%.h : %.h
	@$(MKDIR) $(LOCAL_EXPORT_DIR)/md/x86
	@rm -f $(LOCAL_EXPORT_DIR)/md/x86/$<
	$(LN) $(CUR_DIR)/$< $(LOCAL_EXPORT_DIR)/md/x86/$<

export:: $(LOCAL_MD_x86_EXPORT_FILES)
endif

ifneq ($(filter %.h,$(LOCAL_MD_EXPORTS_ppc)),)
export:: 
	@$(MKDIR) $(LOCAL_EXPORT_DIR)/md/ppc
	@$(LN) $(LOCAL_MD_EXPORTS_ppc) $(LOCAL_EXPORT_DIR)/md/ppc
endif

ifneq ($(filter %.h,$(LOCAL_MD_EXPORTS_sparc)),)
export:: 
	@$(MKDIR) $(LOCAL_EXPORT_DIR)/md/sparc
	@$(LN) $(LOCAL_MD_EXPORTS_sparc) $(LOCAL_EXPORT_DIR)/md/sparc
endif

ifneq ($(filter %.h,$(LOCAL_MD_EXPORTS_hppa)),)
export:: 
	@$(MKDIR) $(LOCAL_EXPORT_DIR)/md/hppa
	@$(LN) $(LOCAL_MD_EXPORTS_hppa) $(LOCAL_EXPORT_DIR)/md/hppa/$<
endif

# Browse information
$(BROWSE_INFO_FILE): $(BROWSE_INFO_OBJS)
	$(BROWSE_INFO_PROGRAM) $(BROWSE_INFO_FLAGS)  -o $(BROWSE_INFO_FILE) $(BROWSE_INFO_OBJS)
	@cp $(BROWSE_INFO_FILE) $(DIST)/lib

# Remove additional ef-specific junk
clean::
	rm -f $(OBJDIR)/vc50*.*
	rm -f $(OBJDIR)/BrowseInfo/*.sbr

clobber::
	rm -r -f $(OBJDIR)/BrowseInfo

ifneq ($(LIBRARIES),)

# Add the lib prefix on all platforms except WINNT

ifeq ($(OS_ARCH),WINNT)
LIBRARIES := $(addprefix $(DIST)/lib/,$(LIBRARIES))
LIBRARIES := $(addsuffix .lib,$(LIBRARIES))
else
LIBRARIES := $(addprefix -l,$(LIBRARIES))
endif

LDFLAGS += $(LIBRARIES)
endif



 
