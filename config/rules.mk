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
################################################################################
#
# We now use a 2-pass build system.  This needs to be re-thought....
#
# Pass 1. export  - Create generated headers and stubs.  Publish public headers
#                   to dist/include.
#
# Pass 2. libs - Create libraries & programs.  Publish them to dist/bin.
#
# Parameters to this makefile (set these before including):
#
# a)
#	TARGETS	-- the target to create
#			(defaults to $LIBRARY $PROGRAM)
# b)
#	DIRS	-- subdirectories for make to recurse on
#			(the 'all' rule builds $TARGETS $DIRS)
# c)
#	CSRCS, CPPSRCS -- .c and .cpp files to compile
#			(used to define $OBJS)
# d)
#	PROGRAM	-- the target program name to create from $OBJS
# d2)
#	SIMPLE_PROGRAMS	-- Compiles Foo.cpp Bar.cpp into Foo, Bar executables.
# e)
#	LIBRARY_NAME	-- the target library name to create from $OBJS
#
################################################################################
ifndef topsrcdir
topsrcdir		= $(DEPTH)
endif

ifndef MOZILLA_DIR
MOZILLA_DIR = $(topsrcdir)
endif

ifndef INCLUDED_CONFIG_MK
include $(topsrcdir)/config/config.mk
endif

ifdef CROSS_COMPILE
HOST_AR_FLAGS		= $(AR_FLAGS)
endif

ifndef INCLUDED_VERSION_MK
include $(topsrcdir)/config/version.mk
endif

REPORT_BUILD = @echo $(notdir $<)

ifeq ($(OS_ARCH),OS2)
EXEC			=
else
EXEC			= exec
endif

# ELOG prints out failed command when building silently (gmake -s).
ifneq (,$(findstring s,$(MAKEFLAGS)))
  ELOG := $(EXEC) sh $(BUILD_TOOLS)/print-failed-commands.sh
else
  ELOG :=
endif

#
# Library rules
#
# If BUILD_SHARED_LIBS or FORCE_SHARED_LIB is set and 
#    FORCE_STATIC_LIB is not set, 
#	the shared library will be built.
# If BUILD_STATIC_LIBS or FORCE_STATIC_LIB is set, 
#	the static library will  be built.
#

ifeq ($(MOZ_OS2_TOOLS),VACPP)
_EXTRA_DSO_RELATIVE_PATHS=1
else
ifeq (_WINNT,$(GNU_CC)_$(OS_ARCH))
_NO_AUTO_VARS=1
_EXTRA_DSO_RELATIVE_PATHS=1
endif
endif

ifdef _EXTRA_DSO_RELATIVE_PATHS
EXTRA_DSO_LIBS		:= $(addsuffix .$(LIB_SUFFIX),$(addprefix $(DIST)/lib/$(LIB_PREFIX),$(EXTRA_DSO_LIBS)))
EXTRA_DSO_LIBS		:= $(filter-out %/bin %/lib,$(EXTRA_DSO_LIBS))
EXTRA_DSO_LDOPTS    := $(patsubst -l%,$(DIST)/lib/%.$(LIB_SUFFIX),$(EXTRA_DSO_LDOPTS))
LIBS                := $(patsubst -l%,$(DIST)/lib/$(LIB_PREFIX)%.$(LIB_SUFFIX),$(LIBS))
else
EXTRA_DSO_LIBS		:= $(addprefix -l,$(EXTRA_DSO_LIBS))
endif

ifndef LIBRARY
ifdef LIBRARY_NAME
ifneq (,$(filter OS2 WINNT,$(OS_ARCH)))
ifdef SHORT_LIBNAME
LIBRARY_NAME		:= $(SHORT_LIBNAME)
endif
endif
LIBRARY			:= $(LIB_PREFIX)$(LIBRARY_NAME).$(LIB_SUFFIX)
endif
endif

ifndef HOST_LIBRARY
ifdef HOST_LIBRARY_NAME
HOST_LIBRARY		:= $(LIB_PREFIX)$(HOST_LIBRARY_NAME).$(LIB_SUFFIX)
endif
endif

ifdef LIBRARY
ifneq (,$(BUILD_SHARED_LIBS)$(FORCE_SHARED_LIB))
ifdef MKSHLIB

# Unix only
ifeq (,$(filter-out BeOS OS2 WINNT, $(OS_ARCH)))
ifdef LIB_IS_C_ONLY
MKSHLIB			= $(MKCSHLIB)
endif
endif

SHARED_LIBRARY		:= $(LIBRARY:.$(LIB_SUFFIX)=$(DLL_SUFFIX))

ifeq ($(OS_ARCH),OS2)
DEF_OBJS		= $(OBJS)
ifneq ($(EXPORT_OBJS),1)
DEF_OBJS		+= $(SHARED_LIBRARY_LIBS)
endif
DEF_FILE		:= $(SHARED_LIBRARY:.dll=.def)
endif

ifneq (,$(filter OS2 WINNT,$(OS_ARCH)))
IMPORT_LIBRARY		:= $(SHARED_LIBRARY:.dll=.lib)
endif

endif # MKSHLIB
endif # BUILD_SHARED_LIBS || FORCE_SHARED_LIB
endif # LIBRARY

ifeq (,$(BUILD_STATIC_LIBS)$(FORCE_STATIC_LIB))
LIBRARY			:= $(NULL)
endif

ifeq (,$(BUILD_SHARED_LIBS)$(FORCE_SHARED_LIB))
SHARED_LIBRARY		:= $(NULL)
DEF_FILE		:= $(NULL)
IMPORT_LIBRARY		:= $(NULL)
endif

ifdef FORCE_STATIC_LIB
ifndef FORCE_SHARED_LIB
SHARED_LIBRARY		:= $(NULL)
DEF_FILE		:= $(NULL)
IMPORT_LIBRARY		:= $(NULL)
endif
endif

ifdef FORCE_SHARED_LIB
ifndef FORCE_STATIC_LIB
LIBRARY			:= $(NULL)
endif
endif

ifeq ($(OS_ARCH),WINNT)

ifdef LIBRARY_NAME
PDBFILE=$(LIBRARY_NAME).pdb
ifdef MOZ_DEBUG
CODFILE=$(LIBRARY_NAME).cod
endif
else
PDBFILE=$(basename $(@F)).pdb
ifdef MOZ_DEBUG
CODFILE=$(basename $(@F)).cod
endif
endif

ifdef MOZ_MAPINFO
ifdef LIBRARY_NAME
MAPFILE=$(LIBRARY_NAME).map
else
MAPFILE=$(basename $(@F)).map
endif # LIBRARY_NAME
endif # MOZ_MAPINFO

ifdef DEFFILE
CFLAGS += /DEF:$(DEFFILE)
CXXFLAGS += /DEF:$(DEFFILE)
DSO_LDOPTS += /DEF:$(DEFFILE)
endif

ifdef MAPFILE
OS_LDFLAGS += /MAP:$(MAPFILE) /MAPINFO:LINES
#CFLAGS += -Fm$(MAPFILE)
#CXXFLAGS += -Fm$(MAPFILE)
endif

#ifdef CODFILE
#CFLAGS += -Fa$(CODFILE) -FAsc
#CFLAGS += -Fa$(CODFILE) -FAsc
#endif

endif # WINNT

ifndef TARGETS
TARGETS			= $(LIBRARY) $(SHARED_LIBRARY) $(PROGRAM) $(SIMPLE_PROGRAMS) $(HOST_LIBRARY) $(HOST_PROGRAM) $(HOST_SIMPLE_PROGRAMS)
endif

ifndef OBJS
_OBJS			= \
	$(JRI_STUB_CFILES) \
	$(addsuffix .$(OBJ_SUFFIX), $(JMC_GEN)) \
	$(CSRCS:.c=.$(OBJ_SUFFIX)) \
	$(CPPSRCS:.cpp=.$(OBJ_SUFFIX)) \
	$(CMMSRCS:.mm=.$(OBJ_SUFFIX)) \
	$(ASFILES:.$(ASM_SUFFIX)=.$(OBJ_SUFFIX))
OBJS	= $(strip $(_OBJS))
endif

ifndef HOST_OBJS
HOST_OBJS		= $(addprefix host_,$(HOST_CSRCS:.c=.o))
endif

ifeq ($(OS_ARCH),OS2)
LIBOBJS			:= $(OBJS)
else
LIBOBJS			:= $(addprefix \", $(OBJS))
LIBOBJS			:= $(addsuffix \", $(LIBOBJS))
endif

ifndef MOZ_AUTO_DEPS
ifneq (,$(OBJS)$(SIMPLE_PROGRAMS))
MDDEPFILES		= $(addprefix $(MDDEPDIR)/,$(OBJS:.$(OBJ_SUFFIX)=.pp))
endif
endif

ALL_TRASH = \
	$(GARBAGE) $(TARGETS) $(OBJS) $(PROGOBJS) LOGS TAGS a.out \
	$(HOST_PROGOBJS) $(HOST_OBJS) $(IMPORT_LIBRARY) $(DEF_FILE)\
	$(EXE_DEF_FILE) so_locations _gen _stubs $(wildcard *.res) \
	$(wildcard *.pdb) $(CODFILE) $(MAPFILE) $(IMPORT_LIBRARY) \
	$(SHARED_LIBRARY:$(DLL_SUFFIX)=.exp) \
	$(PROGRAM:$(BIN_SUFFIX)=.exp) $(SIMPLE_PROGRAMS:$(BIN_SUFFIX)=.exp) \
	$(PROGRAM:$(BIN_SUFFIX)=.lib) $(SIMPLE_PROGRAMS:$(BIN_SUFFIX)=.lib) \
	$(SIMPLE_PROGRAMS:$(BIN_SUFFIX)=.$(OBJ_SUFFIX)) \
	$(wildcard gts_tmp_*) $(LIBRARY:%.a=.%.timestamp)
ALL_TRASH_DIRS = \
	$(GARBAGE_DIRS) /no-such-file

ifdef QTDIR
GARBAGE			+= $(MOCSRCS)
endif

ifdef SIMPLE_PROGRAMS
GARBAGE			+= $(SIMPLE_PROGRAMS:%=%.$(OBJ_SUFFIX))
endif

ifdef HOST_SIMPLE_PROGRAMS
GARBAGE			+= $(addprefix host_,$(HOST_SIMPLE_PROGRAMS:%=%.o))
endif

#
# the Solaris WorkShop template repository cache.  it occasionally can get
# out of sync, so targets like clobber should kill it.
#
ifeq ($(OS_ARCH),SunOS)
ifeq ($(GNU_CXX),)
GARBAGE_DIRS += SunWS_cache
endif
endif

ifeq ($(OS_ARCH),OpenVMS)
GARBAGE			+= $(wildcard *.c*_defines) 
ifdef SHARED_LIBRARY
VMS_SYMVEC_FILE		= $(SHARED_LIBRARY:$(DLL_SUFFIX)=_symvec.opt)
VMS_SYMVEC_FILE_MODULE	= $(topsrcdir)/build/unix/vms/$(notdir $(VMS_SYMVEC_FILE))
VMS_SYMVEC_FILE_COMP	= $(topsrcdir)/build/unix/vms/component_symvec.opt
GARBAGE			+= $(VMS_SYMVEC_FILE)
ifdef IS_COMPONENT
DSO_LDOPTS := $(filter-out -auto_symvec,$(DSO_LDOPTS)) $(VMS_SYMVEC_FILE)
endif
endif
endif

XPIDL_GEN_DIR		= _xpidlgen

ifdef MOZ_UPDATE_XTERM
# Its good not to have a newline at the end of the titlebar string because it
# makes the make -s output easier to read.  Echo -n does not work on all
# platforms, but we can trick sed into doing it.
UPDATE_TITLE = sed -e "s!Y!$@ in $(shell $(BUILD_TOOLS)/print-depth-path.sh)/$$d!" $(MOZILLA_DIR)/config/xterm.str;
endif

EXIT_ON_ERROR := set -e; # Shell loops continue past errors without this.

ifdef DIRS
LOOP_OVER_DIRS = \
    @$(EXIT_ON_ERROR) \
    for d in $(DIRS); do \
        $(UPDATE_TITLE) \
        $(MAKE) -C $$d $@; \
    done

LOOP_OVER_MOZ_DIRS = \
    @$(EXIT_ON_ERROR) \
    for d in $(filter-out $(STATIC_MAKEFILES), $(DIRS)); do \
        $(UPDATE_TITLE) \
        $(MAKE) -C $$d $@; \
    done

endif

#
# Now we can differentiate between objects used to build a library, and
# objects used to build an executable in the same directory.
#
ifndef PROGOBJS
PROGOBJS		= $(OBJS)
endif

ifndef HOST_PROGOBJS
HOST_PROGOBJS		= $(HOST_OBJS)
endif

# SUBMAKEFILES: List of Makefiles for next level down.
#   This is used to update or create the Makefiles before invoking them.
ifneq ($(DIRS),)
SUBMAKEFILES		:= $(addsuffix /Makefile, $(filter-out $(STATIC_MAKEFILES), $(DIRS)))
endif

# MAKE_DIRS: List of directories to build while looping over directories.
ifneq (,$(OBJS)$(SIMPLE_PROGRAMS))
MAKE_DIRS		+= $(MDDEPDIR)
GARBAGE_DIRS		+= $(MDDEPDIR)
endif

ifneq ($(XPIDLSRCS),)
GARBAGE_DIRS		+= $(XPIDL_GEN_DIR)
endif

#
# Tags: emacs (etags), vi (ctags)
# TAG_PROGRAM := ctags -L -
#
TAG_PROGRAM		= xargs etags -a

#
# Turn on C++ linking if we have any .cpp files
# (moved this from config.mk so that config.mk can be included 
#  before the CPPSRCS are defined)
#
ifdef CPPSRCS
CPP_PROG_LINK		= 1
endif

#
# Make sure to wrap static libs inside linker specific flags to turn on & off
# inclusion of all symbols inside the static libs
#
ifndef NO_LD_ARCHIVE_FLAGS
ifdef SHARED_LIBRARY_LIBS
EXTRA_DSO_LDOPTS := $(MKSHLIB_FORCE_ALL) $(SHARED_LIBRARY_LIBS) $(MKSHLIB_UNFORCE_ALL) $(EXTRA_DSO_LDOPTS)
endif
endif

#
# This will strip out symbols that the component shouldnt be 
# exporting from the .dynsym section.
#
ifdef IS_COMPONENT
EXTRA_DSO_LDOPTS += $(MOZ_COMPONENTS_VERSION_SCRIPT_LDFLAGS)
endif # IS_COMPONENT

#
# Allow components to be installed into a secondary path 
# that is not searched by xpcom.
ifndef COMPONENTS_PATH
ifdef INACTIVE_COMPONENT
COMPONENTS_PATH = components_inactive
else
COMPONENTS_PATH = components
endif
endif

#
# MacOS X specific stuff
#

ifeq ($(OS_ARCH),Darwin)
ifdef IS_COMPONENT
EXTRA_DSO_LDOPTS	+= -bundle
else
EXTRA_DSO_LDOPTS	+= -dynamiclib -install_name @executable_path/\$@ -compatibility_version 1 -current_version 1
endif
endif

#
# On NetBSD a.out systems, use -Bsymbolic.  This fixes what would otherwise be
# fatal symbol name clashes between components.
#
ifeq ($(OS_ARCH),NetBSD)
ifeq ($(DLL_SUFFIX),.so.1.0)
ifdef IS_COMPONENT
EXTRA_DSO_LDOPTS += -Wl,-Bsymbolic
endif
endif
endif

ifeq ($(OS_ARCH),NetBSD)
ifneq (,$(filter arc cobalt hpcmips mipsco newsmips pmax sgimips,$(OS_TEST)))
ifeq ($(MODULE),layout)
OS_CFLAGS += -Wa,-xgot
OS_CXXFLAGS += -Wa,-xgot
endif
endif
endif

ifeq ($(OS_ARCH),Linux)
ifneq (,$(filter mips mipsel,$(OS_TEST)))
ifeq ($(MODULE),layout)
OS_CFLAGS += -Wa,-xgot
OS_CXXFLAGS += -Wa,-xgot
endif
endif
endif

#
# HP-UXBeOS specific section: for COMPONENTS only, add -Bsymbolic flag
# which uses internal symbols first
#
ifeq ($(OS_ARCH),HP-UX)
ifdef IS_COMPONENT
ifeq ($(GNU_CC)$(GNU_CXX),)
EXTRA_DSO_LDOPTS += -Wl,-Bsymbolic
ifneq ($(HAS_EXTRAEXPORTS),1)
MKSHLIB  += -Wl,+eNSGetModule -Wl,+eerrno
MKCSHLIB += -Wl,+eNSGetModule -Wl,+eerrno
ifneq ($(OS_TEST),ia64)
MKSHLIB  += -Wl,+e_shlInit
MKCSHLIB += -Wl,+e_shlInit
endif # !ia64
endif # !HAS_EXTRAEXPORTS
endif # non-gnu compilers
endif # IS_COMPONENT
endif # HP-UX

ifeq ($(OS_ARCH),AIX)
ifdef IS_COMPONENT
ifneq ($(HAS_EXTRAEXPORTS),1)
MKSHLIB += -bE:$(MOZILLA_DIR)/build/unix/aix.exp -bnoexpall
MKCSHLIB += -bE:$(MOZILLA_DIR)/build/unix/aix.exp -bnoexpall
endif # HAS_EXTRAEXPORTS
endif # IS_COMPONENT
endif # AIX

#
# OSF1: add -B symbolic flag for components
#
ifeq ($(OS_ARCH),OSF1)
ifdef IS_COMPONENT
ifeq ($(GNU_CC)$(GNU_CXX),)
EXTRA_DSO_LDOPTS += -B symbolic
endif  
endif  
endif

#
# Linux: add -Bsymbolic flag for components
# 
ifeq ($(OS_ARCH),Linux)
ifdef IS_COMPONENT
EXTRA_DSO_LDOPTS += -Wl,-Bsymbolic
endif
endif 

ifeq ($(USE_TVFS),1)
IFLAGS1 = -rb
IFLAGS2 = -rb
else
IFLAGS1 = -m 644
IFLAGS2 = -m 755
endif

ifeq ($(MOZ_OS2_TOOLS),VACPP)
OUTOPTION = -Fo# eol
else
ifeq (_WINNT,$(GNU_CC)_$(OS_ARCH))
OUTOPTION = -Fo# eol
else
OUTOPTION = -o # eol
endif # WINNT && !GNU_CC
endif # VACPP

################################################################################

all:: 
	$(MAKE) export
	$(MAKE) libs

# Do depend as well
alldep:: 
	$(MAKE) export
	$(MAKE) depend
	$(MAKE) libs

# Do everything from scratch
everything::
	$(MAKE) clean
	$(MAKE) alldep

# Add dummy depend target for tinderboxes
depend::

ifdef ALL_PLATFORMS
all_platforms:: $(NFSPWD)
	@d=`$(NFSPWD)`;							\
	if test ! -d LOGS; then rm -rf LOGS; mkdir LOGS; else true; fi;	\
	for h in $(PLATFORM_HOSTS); do					\
		echo "On $$h: $(MAKE) $(ALL_PLATFORMS) >& LOGS/$$h.log";\
		rsh $$h -n "(chdir $$d;					\
			     $(MAKE) $(ALL_PLATFORMS) >& LOGS/$$h.log;	\
			     echo DONE) &" 2>&1 > LOGS/$$h.pid &	\
		sleep 1;						\
	done

$(NFSPWD):
	cd $(@D); $(MAKE) $(@F)
endif

# Target to only regenerate makefiles
makefiles: $(SUBMAKEFILES)
ifdef DIRS
	@for d in $(filter-out $(STATIC_MAKEFILES), $(DIRS)); do\
		$(UPDATE_TITLE) 				\
		$(MAKE) -C $$d $@				\
	done
endif

export:: $(SUBMAKEFILES) $(MAKE_DIRS)
	+$(LOOP_OVER_DIRS)

#
# Rule to create list of libraries for final link
#
export::
ifdef LIBRARY_NAME
ifdef EXPORT_LIBRARY
ifdef IS_COMPONENT
ifdef BUILD_STATIC_LIBS
	@$(PERL) -I$(MOZILLA_DIR)/config $(MOZILLA_DIR)/config/build-list.pl $(FINAL_LINK_COMPS) $(LIBRARY_NAME)
ifdef MODULE_NAME
	@$(PERL) -I$(MOZILLA_DIR)/config $(MOZILLA_DIR)/config/build-list.pl $(FINAL_LINK_COMP_NAMES) $(MODULE_NAME)
endif
endif
else
	$(PERL) -I$(MOZILLA_DIR)/config $(MOZILLA_DIR)/config/build-list.pl $(FINAL_LINK_LIBS) $(LIBRARY_NAME)
endif # IS_COMPONENT
endif # EXPORT_LIBRARY
endif # LIBRARY_NAME

##############################################
libs:: $(SUBMAKEFILES) $(MAKE_DIRS) $(HOST_LIBRARY) $(LIBRARY) $(SHARED_LIBRARY) $(IMPORT_LIBRARY) $(HOST_PROGRAM) $(PROGRAM) $(HOST_SIMPLE_PROGRAMS) $(SIMPLE_PROGRAMS) $(MAPS)
ifndef NO_DIST_INSTALL
ifneq (,$(BUILD_STATIC_LIBS)$(FORCE_STATIC_LIB))
ifdef LIBRARY
ifdef IS_COMPONENT
	$(INSTALL) $(IFLAGS1) $(LIBRARY) $(DIST)/lib/$(COMPONENTS_PATH)
else
	$(INSTALL) $(IFLAGS1) $(LIBRARY) $(DIST)/lib
endif
endif # LIBRARY
endif # BUILD_STATIC_LIBS || FORCE_STATIC_LIB
ifdef MAPS
	$(INSTALL) $(IFLAGS1) $(MAPS) $(DIST)/bin
endif
ifdef SHARED_LIBRARY
ifdef IS_COMPONENT
ifeq ($(OS_ARCH),OS2)
	$(INSTALL) $(IFLAGS2) $(IMPORT_LIBRARY) $(DIST)/lib/$(COMPONENTS_PATH)
else
	$(INSTALL) $(IFLAGS2) $(SHARED_LIBRARY) $(DIST)/lib/$(COMPONENTS_PATH)
	$(ELF_DYNSTR_GC) $(DIST)/lib/$(COMPONENTS_PATH)/$(SHARED_LIBRARY)
endif
	$(INSTALL) $(IFLAGS2) $(SHARED_LIBRARY) $(DIST)/bin/$(COMPONENTS_PATH)
	$(ELF_DYNSTR_GC) $(DIST)/bin/$(COMPONENTS_PATH)/$(SHARED_LIBRARY)
ifdef BEOS_ADDON_WORKAROUND
	( cd $(DIST)/bin/components && $(CC) -nostart -o $(SHARED_LIBRARY).stub $(SHARED_LIBRARY) )
endif
else # ! IS_COMPONENT
ifneq (,$(filter OS2 WINNT,$(OS_ARCH)))
	$(INSTALL) $(IFLAGS2) $(IMPORT_LIBRARY) $(DIST)/lib
else
	$(INSTALL) $(IFLAGS2) $(SHARED_LIBRARY) $(DIST)/lib
endif
	$(INSTALL) $(IFLAGS2) $(SHARED_LIBRARY) $(DIST)/bin
ifdef BEOS_ADDON_WORKAROUND
	( cd $(DIST)/bin && $(CC) -nostart -o $(SHARED_LIBRARY).stub $(SHARED_LIBRARY) )
endif
endif # IS_COMPONENT
endif # SHARED_LIBRARY
ifdef PROGRAM
	$(INSTALL) $(IFLAGS2) $(PROGRAM) $(DIST)/bin
endif
ifdef SIMPLE_PROGRAMS
	$(INSTALL) $(IFLAGS2) $(SIMPLE_PROGRAMS) $(DIST)/bin
endif
ifdef HOST_PROGRAM
	$(INSTALL) $(IFLAGS2) $(HOST_PROGRAM) $(DIST)/host/bin
endif
ifdef HOST_SIMPLE_PROGRAMS
	$(INSTALL) $(IFLAGS2) $(HOST_SIMPLE_PROGRAMS) $(DIST)/host/bin
endif
ifdef HOST_LIBRARY
	$(INSTALL) $(IFLAGS1) $(HOST_LIBRARY) $(DIST)/host/lib
endif
endif # !NO_DIST_INSTALL
	+$(LOOP_OVER_DIRS)

##############################################
install:: $(SUBMAKEFILES) $(MAKE_DIRS)
	+$(LOOP_OVER_DIRS)

install:: $(EXPORTS)
ifndef NO_INSTALL
ifdef EXPORTS
	$(SYSINSTALL) $(IFLAGS1) $^ $(DESTDIR)$(includedir)/$(MODULE)
endif
endif

install:: $(SDK_HEADERS)
ifndef NO_INSTALL
ifdef SDK_HEADERS
	$(SYSINSTALL) $(IFLAGS1) $^ $(DESTDIR)$(includedir)/$(MODULE)
endif
endif

install:: $(SHARED_LIBRARY) $(IMPORT_LIBRARY) $(LIBRARY) $(PROGRAM) $(SIMPLE_PROGRAMS)
ifndef NO_INSTALL
#ifdef LIBRARY
#ifndef IS_COMPONENT
#ifdef MRE_DIST
#	$(SYSINSTALL) $(IFLAGS1) $(LIBRARY) $(DESTDIR)$(mredir)
#else
#	$(SYSINSTALL) $(IFLAGS1) $(LIBRARY) $(DESTDIR)$(mozappdir)
#endif
#endif # !IS_COMPONENT
#endif # LIBRARY
ifdef SHARED_LIBRARY
ifdef IS_COMPONENT
ifdef MRE_DIST
	$(SYSINSTALL) $(IFLAGS2) $(SHARED_LIBRARY) $(IMPORT_LIBRARY) $(DESTDIR)$(mredir)/$(COMPONENTS_PATH)
else
	$(SYSINSTALL) $(IFLAGS2) $(SHARED_LIBRARY) $(IMPORT_LIBRARY) $(DESTDIR)$(mozappdir)/$(COMPONENTS_PATH)
endif
else
ifdef MRE_DIST
	$(SYSINSTALL) $(IFLAGS2) $(SHARED_LIBRARY) $(IMPORT_LIBRARY) $(DESTDIR)$(mredir)
else
	$(SYSINSTALL) $(IFLAGS2) $(SHARED_LIBRARY) $(IMPORT_LIBRARY) $(DESTDIR)$(mozappdir)
endif
endif
endif # SHARED_LIBRARY
ifdef PROGRAM
ifdef MRE_DIST
	$(SYSINSTALL) $(IFLAGS2) $(PROGRAM) $(DESTDIR)$(mredir)
else
	$(SYSINSTALL) $(IFLAGS2) $(PROGRAM) $(DESTDIR)$(mozappdir)
endif
endif # PROGRAM
ifdef SIMPLE_PROGRAMS
ifdef MRE_DIST
	$(SYSINSTALL) $(IFLAGS2) $(SIMPLE_PROGRAMS) $(DESTDIR)$(mredir)
else
	$(SYSINSTALL) $(IFLAGS2) $(SIMPLE_PROGRAMS) $(DESTDIR)$(mozappdir)
endif
endif # SIMPLE_PROGRAMS
endif # NO_INSTALL

checkout:
	$(MAKE) -C $(topsrcdir) -f client.mk checkout

run_viewer: $(DIST)/bin/viewer
	cd $(DIST)/bin; \
	MOZILLA_FIVE_HOME=`pwd` \
	LD_LIBRARY_PATH=".:$(LIBS_PATH):$$LD_LIBRARY_PATH" \
	viewer

clean clobber realclean clobber_all:: $(SUBMAKEFILES)
	-rm -f $(ALL_TRASH)
	-rm -rf $(ALL_TRASH_DIRS)
	+$(LOOP_OVER_DIRS)

distclean:: $(SUBMAKEFILES)
	+$(LOOP_OVER_DIRS)
	-rm -rf $(ALL_TRASH_DIRS) 
	-rm -f $(ALL_TRASH)  \
	Makefile .HSancillary \
	$(wildcard *.$(OBJ_SUFFIX)) $(wildcard *.ho) $(wildcard host_*.o) \
	$(wildcard *.$(LIB_SUFFIX)) $(wildcard *$(DLL_SUFFIX))
ifeq ($(MOZ_OS2_TOOLS),VACPP)
	-rm -f $(PROGRAM:.exe=.map)
endif

alltags:
	rm -f TAGS
	find $(topsrcdir) -name dist -prune -o \( -name '*.[hc]' -o -name '*.cp' -o -name '*.cpp' -o -name '*.idl' \) -print | $(TAG_PROGRAM)

#
# PROGRAM = Foo
# creates OBJS, links with LIBS to create Foo
#
$(PROGRAM): $(PROGOBJS) $(EXTRA_DEPS) $(EXE_DEF_FILE) $(RESFILE) Makefile Makefile.in
ifeq ($(MOZ_OS2_TOOLS),VACPP)
	$(LD) -OUT:$@ $(LDFLAGS) $(PROGOBJS) $(LIBS) $(EXTRA_LIBS) $(OS_LIBS) $(EXE_DEF_FILE) /ST:0x100000
else
ifeq (_WINNT,$(GNU_CC)_$(OS_ARCH))
	$(LD) /NOLOGO /OUT:$@ /PDB:$(PDBFILE) $(PROGOBJS) $(RESFILE) $(WIN32_EXE_LDFLAGS) $(LDFLAGS) $(LIBS) $(EXTRA_LIBS) $(OS_LIBS)
else
ifeq ($(CPP_PROG_LINK),1)
	$(CCC) -o $@ $(CXXFLAGS) $(WRAP_MALLOC_CFLAGS) $(PROGOBJS) $(LDFLAGS) $(LIBS_DIR) $(LIBS) $(OS_LIBS) $(EXTRA_LIBS) $(BIN_FLAGS) $(WRAP_MALLOC_LIB) $(PROFILER_LIBS)
else # ! CPP_PROG_LINK
	$(CC) -o $@ $(CFLAGS) $(PROGOBJS) $(LDFLAGS) $(LIBS_DIR) $(LIBS) $(OS_LIBS) $(EXTRA_LIBS) $(BIN_FLAGS)
endif # CPP_PROG_LINK
endif # WINNT && !GNU_CC
endif # OS2
ifdef ENABLE_STRIP
	$(STRIP) $@
endif
ifdef MOZ_POST_PROGRAM_COMMAND
	$(MOZ_POST_PROGRAM_COMMAND) $@
endif
ifeq ($(OS_ARCH),BeOS)
ifdef BEOS_PROGRAM_RESOURCE
	xres -o $@ $(BEOS_PROGRAM_RESOURCE)
	mimeset $@
endif
endif # BeOS
ifeq ($(OS_ARCH),OS2)
ifdef RESFILE
	$(RC) $(RCFLAGS) $(RESFILE) $@
endif
endif

$(HOST_PROGRAM): $(HOST_PROGOBJS) $(HOST_EXTRA_DEPS) Makefile Makefile.in
ifeq (_WINNT,$(GNU_CC)_$(OS_ARCH))
	$(HOST_LD) /NOLOGO /OUT:$@ /PDB:$(PDBFILE) $(HOST_OBJS) $(WIN32_EXE_LDFLAGS) $(HOST_LIBS) $(EXTRA_LIBS)
else
	$(HOST_CC) -o $@ $(HOST_CFLAGS) $(HOST_LDFLAGS) $(HOST_PROGOBJS) $(HOST_LIBS) $(HOST_EXTRA_LIBS)
endif

#
# This is an attempt to support generation of multiple binaries
# in one directory, it assumes everything to compile Foo is in
# Foo.o (from either Foo.c or Foo.cpp).
#
# SIMPLE_PROGRAMS = Foo Bar
# creates Foo.o Bar.o, links with LIBS to create Foo, Bar.
#
$(SIMPLE_PROGRAMS): %$(BIN_SUFFIX): %.$(OBJ_SUFFIX) $(EXTRA_DEPS) Makefile Makefile.in
ifeq ($(MOZ_OS2_TOOLS),VACPP)
	$(LD) /Out:$@ $< $(LDFLAGS) $(LIBS) $(OS_LIBS) $(EXTRA_LIBS) $(WRAP_MALLOC_LIB) $(PROFILER_LIBS)
else
ifeq (_WINNT,$(GNU_CC)_$(OS_ARCH))
	$(LD) /nologo /out:$@ /pdb:$(PDBFILE) $< $(WIN32_EXE_LDFLAGS) $(LDFLAGS) $(LIBS) $(EXTRA_LIBS) $(OS_LIBS)
else
ifeq ($(CPP_PROG_LINK),1)
	$(CCC) $(WRAP_MALLOC_CFLAGS) $(CXXFLAGS) -o $@ $< $(LDFLAGS) $(LIBS_DIR) $(LIBS) $(OS_LIBS) $(EXTRA_LIBS) $(WRAP_MALLOC_LIB) $(PROFILER_LIBS) $(BIN_FLAGS)
else
	$(CC) $(WRAP_MALLOC_CFLAGS) $(CFLAGS) $(OUTOPTION)$@ $< $(LDFLAGS) $(LIBS_DIR) $(LIBS) $(OS_LIBS) $(EXTRA_LIBS) $(WRAP_MALLOC_LIB) $(PROFILER_LIBS) $(BIN_FLAGS)
endif # CPP_PROG_LINK
endif # WINNT && !GNU_CC
endif # OS/2 VACPP
ifdef ENABLE_STRIP
	$(STRIP) $@
endif
ifdef MOZ_POST_PROGRAM_COMMAND
	$(MOZ_POST_PROGRAM_COMMAND) $@
endif

$(HOST_SIMPLE_PROGRAMS): host_%$(BIN_SUFFIX): host_%.o $(HOST_EXTRA_DEPS) Makefile Makefile.in
	$(HOST_CC) $(OUTOPTION)$@ $(HOST_CFLAGS) $(INCLUDES) $< $(HOST_LIBS) $(HOST_EXTRA_LIBS)

#
# Purify target.  Solaris/sparc only to start.
# Purify does not recognize "egcs" or "c++" so we go with 
# "gcc" and "g++" for now.
#
pure:	$(PROGRAM)
ifeq ($(CPP_PROG_LINK),1)
	$(PURIFY) $(CCC) -o $^.pure $(CXXFLAGS) $(PROGOBJS) $(LDFLAGS) $(LIBS_DIR) $(LIBS) $(OS_LIBS) $(EXTRA_LIBS)
else
	$(PURIFY) $(CC) -o $^.pure $(CFLAGS) $(PROGOBJS) $(LDFLAGS) $(LIBS_DIR) $(LIBS) $(OS_LIBS) $(EXTRA_LIBS)
endif
ifndef NO_DIST_INSTALL
	$(INSTALL) $(IFLAGS2) $^.pure $(DIST)/bin
endif

quantify: $(PROGRAM)
ifeq ($(CPP_PROG_LINK),1)
	$(QUANTIFY) $(CCC) -o $^.quantify $(CXXFLAGS) $(PROGOBJS) $(LDFLAGS) $(LIBS_DIR) $(LIBS) $(OS_LIBS) $(EXTRA_LIBS)
else
	$(QUANTIFY) $(CC) -o $^.quantify $(CFLAGS) $(PROGOBJS) $(LDFLAGS) $(LIBS_DIR) $(LIBS) $(OS_LIBS) $(EXTRA_LIBS)
endif
ifndef NO_DIST_INSTALL
	$(INSTALL) $(IFLAGS2) $^.quantify $(DIST)/bin
endif

ifneq ($(OS_ARCH),OS2)
#
# This allows us to create static versions of the shared libraries
# that are built using other static libraries.  Confused...?
#
ifdef SHARED_LIBRARY_LIBS
ifeq ($(OS_ARCH),WINNT)
ifneq (,$(BUILD_STATIC_LIBS)$(FORCE_STATIC_LIB))
LOBJS	+= $(SHARED_LIBRARY_LIBS)
endif
else
ifneq (,$(filter OSF1 BSD_OS FreeBSD NetBSD OpenBSD SunOS Darwin,$(OS_ARCH)))
CLEANUP1	:= | egrep -v '(________64ELEL_|__.SYMDEF)'
CLEANUP2	:= rm -f ________64ELEL_ __.SYMDEF
else
CLEANUP2	:= true
endif
SUB_LOBJS	= $(shell for lib in $(SHARED_LIBRARY_LIBS); do $(AR_LIST) $${lib} $(CLEANUP1); done;)
endif
endif

$(LIBRARY): $(OBJS) $(LOBJS) $(SHARED_LIBRARY_LIBS) Makefile Makefile.in
	rm -f $@
ifneq ($(OS_ARCH),WINNT)
ifdef SHARED_LIBRARY_LIBS
	@rm -f $(SUB_LOBJS)
	@for lib in $(SHARED_LIBRARY_LIBS); do $(AR_EXTRACT) $${lib}; $(CLEANUP2); done
endif
endif
	$(AR) $(AR_FLAGS) $(OBJS) $(LOBJS) $(SUB_LOBJS)
	$(RANLIB) $@
	@rm -f foodummyfilefoo $(SUB_LOBJS)

ifeq ($(OS_ARCH),WINNT)
$(IMPORT_LIBRARY): $(SHARED_LIBRARY)
endif

else # OS2
ifdef SHARED_LIBRARY_LIBS
SUB_LOBJS	= $(SHARED_LIBRARY_LIBS)
endif

$(DEF_FILE): $(DEF_OBJS)
	rm -f $@
	@cmd /C "echo LIBRARY $(LIBRARY_NAME) INITINSTANCE TERMINSTANCE >$(DEF_FILE)"
	@cmd /C "echo PROTMODE >>$(DEF_FILE)"
	@cmd /C "echo CODE    LOADONCALL MOVEABLE DISCARDABLE >>$(DEF_FILE)"
	@cmd /C "echo DATA    PRELOAD MOVEABLE MULTIPLE NONSHARED >>$(DEF_FILE)"        
	@cmd /C "echo EXPORTS >>$(DEF_FILE)"
ifeq ($(IS_COMPONENT),1)
ifeq ($(HAS_EXTRAEXPORTS),1)
	$(FILTER) $(DEF_OBJS) >> $(DEF_FILE)
else
	@cmd /C "echo    NSGetModule>>$(DEF_FILE)"
endif
else
	$(FILTER) $(DEF_OBJS) >> $(DEF_FILE)
endif
	$(ADD_TO_DEF_FILE)

$(IMPORT_LIBRARY): $(OBJS) $(DEF_FILE)
	rm -f $@
	$(MAKE_DEF_FILE)
	$(IMPLIB) $@ $(DEF_FILE)
	$(RANLIB) $@

$(LIBRARY): $(OBJS) $(LOBJS) $(SHARED_LIBRARY_LIBS) Makefile Makefile.in
	rm -f $@
	$(AR) $(AR_FLAGS) $(OBJS) $(LOBJS) $(SUB_LOBJS)
	$(RANLIB) $@
endif # OS/2

$(HOST_LIBRARY): $(HOST_OBJS) Makefile
	rm -f $@
	$(HOST_AR) $(HOST_AR_FLAGS) $(HOST_OBJS)
	$(HOST_RANLIB) $@

ifdef NO_LD_ARCHIVE_FLAGS
SUB_SHLOBJS = $(SUB_LOBJS)
endif

$(SHARED_LIBRARY): $(OBJS) $(LOBJS) $(DEF_FILE) $(RESFILE) $(SHARED_LIBRARY_LIBS) $(EXTRA_DEPS) Makefile Makefile.in
	rm -f $@
ifneq ($(OS_ARCH),OS2)
ifeq ($(OS_ARCH),OpenVMS)
	@if test ! -f $(VMS_SYMVEC_FILE); then \
	  if test -f $(VMS_SYMVEC_FILE_MODULE); then \
	    echo Creating specific component options file $(VMS_SYMVEC_FILE); \
	    cp $(VMS_SYMVEC_FILE_MODULE) $(VMS_SYMVEC_FILE); \
	  fi; \
	fi
ifdef IS_COMPONENT
	@if test ! -f $(VMS_SYMVEC_FILE); then \
	  echo Creating generic component options file $(VMS_SYMVEC_FILE); \
	  cp $(VMS_SYMVEC_FILE_COMP) $(VMS_SYMVEC_FILE); \
	fi
endif
endif
ifdef NO_LD_ARCHIVE_FLAGS
ifdef SHARED_LIBRARY_LIBS
	@rm -f $(SUB_SHLOBJS)
	@for lib in $(SHARED_LIBRARY_LIBS); do $(AR_EXTRACT) $${lib}; $(CLEANUP2); done
endif # SHARED_LIBRARY_LIBS
endif # NO_LD_ARCHIVE_FLAGS
	$(MKSHLIB) $(SHLIB_LDSTARTFILE) $(OBJS) $(LOBJS) $(SUB_SHLOBJS) $(RESFILE) $(LDFLAGS) $(EXTRA_DSO_LDOPTS) $(OS_LIBS) $(EXTRA_LIBS) $(DEF_FILE) $(SHLIB_LDENDFILE)
	@rm -f foodummyfilefoo $(SUB_SHLOBJS)
else # OS2
ifeq ($(MOZ_OS2_TOOLS),VACPP)
	$(MKSHLIB) /O:$@ /DLL /INC:_dllentry $(LDFLAGS) $(OBJS) $(LOBJS) $(EXTRA_DSO_LDOPTS) $(OS_LIBS) $(EXTRA_LIBS) $(DEF_FILE)
else
	$(MKSHLIB) -o $@ $(OBJS) $(LOBJS) $(EXTRA_DSO_LDOPTS) $(OS_LIBS) $(EXTRA_LIBS) $(DEF_FILE)
endif
ifdef RESFILE
	$(RC) $(RCFLAGS) $(RESFILE) $@
endif
endif # OS2
	chmod +x $@
ifdef ENABLE_STRIP
	$(STRIP) $@
endif
ifdef MOZ_POST_DSO_LIB_COMMAND
	$(MOZ_POST_DSO_LIB_COMMAND) $@
endif

ifeq ($(OS_ARCH),OS2)
$(DLL): $(OBJS) $(EXTRA_LIBS)
	rm -f $@
	$(MKSHLIB) -o $@ $(OBJS) $(EXTRA_LIBS) $(OS_LIBS)
endif

ifdef MOZ_AUTO_DEPS
ifndef COMPILER_DEPEND
#
# Generate dependencies on the fly
#
_MDDEPFILE = $(MDDEPDIR)/$(@F).pp

ifneq ($(OS_ARCH),OS2)
ifeq ($(OS_ARCH),WINNT)
define MAKE_DEPS_AUTO
if test -d $(@D); then \
	echo "Building deps for $(srcdir)/$(<F)"; \
	touch $(_MDDEPFILE) && \
	$(MKDEPEND) -o'.$(OBJ_SUFFIX)' -f$(_MDDEPFILE) $(DEFINES) $(ACDEFINES) $(INCLUDES) $(srcdir)/$(<F) >/dev/null 2>&1 && \
	mv $(_MDDEPFILE) $(_MDDEPFILE).old && \
	cat $(_MDDEPFILE).old | sed -e "s|^$(srcdir)/||g" > $(_MDDEPFILE) && rm -f $(_MDDEPFILE).old ; \
fi
endef
else
define MAKE_DEPS_AUTO
if test -d $(@D); then \
	echo "Building deps for $<"; \
	touch $(_MDDEPFILE) && \
	$(MKDEPEND) -o'.$(OBJ_SUFFIX)' -f$(_MDDEPFILE) $(DEFINES) $(ACDEFINES) $(INCLUDES) $< >/dev/null 2>&1 && \
	mv $(_MDDEPFILE) $(_MDDEPFILE).old && \
	cat $(_MDDEPFILE).old | sed -e "s|^$(<D)/||g" > $(_MDDEPFILE) && rm -f $(_MDDEPFILE).old ; \
fi
endef
endif # WINNT
endif # OS2

endif # !COMPILER_DEPEND

endif # MOZ_AUTO_DEPS

# Rules for building native targets must come first because of the host_ prefix
host_%.o: %.c Makefile.in
	$(REPORT_BUILD)
ifdef _NO_AUTO_VARS
	$(ELOG) $(HOST_CC) $(OUTOPTION)$@ -c $(HOST_CFLAGS) $(INCLUDES) $(NSPR_CFLAGS) $(srcdir)/$*.c
else
	$(ELOG) $(HOST_CC) $(OUTOPTION)$@ -c $(HOST_CFLAGS) $(INCLUDES) $(NSPR_CFLAGS) $<
endif

%: %.c Makefile.in
	$(REPORT_BUILD)
	@$(MAKE_DEPS_AUTO)
ifdef _NO_AUTO_VARS
	$(ELOG) $(CC) $(CFLAGS) $(LDFLAGS) $(OUTOPTION)$@ $(srcdir)/$*.c
else
	$(ELOG) $(CC) $(CFLAGS) $(LDFLAGS) $(OUTOPTION)$@ $<
endif

%.$(OBJ_SUFFIX): %.c Makefile.in
	$(REPORT_BUILD)
	@$(MAKE_DEPS_AUTO)
ifdef _NO_AUTO_VARS
	$(ELOG) $(CC) $(OUTOPTION)$@ -c $(COMPILE_CFLAGS) $(srcdir)/$*.c
else
	$(ELOG) $(CC) $(OUTOPTION)$@ -c $(COMPILE_CFLAGS) $<
endif

moc_%.cpp: %.h Makefile.in
	$(MOC) $< $(OUTOPTION)$@ 

# The AS_DASH_C_FLAG is needed cause not all assemblers (Solaris) accept
# a '-c' flag.
%.$(OBJ_SUFFIX): %.$(ASM_SUFFIX) Makefile.in
ifeq ($(MOZ_OS2_TOOLS),VACPP)
	$(AS) -Fdo:./$(OBJDIR) -Feo:.$(OBJ_SUFFIX) $(ASFLAGS) $(AS_DASH_C_FLAG) $<
else
	$(AS) -o $@ $(ASFLAGS) $(AS_DASH_C_FLAG) $<
endif

%.$(OBJ_SUFFIX): %.S Makefile.in
	$(AS) -o $@ $(ASFLAGS) -c $<

%: %.cpp Makefile.in
	@$(MAKE_DEPS_AUTO)
ifdef _NO_AUTO_VARS
	$(CCC) $(OUTOPTION)$@ $(CXXFLAGS) $(srcdir)/$*.cpp $(LDFLAGS)
else
	$(CCC) $(OUTOPTION)$@ $(CXXFLAGS) $< $(LDFLAGS)
endif

#
# Please keep the next two rules in sync.
#
%.$(OBJ_SUFFIX): %.cc Makefile.in
	$(REPORT_BUILD)
	@$(MAKE_DEPS_AUTO)
ifdef _NO_AUTO_VARS
	$(ELOG) $(CCC) $(OUTOPTION)$@ -c $(COMPILE_CXXFLAGS) $(srcdir)/$*.cc
else
	$(ELOG) $(CCC) $(OUTOPTION)$@ -c $(COMPILE_CXXFLAGS) $<
endif

%.$(OBJ_SUFFIX): %.cpp Makefile.in
	$(REPORT_BUILD)
	@$(MAKE_DEPS_AUTO)
ifdef STRICT_CPLUSPLUS_SUFFIX
	echo "#line 1 \"$*.cpp\"" | cat - $*.cpp > t_$*.cc
	$(ELOG) $(CCC) -o $@ -c $(COMPILE_CXXFLAGS) t_$*.cc
	rm -f t_$*.cc
else
ifdef _NO_AUTO_VARS
	$(ELOG) $(CCC) $(OUTOPTION)$@ -c $(COMPILE_CXXFLAGS) $(srcdir)/$*.cpp
else
	$(ELOG) $(CCC) $(OUTOPTION)$@ -c $(COMPILE_CXXFLAGS) $<
endif
endif #STRICT_CPLUSPLUS_SUFFIX

$(OBJ_PREFIX)%.$(OBJ_SUFFIX): %.mm Makefile.in
	$(REPORT_BUILD)
	@$(MAKE_DEPS_AUTO)
	$(ELOG) $(CCC) -o $@ -c $(COMPILE_CXXFLAGS) $<

%.i: %.cpp
	$(CCC) -C -E $(COMPILE_CXXFLAGS) $< > $*.i

%.i: %.c
	$(CC) -C -E $(COMPILE_CFLAGS) $< > $*.i

%.res: %.rc
	@echo Creating Resource file: $@
ifeq ($(OS_ARCH),OS2)
	$(RC) $(RCFLAGS) -i $(subst /,\,$(srcdir)) -r $< $@
else
	$(RC) $(RCFLAGS) -r $(DEFINES) $(INCLUDES) $(OUTOPTION)$@ $<
endif


# need 3 separate lines for OS/2
%: %.pl
	rm -f $@
	cp $< $@
	chmod +x $@

%: %.sh
	rm -f $@; cp $< $@; chmod +x $@

# Cancel these implicit rules
#
%: %,v

%: RCS/%,v

%: s.%

%: SCCS/s.%

###############################################################################
# Update Makefiles
###############################################################################
# Note: Passing depth to make-makefile is optional.
#       It saves the script some work, though.
Makefile: Makefile.in
	@$(PERL) $(AUTOCONF_TOOLS)/make-makefile -t $(topsrcdir) -d $(DEPTH) 

ifdef SUBMAKEFILES
# VPATH does not work on some machines in this case, so add $(srcdir)
$(SUBMAKEFILES): % : $(srcdir)/%.in
	$(PERL) $(AUTOCONF_TOOLS)/make-makefile -t $(topsrcdir) -d $(DEPTH) $@
endif

ifdef AUTOUPDATE_CONFIGURE
$(topsrcdir)/configure: $(topsrcdir)/configure.in
	(cd $(topsrcdir) && $(AUTOCONF)) && (cd $(DEPTH) && ./config.status --recheck)
endif

###############################################################################
# Bunch of things that extend the 'export' rule (in order):
###############################################################################

################################################################################
# Copy each element of EXPORTS to $(PUBLIC)

ifneq ($(EXPORTS)$(XPIDLSRCS)$(SDK_HEADERS)$(SDK_XPIDLSRCS),)
$(SDK_PUBLIC) $(PUBLIC)::
	@if test ! -d $@; then echo Creating $@; rm -rf $@; $(NSINSTALL) -D $@; else true; fi
endif

ifneq ($(EXPORTS),)
export:: $(EXPORTS) $(PUBLIC)
ifndef NO_DIST_INSTALL
	$(INSTALL) $(IFLAGS1) $^
	$(PERL) -I$(MOZILLA_DIR)/config $(MOZILLA_DIR)/config/build-list.pl $(PUBLIC)/.headerlist $(notdir $(filter-out $(PUBLIC),$^))
endif
endif 

ifneq ($(SDK_HEADERS),)
export:: $(PUBLIC) $(SDK_PUBLIC)

export:: $(SDK_HEADERS) 
ifndef NO_DIST_INSTALL
	$(INSTALL) $(IFLAGS1) $^ $(PUBLIC)
	$(PERL) -I$(MOZILLA_DIR)/config $(MOZILLA_DIR)/config/build-list.pl $(PUBLIC)/.headerlist $(notdir $(filter-out $(PUBLIC),$^))
	$(INSTALL) $(IFLAGS1) $^ $(SDK_PUBLIC)
	$(PERL) -I$(MOZILLA_DIR)/config $(MOZILLA_DIR)/config/build-list.pl $(PUBLIC)/.headerlist $(notdir $(filter-out $(SDK_PUBLIC),$^))
endif
endif 

################################################################################
# Copy each element of PREF_JS_EXPORTS to $(DIST)/bin/defaults/pref

ifneq ($(PREF_JS_EXPORTS),)
$(DIST)/bin/defaults/pref::
	@if test ! -d $@; then echo Creating $@; rm -rf $@; $(NSINSTALL) -D $@; else true; fi

export:: $(PREF_JS_EXPORTS) $(DIST)/bin/defaults/pref
ifndef NO_DIST_INSTALL
	$(INSTALL) $(IFLAGS1) $^
endif

install:: $(PREF_JS_EXPORTS)
ifndef NO_INSTALL
	$(SYSINSTALL) $(IFLAGS1) $^ $(DESTDIR)$(mozappdir)/defaults/pref
endif
endif 
################################################################################
# Copy each element of AUTOCFG_JS_EXPORTS to $(DIST)/bin/defaults/autoconfig

ifneq ($(AUTOCFG_JS_EXPORTS),)
$(DIST)/bin/defaults/autoconfig::
	@if test ! -d $@; then echo Creating $@; rm -rf $@; $(NSINSTALL) -D $@; else true; fi

export:: $(AUTOCFG_JS_EXPORTS) $(DIST)/bin/defaults/autoconfig
ifndef NO_DIST_INSTALL
	$(INSTALL) $(IFLAGS1) $^
endif

install:: $(AUTOCFG_JS_EXPORTS)
ifndef NO_INSTALL
	$(SYSINSTALL) $(IFLAGS1) $^ $(DESTDIR)$(mozappdir)/defaults/autoconfig
endif
endif 
################################################################################
# Export the elements of $(XPIDLSRCS) & $(SDK_XPIDLSRCS), 
# generating .h and .xpt files and moving them to the appropriate places.

ifneq ($(XPIDLSRCS)$(SDK_XPIDLSRCS),)

ifndef XPIDL_MODULE
XPIDL_MODULE		= $(MODULE)
endif

ifeq ($(XPIDL_MODULE),) # we need $(XPIDL_MODULE) to make $(XPIDL_MODULE).xpt
export:: FORCE
	@echo
	@echo "*** Error processing XPIDLSRCS:"
	@echo "Please define MODULE or XPIDL_MODULE when defining XPIDLSRCS,"
	@echo "so we have a module name to use when creating MODULE.xpt."
	@echo; sleep 2; false
endif

$(SDK_IDL_DIR) $(IDL_DIR)::
	@if test ! -d $@; then echo Creating $@; rm -rf $@; $(NSINSTALL) -D $@; else true; fi

# generate .h files from into $(XPIDL_GEN_DIR), then export to $(PUBLIC);
# warn against overriding existing .h file. 
$(XPIDL_GEN_DIR)/.done:
	@if test ! -d $(XPIDL_GEN_DIR); then echo Creating $(XPIDL_GEN_DIR)/.done; rm -rf $(XPIDL_GEN_DIR); mkdir $(XPIDL_GEN_DIR); fi
	@touch $@

# don't depend on $(XPIDL_GEN_DIR), because the modification date changes
# with any addition to the directory, regenerating all .h files -> everything.

$(XPIDL_GEN_DIR)/%.h: %.idl $(XPIDL_COMPILE) $(XPIDL_GEN_DIR)/.done
	$(REPORT_BUILD)
ifdef _NO_AUTO_VARS
	$(ELOG) $(XPIDL_COMPILE) -m header -w -I $(IDL_DIR) -I$(srcdir) -o $(XPIDL_GEN_DIR)/$* $(srcdir)/$*.idl
else
	$(ELOG) $(XPIDL_COMPILE) -m header -w -I $(IDL_DIR) -I$(srcdir) -o $(XPIDL_GEN_DIR)/$* $<
endif
	@if test -n "$(findstring $*.h, $(EXPORTS) $(SDK_HEADERS))"; \
	  then echo "*** WARNING: file $*.h generated from $*.idl overrides $(srcdir)/$*.h"; else true; fi

ifndef NO_GEN_XPT
# generate intermediate .xpt files into $(XPIDL_GEN_DIR), then link
# into $(XPIDL_MODULE).xpt and export it to $(DIST)/bin/components.
$(XPIDL_GEN_DIR)/%.xpt: %.idl $(XPIDL_COMPILE)
	$(REPORT_BUILD)
ifdef _NO_AUTO_VARS
	$(ELOG) $(XPIDL_COMPILE) -m typelib -w -I $(IDL_DIR) -I$(srcdir) -o $(XPIDL_GEN_DIR)/$* $(srcdir)/$*.idl
else
	$(ELOG) $(XPIDL_COMPILE) -m typelib -w -I $(IDL_DIR) -I$(srcdir) -o $(XPIDL_GEN_DIR)/$* $<
endif

$(XPIDL_GEN_DIR)/$(XPIDL_MODULE).xpt: $(patsubst %.idl,$(XPIDL_GEN_DIR)/%.xpt,$(XPIDLSRCS) $(SDK_XPIDLSRCS)) Makefile.in Makefile
	$(XPIDL_LINK) $(XPIDL_GEN_DIR)/$(XPIDL_MODULE).xpt $(patsubst %.idl,$(XPIDL_GEN_DIR)/%.xpt,$(XPIDLSRCS) $(SDK_XPIDLSRCS)) 

libs:: $(XPIDL_GEN_DIR)/$(XPIDL_MODULE).xpt
ifndef NO_DIST_INSTALL
	$(INSTALL) $(IFLAGS1) $(XPIDL_GEN_DIR)/$(XPIDL_MODULE).xpt $(DIST)/bin/$(COMPONENTS_PATH)
endif

install:: $(XPIDL_GEN_DIR)/$(XPIDL_MODULE).xpt
ifndef NO_INSTALL
ifdef MRE_DIST
	$(SYSINSTALL) $(IFLAGS1) $(XPIDL_GEN_DIR)/$(XPIDL_MODULE).xpt $(DESTDIR)$(mredir)/$(COMPONENTS_PATH)
else 
	$(SYSINSTALL) $(IFLAGS1) $(XPIDL_GEN_DIR)/$(XPIDL_MODULE).xpt $(DESTDIR)$(mozappdir)/$(COMPONENTS_PATH)
endif 
endif # NO_INSTALL
endif # NO_GEN_XPT

GARBAGE_DIRS		+= $(XPIDL_GEN_DIR)

endif # XPIDLSRCS || SDK_XPIDLSRCS

ifneq ($(XPIDLSRCS),)
# export .idl files to $(IDL_DIR)
export:: $(XPIDLSRCS) $(IDL_DIR)
ifndef NO_DIST_INSTALL
	$(INSTALL) $(IFLAGS1) $^
endif

export:: $(patsubst %.idl,$(XPIDL_GEN_DIR)/%.h, $(XPIDLSRCS)) $(PUBLIC)
ifndef NO_DIST_INSTALL
	$(INSTALL) $(IFLAGS1) $^
	$(PERL) -I$(MOZILLA_DIR)/config $(MOZILLA_DIR)/config/build-list.pl $(PUBLIC)/.headerlist $(notdir $(filter-out $(PUBLIC),$^))
endif

install:: $(XPIDLSRCS)
ifndef NO_INSTALL
	$(SYSINSTALL) $(IFLAGS1) $^ $(DESTDIR)$(idldir)
endif

install:: $(patsubst %.idl,$(XPIDL_GEN_DIR)/%.h, $(XPIDLSRCS))
ifndef NO_INSTALL
	$(SYSINSTALL) $(IFLAGS1) $^ $(DESTDIR)$(includedir)/$(MODULE)
endif

endif # XPIDLSRCS



#
# General rules for exporting idl files.
#
# WORK-AROUND ONLY, for mozilla/tools/module-deps/bootstrap.pl build.
# Bug to fix idl dependecy problems w/o this extra build pass is
#   http://bugzilla.mozilla.org/show_bug.cgi?id=145777
#
$(IDL_DIR)::
	@if test ! -d $@; then echo Creating $@; rm -rf $@; $(NSINSTALL) -D $@; else true; fi

export-idl:: $(SUBMAKEFILES) $(MAKE_DIRS)

export-idl:: $(XPIDLSRCS) $(SDK_XPIDLSRCS) $(IDL_DIR)
ifneq ($(XPIDLSRCS),)
ifndef NO_DIST_INSTALL
	$(INSTALL) $(IFLAGS1) $^
endif
endif
	+$(LOOP_OVER_DIRS)




ifneq ($(SDK_XPIDLSRCS),)
# export .idl files to $(IDL_DIR) & $(SDK_IDL_DIR)
export:: $(IDL_DIR) $(SDK_IDL_DIR)

export:: $(SDK_XPIDLSRCS)
ifndef NO_DIST_INSTALL
	$(INSTALL) $(IFLAGS1) $^ $(IDL_DIR)
	$(INSTALL) $(IFLAGS1) $^ $(SDK_IDL_DIR)
endif

export:: $(patsubst %.idl,$(XPIDL_GEN_DIR)/%.h, $(SDK_XPIDLSRCS))
ifndef NO_DIST_INSTALL
	$(INSTALL) $(IFLAGS1) $^ $(PUBLIC)
	$(PERL) -I$(MOZILLA_DIR)/config $(MOZILLA_DIR)/config/build-list.pl $(PUBLIC)/.headerlist $(notdir $(filter-out $(PUBLIC),$^))
	$(INSTALL) $(IFLAGS1) $^ $(SDK_PUBLIC)
	$(PERL) -I$(MOZILLA_DIR)/config $(MOZILLA_DIR)/config/build-list.pl $(PUBLIC)/.headerlist $(notdir $(filter-out $(SDK_PUBLIC),$^))
endif

install:: $(SDK_XPIDLSRCS)
ifndef NO_INSTALL
	$(SYSINSTALL) $(IFLAGS1) $^ $(DESTDIR)$(idldir)
endif

install:: $(patsubst %.idl,$(XPIDL_GEN_DIR)/%.h, $(SDK_XPIDLSRCS))
ifndef NO_INSTALL
	$(SYSINSTALL) $(IFLAGS1) $^ $(DESTDIR)$(includedir)/$(MODULE)
endif

endif # SDK_XPIDLSRCS

################################################################################
# Copy each element of EXTRA_COMPONENTS to $(DIST)/bin/components
ifdef EXTRA_COMPONENTS
libs:: $(EXTRA_COMPONENTS)
ifndef NO_DIST_INSTALL
	$(INSTALL) $(IFLAGS2) $^ $(DIST)/bin/components
endif

install:: $(EXTRA_COMPONENTS)
ifndef NO_INSTALL
	$(SYSINSTALL) $(IFLAGS2) $^ $(DESTDIR)$(mozappdir)/components
endif
endif

################################################################################
# SDK

ifneq (,$(SDK_BINARY))
$(SDK_BIN_DIR)::
	@if test ! -d $@; then echo Creating $@; rm -rf $@; $(NSINSTALL) -D $@; else true; fi

libs:: $(SDK_BINARY) $(SDK_BIN_DIR)
ifndef NO_DIST_INSTALL
	$(INSTALL) $(IFLAGS2) $^
endif

endif # SDK_BINARY

################################################################################
# CHROME PACKAGING

JAR_MANIFEST := $(srcdir)/jar.mn

ifeq (flat,$(MOZ_CHROME_FILE_FORMAT))
_JAR_REGCHROME_DISABLE_JAR=1
else
_JAR_REGCHROME_DISABLE_JAR=0
endif

ifdef NO_JAR_AUTO_REG
_JAR_AUTO_REG=-a
endif

ifeq ($(OS_TARGET),WIN95)
_NO_FLOCK=-l
endif

libs chrome:: $(CHROME_DEPS)
ifndef NO_DIST_INSTALL
ifdef MOZ_PHOENIX
	@if test -f $(JAR_MANIFEST); then $(PERL) -I$(MOZILLA_DIR)/config $(MOZILLA_DIR)/config/make-jars.pl $(_NO_FLOCK) $(_JAR_AUTO_REG) -f $(MOZ_CHROME_FILE_FORMAT) -d $(DIST)/bin/chrome -s $(srcdir) -p $(MOZILLA_DIR)/config/preprocessor.pl -- "$(DEFINES) $(ACDEFINES)" < $(JAR_MANIFEST); fi
else
	@if test -f $(JAR_MANIFEST); then $(PERL) -I$(MOZILLA_DIR)/config $(MOZILLA_DIR)/config/make-jars.pl $(_NO_FLOCK) $(_JAR_AUTO_REG) -f $(MOZ_CHROME_FILE_FORMAT) -d $(DIST)/bin/chrome -s $(srcdir) < $(JAR_MANIFEST); fi
endif
	@if test -f $(JAR_MANIFEST); then $(PERL) -I$(MOZILLA_DIR)/config $(MOZILLA_DIR)/config/make-chromelist.pl $(DIST)/bin/chrome $(JAR_MANIFEST) $(_NO_FLOCK); fi
endif

install:: $(CHROME_DEPS)
ifndef NO_INSTALL
ifdef MOZ_PHOENIX
	@if test -f $(JAR_MANIFEST); then $(PERL) -I$(MOZILLA_DIR)/config $(MOZILLA_DIR)/config/make-jars.pl $(_NO_FLOCK) $(_JAR_AUTO_REG) -f $(MOZ_CHROME_FILE_FORMAT) -d $(DESTDIR)$(mozappdir)/chrome -s $(srcdir) -p $(MOZILLA_DIR)/config/preprocessor.pl -- "$(DEFINES) $(ACDEFINES)" < $(JAR_MANIFEST); fi
else
	@if test -f $(JAR_MANIFEST); then $(PERL) -I$(MOZILLA_DIR)/config $(MOZILLA_DIR)/config/make-jars.pl $(_NO_FLOCK) $(_JAR_AUTO_REG) -f $(MOZ_CHROME_FILE_FORMAT) -d $(DESTDIR)$(mozappdir)/chrome -s $(srcdir) < $(JAR_MANIFEST); fi
endif
	@if test -f $(JAR_MANIFEST); then $(PERL) -I$(MOZILLA_DIR)/config $(MOZILLA_DIR)/config/make-chromelist.pl $(DESTDIR)$(mozappdir)/chrome $(JAR_MANIFEST) $(_NO_FLOCK); fi
endif

REGCHROME = $(PERL) -I$(MOZILLA_DIR)/config $(MOZILLA_DIR)/config/add-chrome.pl $(DIST)/bin/chrome/installed-chrome.txt $(_JAR_REGCHROME_DISABLE_JAR)
REGCHROME_INSTALL = $(PERL) -I$(MOZILLA_DIR)/config $(MOZILLA_DIR)/config/add-chrome.pl $(DESTDIR)$(mozappdir)/chrome/installed-chrome.txt $(_JAR_REGCHROME_DISABLE_JAR)

#############################################################################
# Dependency system
#############################################################################
ifdef COMPILER_DEPEND
depend::
	@echo "$(MAKE): No need to run depend target.\
			Using compiler-based depend." 1>&2
ifeq ($(GNU_CC)$(GNU_CXX),)
# Non-GNU compilers
	@echo "`echo '$(MAKE):'|sed 's/./ /g'`"\
	'(Compiler-based depend was turned on by "--enable-md".)' 1>&2
else
# GNU compilers
	@space="`echo '$(MAKE): '|sed 's/./ /g'`";\
	echo "$$space"'Since you are using a GNU compiler,\
		it is on by default.' 1>&2; \
	echo "$$space"'To turn it off, pass --disable-md to configure.' 1>&2
endif

else # ! COMPILER_DEPEND

ifndef MOZ_AUTO_DEPS

ifneq ($(OS_ARCH),OS2)
ifeq ($(OS_ARCH),WINNT)
define MAKE_DEPS_NOAUTO
	set -e ; \
	touch $@ && \
	$(MKDEPEND) -w1024 -o'.$(OBJ_SUFFIX)' -f$@ $(DEFINES) $(ACDEFINES) $(INCLUDES) $(srcdir)/$(<F) >/dev/null 2>&1 && \
	mv $@ $@.old && cat $@.old | sed "s|^$(srcdir)/||g" > $@ && rm -f $@.old
endef
else
define MAKE_DEPS_NOAUTO
	set -e ; \
	touch $@ && \
	$(MKDEPEND) -w1024 -o'.$(OBJ_SUFFIX)' -f$@ $(DEFINES) $(ACDEFINES) $(INCLUDES) $< >/dev/null 2>&1 && \
	mv $@ $@.old && cat $@.old | sed "s|^$(<D)/||g" > $@ && rm -f $@.old
endef
endif # WINNT
endif # OS2

$(MDDEPDIR)/%.pp: %.c
	$(REPORT_BUILD)
	@$(MAKE_DEPS_NOAUTO)

$(MDDEPDIR)/%.pp: %.cpp
	$(REPORT_BUILD)
	@$(MAKE_DEPS_NOAUTO)

$(MDDEPDIR)/%.pp: %.s
	$(REPORT_BUILD)
	@$(MAKE_DEPS_NOAUTO)

ifneq (,$(OBJS)$(SIMPLE_PROGRAMS))
depend:: $(SUBMAKEFILES) $(MAKE_DIRS) $(MDDEPFILES)
else
depend:: $(SUBMAKEFILES)
endif
	+$(LOOP_OVER_DIRS)

dependclean:: $(SUBMAKEFILES)
	rm -f $(MDDEPFILES)
	+$(LOOP_OVER_DIRS)

endif # MOZ_AUTO_DEPS

endif # COMPILER_DEPEND


#############################################################################
# MDDEPDIR is the subdirectory where all the dependency files are placed.
#   This uses a make rule (instead of a macro) to support parallel
#   builds (-jN). If this were done in the LOOP_OVER_DIRS macro, two
#   processes could simultaneously try to create the same directory.
#
$(MDDEPDIR):
	@if test ! -d $@; then echo Creating $@; rm -rf $@; mkdir $@; else true; fi

ifneq (,$(OBJS)$(SIMPLE_PROGRAMS))
MDDEPEND_FILES		:= $(strip $(wildcard $(MDDEPDIR)/*.pp))

ifneq (,$(MDDEPEND_FILES))
ifdef PERL
# The script mddepend.pl checks the dependencies and writes to stdout
# one rule to force out-of-date objects. For example,
#   foo.o boo.o: FORCE
# The script has an advantage over including the *.pp files directly
# because it handles the case when header files are removed from the build.
# 'make' would complain that there is no way to build missing headers.
$(MDDEPDIR)/.all.pp: FORCE
	@cat $(MDDEPEND_FILES) | $(PERL) $(BUILD_TOOLS)/mddepend.pl $@
-include $(MDDEPDIR)/.all.pp
else
include $(MDDEPEND_FILES)
endif
endif

endif
#############################################################################

-include $(MY_RULES)

#
# This speeds up gmake's processing if these files don't exist.
#
$(MY_CONFIG) $(MY_RULES):
	@touch $@

#
# Generate Emacs tags in a file named TAGS if ETAGS was set in $(MY_CONFIG)
# or in $(MY_RULES)
#
ifdef ETAGS
ifneq ($(CSRCS)$(CPPSRCS)$(HEADERS),)
all:: TAGS
TAGS:: $(CSRCS) $(CPPSRCS) $(HEADERS)
	$(ETAGS) $(CSRCS) $(CPPSRCS) $(HEADERS)
endif
endif

################################################################################
# Special gmake rules.
################################################################################

#
# Re-define the list of default suffixes, so gmake won't have to churn through
# hundreds of built-in suffix rules for stuff we don't need.
#
.SUFFIXES:
.SUFFIXES: .out .a .ln .o .c .cc .C .cpp .y .l .s .S .h .sh .i .pl .class .java .html .pp .mk .in .$(OBJ_SUFFIX) .mm .idl $(BIN_SUFFIX)

#
# Fake targets.  Always run these rules, even if a file/directory with that
# name already exists.
#
.PHONY: all all_platforms alltags boot checkout clean clobber clobber_all export install libs makefiles realclean run_viewer run_apprunner $(DIRS) FORCE

# Used as a dependency to force targets to rebuild
FORCE:

# Delete target if error occurs when building target
.DELETE_ON_ERROR:

# pdbfiles are written to by every file when debugging in enabled,
# so the files must be built serially
# This requires a recent version of gmake 
ifeq ($(OS_ARCH),WINNT)
ifneq (,$(MOZ_DEBUG)$(MOZ_PROFILE)$(MOZ_COVERAGE))
.NOTPARALLEL::
endif
endif

tags: TAGS

TAGS: $(SUBMAKEFILES) $(CSRCS) $(CPPSRCS) $(wildcard *.h)
	-etags $(CSRCS) $(CPPSRCS) $(wildcard *.h)
	+$(LOOP_OVER_DIRS)

echo-dirs:
	@echo $(DIRS)

echo-module:
	@echo $(MODULE)

echo-requires:
	@echo $(REQUIRES)

echo-depth-path:
	@$(topsrcdir)/build/unix/print-depth-path.sh

echo-module-name:
	@$(topsrcdir)/build/package/rpm/print-module-name.sh

echo-module-filelist:
	@$(topsrcdir)/build/package/rpm/print-module-filelist.sh

showtargs:
ifneq (,$(filter $(PROGRAM) $(HOST_PROGRAM) $(SIMPLE_PROGRAMS) $(HOST_LIBRARY) $(LIBRARY) $(SHARED_LIBRARY),$(TARGETS)))
	@echo --------------------------------------------------------------------------------
	@echo "PROGRAM             = $(PROGRAM)"
	@echo "SIMPLE_PROGRAMS     = $(SIMPLE_PROGRAMS)"
	@echo "LIBRARY             = $(LIBRARY)"
	@echo "SHARED_LIBRARY      = $(SHARED_LIBRARY)"
	@echo "SHARED_LIBRARY_LIBS = $(SHARED_LIBRARY_LIBS)"
	@echo "LIBS                = $(LIBS)"
	@echo "DEF_FILE            = $(DEF_FILE)"
	@echo "DEF_OBJS            = $(DEF_OBJS)"
	@echo "IMPORT_LIBRARY      = $(IMPORT_LIBRARY)"
	@echo "STATIC_LIBS         = $(STATIC_LIBS)"
	@echo "SHARED_LIBS         = $(SHARED_LIBS)"
	@echo "EXTRA_DSO_LIBS      = $(EXTRA_DSO_LIBS)"
	@echo "EXTRA_DSO_LDOPTS    = $(EXTRA_DSO_LDOPTS)"
	@echo --------------------------------------------------------------------------------
endif
	+$(LOOP_OVER_MOZ_DIRS)

showbuild:
	@echo "MOZ_BUILD_ROOT     = $(MOZ_BUILD_ROOT)"
	@echo "MOZ_WIDGET_TOOLKIT = $(MOZ_WIDGET_TOOLKIT)"
	@echo "CC                 = $(CC)"
	@echo "CXX                = $(CXX)"
	@echo "CPP                = $(CPP)"
	@echo "LD                 = $(LD)"
	@echo "AR                 = $(AR)"
	@echo "IMPLIB             = $(IMPLIB)"
	@echo "FILTER             = $(FILTER)"
	@echo "MKSHLIB            = $(MKSHLIB)"
	@echo "MKCSHLIB           = $(MKCSHLIB)"
	@echo "RC                 = $(RC)"
	@echo "CFLAGS             = $(CFLAGS)"
	@echo "OS_CFLAGS          = $(OS_CFLAGS)"
	@echo "COMPILE_CFLAGS     = $(COMPILE_CFLAGS)"
	@echo "CXXFLAGS           = $(CXXFLAGS)"
	@echo "OS_CXXFLAGS        = $(OS_CXXFLAGS)"
	@echo "COMPILE_CXXFLAGS   = $(COMPILE_CXXFLAGS)"
	@echo "LDFLAGS            = $(LDFLAGS)"
	@echo "OS_LDFLAGS         = $(OS_LDFLAGS)"
	@echo "DSO_LDOPTS         = $(DSO_LDOPTS)"
	@echo "OS_INCLUDES        = $(OS_INCLUDES)"
	@echo "OS_LIBS            = $(OS_LIBS)"
	@echo "EXTRA_LIBS         = $(EXTRA_LIBS)"
	@echo "BIN_FLAGS          = $(BIN_FLAGS)"
	@echo "INCLUDES           = $(INCLUDES)"
	@echo "DEFINES            = $(DEFINES)"
	@echo "ACDEFINES          = $(ACDEFINES)"
	@echo "BIN_SUFFIX         = $(BIN_SUFFIX)"
	@echo "LIB_SUFFIX         = $(LIB_SUFFIX)"
	@echo "DLL_SUFFIX         = $(DLL_SUFFIX)"
	@echo "INSTALL            = $(INSTALL)"

showhost:
	@echo "HOST_CC            = $(HOST_CC)"
	@echo "HOST_CFLAGS        = $(HOST_CFLAGS)"
	@echo "HOST_LDFLAGS       = $(HOST_LDFLAGS)"
	@echo "HOST_LIBS          = $(HOST_LIBS)"
	@echo "HOST_EXTRA_LIBS    = $(HOST_EXTRA_LIBS)"
	@echo "HOST_EXTRA_DEPS    = $(HOST_EXTRA_DEPS)"
	@echo "HOST_PROGRAM       = $(HOST_PROGRAM)"
	@echo "HOST_PROGOBJS      = $(HOST_PROGOBJS)"

showbuildmods::
	@echo "Build Modules	= $(BUILD_MODULES)"
	@echo "Module dirs	= $(BUILD_MODULE_DIRS)"

zipmakes:
ifneq (,$(filter $(PROGRAM) $(SIMPLE_PROGRAMS) $(LIBRARY) $(SHARED_LIBRARY),$(TARGETS)))
	zip $(DEPTH)/makefiles $(subst $(topsrcdir),$(MOZ_SRC)/mozilla,$(srcdir)/Makefile.in)
endif
	+$(LOOP_OVER_MOZ_DIRS)

documentation:
	@cd $(DEPTH)
	$(DOXYGEN) $(DEPTH)/config/doxygen.cfg
