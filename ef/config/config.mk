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

ifndef EF_CONFIG_MK

EF_CONFIG_MK=1

CORE_DEPTH = $(DEPTH)/..

include $(DEPTH)/config/coreconfig.mk
include $(DEPTH)/config/arch.mk
include $(DEPTH)/config/$(OS_CONFIG).mk

ifeq ($(OS_ARCH),WINNT)
	CCC = cl
	EXC_FLAGS = -GX
	OS_DLLFLAGS = -nologo -DLL -incremental:yes -subsystem:console -machine:I386 wsock32.lib
	EF_LIBS = $(DIST)/lib/EF.lib
	EF_LIB_FILES = $(EF_LIBS)
	NSPR_LIBS = $(DIST)/lib/libnspr21.lib $(DIST)/lib/libplc21_s.lib
	BROWSE_INFO_DIR = $(DEPTH)/$(OBJDIR)/BrowseInfo
	BROWSE_INFO_OBJS = $(wildcard $(BROWSE_INFO_DIR)/*.sbr)
	BROWSE_INFO_PROGRAM = bscmake 
	BROWSE_INFO_FLAGS = -nologo -incremental:yes
	BROWSE_INFO_FILE = $(BROWSE_INFO_DIR)/ef.bsc
	YACC = "$(NSTOOLS)/bin/yacc$(PROG_SUFFIX)" -l -b y.tab
	LN = "$(NSTOOLS)/bin/ln$(PROG_SUFFIX)" -f

# Flag to generate pre-compiled headers
	CFLAGS += -YX -Fp$(OBJDIR)/ef.pch -Fd$(OBJDIR)/ef.pdb
	MKDIR = mkdir
else
	CCC = gcc
	AS = gcc
	ASFLAGS += -x assembler-with-cpp
	CFLAGS += -fdollars-in-identifiers 
	EXC_FLAGS = -fhandle-exceptions
	EF_LIBS = -lEF 
	EF_LIB_FILES = $(DIST)/lib/libEF.a
	NSPR_LIBS = -L$(DIST)/lib -lnspr21 -lplc21
	MKDIR = mkdir -p
	LN = ln -s -f
endif

CFLAGS += $(EXC_FLAGS)
#LDFLAGS += $(NSPR_LIBS)

ifeq ($(CPU_ARCH),x86)
ARCH_DEFINES	+= -DGENERATE_FOR_X86
NO_GENERIC	= 1
endif

ifeq ($(CPU_ARCH),ppc)
ARCH_DEFINES	+= -DGENERATE_FOR_PPC
endif

ifeq ($(CPU_ARCH),sparc)
# No CodeGenerator for sparc yet.
ARCH_DEFINES    += -DGENERATE_FOR_X86
CPU_ARCH		= x86
endif

ifeq ($(CPU_ARCH),hppa)
# No CodeGenerator for hppa yet.
ARCH_DEFINES    += -DGENERATE_FOR_X86
CPU_ARCH		= x86
endif

ifneq ($(NO_GENERIC),1)
GENERIC	= generic
endif

ARCH_DEFINES	+= -DTARGET_CPU=$(CPU_ARCH)
CFLAGS += $(ARCH_DEFINES)


#
# Some tools.
#
NFSPWD = 		$(DEPTH)/config/$(OBJDIR)/nfspwd
JAVAH =			$(DIST)/bin/javah$(PROG_SUFFIX)
BURG = 			$(DEPTH)/Tools/Burg/$(OBJDIR)/burg$(PROG_SUFFIX)
PERL = 			perl

HEADER_GEN_DIR = 	$(DEPTH)/GenIncludes
LOCAL_EXPORT_DIR = 	$(DEPTH)/LocalIncludes

INCLUDES += -I $(LOCAL_EXPORT_DIR) -I $(LOCAL_EXPORT_DIR)/md/$(CPU_ARCH) -I $(DIST)/include -I $(DEPTH)/Includes

ifneq ($(HEADER_GEN),)
INCLUDES += -I $(HEADER_GEN_DIR)
CFLAGS += -DIMPORTING_VM_FILES
PACKAGE_CLASSES	= 	$(HEADER_GEN)
PATH_CLASSES	= 	$(subst .,/,$(PACKAGE_CLASSES))
CLASS_FILE_NAMES = 	$(subst .,_,$(PACKAGE_CLASSES))
HEADER_INCLUDES = 	$(patsubst %,$(HEADER_GEN_DIR)/%.h,$(CLASS_FILE_NAMES))
endif

MKDEPEND_DIR    = $(DEPTH)/config/mkdepend
MKDEPEND        = $(MKDEPEND_DIR)/$(OBJDIR_NAME)/mkdepend
MKDEPENDENCIES  = $(OBJDIR_NAME)/depend.mk

endif


