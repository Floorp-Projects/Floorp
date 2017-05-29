# -*- makefile -*-
# vim:set ts=8 sw=8 sts=8 noet:
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
#

ifndef topsrcdir
$(error topsrcdir was not set))
endif

# Define an include-at-most-once flag
ifdef INCLUDED_RULES_MK
$(error Do not include rules.mk twice!)
endif
INCLUDED_RULES_MK = 1

ifndef INCLUDED_CONFIG_MK
include $(topsrcdir)/config/config.mk
endif

ifndef INCLUDED_VERSION_MK
include $(MOZILLA_DIR)/config/version.mk
endif

USE_AUTOTARGETS_MK = 1
include $(MOZILLA_DIR)/config/makefiles/makeutils.mk

ifdef REBUILD_CHECK
REPORT_BUILD = $(info $(shell $(PYTHON) $(MOZILLA_DIR)/config/rebuild_check.py $@ $^))
REPORT_BUILD_VERBOSE = $(REPORT_BUILD)
else
REPORT_BUILD = $(info $(notdir $@))

ifdef BUILD_VERBOSE_LOG
REPORT_BUILD_VERBOSE = $(REPORT_BUILD)
else
REPORT_BUILD_VERBOSE =
endif

endif

EXEC			= exec

# ELOG prints out failed command when building silently (gmake -s). Pymake
# prints out failed commands anyway, so ELOG just makes things worse by
# forcing shell invocations.
ifneq (,$(findstring -s, $(filter-out --%, $(MAKEFLAGS))))
  ELOG := $(EXEC) sh $(BUILD_TOOLS)/print-failed-commands.sh
else
  ELOG :=
endif # -s

_VPATH_SRCS = $(abspath $<)

################################################################################
# Testing frameworks support
################################################################################

testxpcobjdir = $(DEPTH)/_tests/xpcshell

ifdef ENABLE_TESTS
ifdef CPP_UNIT_TESTS
ifdef COMPILE_ENVIRONMENT

# Compile the tests to $(DIST)/bin.  Make lots of niceties available by default
# through TestHarness.h, by modifying the list of includes and the libs against
# which stuff links.
SIMPLE_PROGRAMS += $(CPP_UNIT_TESTS)
INCLUDES += -I$(ABS_DIST)/include/testing

ifndef MOZ_PROFILE_GENERATE
CPP_UNIT_TESTS_FILES = $(CPP_UNIT_TESTS)
CPP_UNIT_TESTS_DEST = $(DIST)/cppunittests
CPP_UNIT_TESTS_TARGET = target
INSTALL_TARGETS += CPP_UNIT_TESTS
endif

run-cppunittests::
	@$(PYTHON) $(MOZILLA_DIR)/testing/runcppunittests.py --xre-path=$(DIST)/bin --symbols-path=$(DIST)/crashreporter-symbols $(CPP_UNIT_TESTS)

cppunittests-remote:
	$(PYTHON) -u $(MOZILLA_DIR)/testing/remotecppunittests.py \
		--xre-path=$(DEPTH)/dist/bin \
		--localLib=$(DEPTH)/dist/$(MOZ_APP_NAME) \
		--deviceIP=${TEST_DEVICE} \
		$(CPP_UNIT_TESTS) $(EXTRA_TEST_ARGS); \

endif # COMPILE_ENVIRONMENT
endif # CPP_UNIT_TESTS
endif # ENABLE_TESTS


#
# Library rules
#
# If FORCE_STATIC_LIB is set, build a static library.
# Otherwise, build a shared library.
#

ifndef LIBRARY
ifdef REAL_LIBRARY
# Don't build actual static library if a shared library is also built
ifdef FORCE_SHARED_LIB
# ... except when we really want one
ifdef NO_EXPAND_LIBS
LIBRARY			:= $(REAL_LIBRARY)
else
LIBRARY			:= $(REAL_LIBRARY).$(LIBS_DESC_SUFFIX)
endif
else
# Only build actual library if it is installed in DIST/lib
ifeq (,$(DIST_INSTALL)$(NO_EXPAND_LIBS))
LIBRARY			:= $(REAL_LIBRARY).$(LIBS_DESC_SUFFIX)
else
ifdef NO_EXPAND_LIBS
LIBRARY			:= $(REAL_LIBRARY)
else
LIBRARY			:= $(REAL_LIBRARY) $(REAL_LIBRARY).$(LIBS_DESC_SUFFIX)
endif
endif
endif
endif # REAL_LIBRARY
endif # LIBRARY

ifndef HOST_LIBRARY
ifdef HOST_LIBRARY_NAME
HOST_LIBRARY		:= $(LIB_PREFIX)$(HOST_LIBRARY_NAME).$(LIB_SUFFIX)
endif
endif

ifdef LIBRARY
ifdef FORCE_SHARED_LIB
ifdef MKSHLIB

ifdef LIB_IS_C_ONLY
MKSHLIB			= $(MKCSHLIB)
endif

EMBED_MANIFEST_AT=2

endif # MKSHLIB
endif # FORCE_SHARED_LIB
endif # LIBRARY

ifeq ($(OS_ARCH),WINNT)
ifndef GNU_CC

#
# Unless we're building SIMPLE_PROGRAMS, all C++ files share a PDB file per
# directory. For parallel builds, this PDB file is shared and locked by
# MSPDBSRV.EXE, starting with MSVC8 SP1. If you're using MSVC 7.1 or MSVC8
# without SP1, don't do parallel builds.
#
# The final PDB for libraries and programs is created by the linker and uses
# a different name from the single PDB file created by the compiler. See
# bug 462740.
#

ifdef SIMPLE_PROGRAMS
COMPILE_PDB_FLAG ?= -Fd$(basename $(@F)).pdb
else
COMPILE_PDB_FLAG ?= -Fdgenerated.pdb -FS
endif
COMPILE_CFLAGS += $(COMPILE_PDB_FLAG)
COMPILE_CXXFLAGS += $(COMPILE_PDB_FLAG)

LINK_PDBFILE ?= $(basename $(@F)).pdb
ifdef MOZ_DEBUG
CODFILE=$(basename $(@F)).cod
endif

ifdef DEFFILE
OS_LDFLAGS += -DEF:$(call normalizepath,$(DEFFILE))
EXTRA_DEPS += $(DEFFILE)
endif

else #!GNU_CC

ifdef DEFFILE
OS_LDFLAGS += $(call normalizepath,$(DEFFILE))
EXTRA_DEPS += $(DEFFILE)
endif

endif # !GNU_CC

endif # WINNT

ifeq (arm-Darwin,$(CPU_ARCH)-$(OS_TARGET))
ifdef PROGRAM
MOZ_PROGRAM_LDFLAGS += -Wl,-rpath -Wl,@executable_path/Frameworks
endif
endif

ifeq ($(HOST_OS_ARCH),WINNT)
HOST_PDBFILE=$(basename $(@F)).pdb
HOST_PDB_FLAG ?= -Fd$(HOST_PDBFILE)
HOST_CFLAGS += $(HOST_PDB_FLAG)
HOST_CXXFLAGS += $(HOST_PDB_FLAG)
endif

# Don't build SIMPLE_PROGRAMS during the MOZ_PROFILE_GENERATE pass, and do not
# attempt to install them
ifdef MOZ_PROFILE_GENERATE
$(foreach category,$(INSTALL_TARGETS),\
  $(eval $(category)_FILES := $(foreach file,$($(category)_FILES),$(if $(filter $(SIMPLE_PROGRAMS),$(notdir $(file))),,$(file)))))
SIMPLE_PROGRAMS :=
endif

ifdef COMPILE_ENVIRONMENT
ifndef TARGETS
TARGETS			= $(LIBRARY) $(SHARED_LIBRARY) $(PROGRAM) $(SIMPLE_PROGRAMS) $(HOST_LIBRARY) $(HOST_PROGRAM) $(HOST_SIMPLE_PROGRAMS)
endif

COBJS = $(notdir $(CSRCS:.c=.$(OBJ_SUFFIX)))
SOBJS = $(notdir $(SSRCS:.S=.$(OBJ_SUFFIX)))
# CPPSRCS can have different extensions (eg: .cpp, .cc)
CPPOBJS = $(notdir $(addsuffix .$(OBJ_SUFFIX),$(basename $(CPPSRCS))))
CMOBJS = $(notdir $(CMSRCS:.m=.$(OBJ_SUFFIX)))
CMMOBJS = $(notdir $(CMMSRCS:.mm=.$(OBJ_SUFFIX)))
# ASFILES can have different extensions (.s, .asm)
ASOBJS = $(notdir $(addsuffix .$(OBJ_SUFFIX),$(basename $(ASFILES))))
RS_STATICLIB_CRATE_OBJ = $(addprefix lib,$(notdir $(RS_STATICLIB_CRATE_SRC:.rs=.$(LIB_SUFFIX))))
ifndef OBJS
_OBJS = $(COBJS) $(SOBJS) $(CPPOBJS) $(CMOBJS) $(CMMOBJS) $(ASOBJS)
OBJS = $(strip $(_OBJS))
endif

HOST_COBJS = $(addprefix host_,$(notdir $(HOST_CSRCS:.c=.$(OBJ_SUFFIX))))
# HOST_CPPOBJS can have different extensions (eg: .cpp, .cc)
HOST_CPPOBJS = $(addprefix host_,$(notdir $(addsuffix .$(OBJ_SUFFIX),$(basename $(HOST_CPPSRCS)))))
HOST_CMOBJS = $(addprefix host_,$(notdir $(HOST_CMSRCS:.m=.$(OBJ_SUFFIX))))
HOST_CMMOBJS = $(addprefix host_,$(notdir $(HOST_CMMSRCS:.mm=.$(OBJ_SUFFIX))))
ifndef HOST_OBJS
_HOST_OBJS = $(HOST_COBJS) $(HOST_CPPOBJS) $(HOST_CMOBJS) $(HOST_CMMOBJS)
HOST_OBJS = $(strip $(_HOST_OBJS))
endif
else
LIBRARY :=
SHARED_LIBRARY :=
IMPORT_LIBRARY :=
REAL_LIBRARY :=
PROGRAM :=
SIMPLE_PROGRAMS :=
HOST_LIBRARY :=
HOST_PROGRAM :=
HOST_SIMPLE_PROGRAMS :=
endif

ALL_TRASH = \
	$(GARBAGE) $(TARGETS) $(OBJS) $(PROGOBJS) LOGS TAGS a.out \
	$(filter-out $(ASFILES),$(OBJS:.$(OBJ_SUFFIX)=.s)) $(OBJS:.$(OBJ_SUFFIX)=.ii) \
	$(OBJS:.$(OBJ_SUFFIX)=.i) $(OBJS:.$(OBJ_SUFFIX)=.i_o) \
	$(HOST_PROGOBJS) $(HOST_OBJS) $(IMPORT_LIBRARY) \
	$(EXE_DEF_FILE) so_locations _gen _stubs $(wildcard *.res) $(wildcard *.RES) \
	$(wildcard *.pdb) $(CODFILE) $(IMPORT_LIBRARY) \
	$(SHARED_LIBRARY:$(DLL_SUFFIX)=.exp) $(wildcard *.ilk) \
	$(PROGRAM:$(BIN_SUFFIX)=.exp) $(SIMPLE_PROGRAMS:$(BIN_SUFFIX)=.exp) \
	$(PROGRAM:$(BIN_SUFFIX)=.lib) $(SIMPLE_PROGRAMS:$(BIN_SUFFIX)=.lib) \
	$(SIMPLE_PROGRAMS:$(BIN_SUFFIX)=.$(OBJ_SUFFIX)) \
	$(wildcard gts_tmp_*) $(LIBRARY:%.a=.%.timestamp)
ALL_TRASH_DIRS = \
	$(GARBAGE_DIRS) /no-such-file

ifdef QTDIR
GARBAGE                 += $(MOCSRCS)
endif

ifdef SIMPLE_PROGRAMS
GARBAGE			+= $(SIMPLE_PROGRAMS:%=%.$(OBJ_SUFFIX))
endif

ifdef HOST_SIMPLE_PROGRAMS
GARBAGE			+= $(HOST_SIMPLE_PROGRAMS:%=%.$(OBJ_SUFFIX))
endif

ifdef MACH
ifndef NO_BUILDSTATUS_MESSAGES
define BUILDSTATUS
@echo 'BUILDSTATUS $1'

endef
endif
endif

define SUBMAKE # $(call SUBMAKE,target,directory,static)
+@$(MAKE) $(if $(2),-C $(2)) $(1)

endef # The extra line is important here! don't delete it

define TIER_DIR_SUBMAKE
$(call SUBMAKE,$(4),$(3),$(5))

endef # Ths empty line is important.

ifneq (,$(strip $(DIRS)))
LOOP_OVER_DIRS = \
  $(foreach dir,$(DIRS),$(call SUBMAKE,$@,$(dir)))
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

GARBAGE_DIRS    += $(wildcard $(CURDIR)/$(MDDEPDIR))

#
# Tags: emacs (etags), vi (ctags)
# TAG_PROGRAM := ctags -L -
#
TAG_PROGRAM		= xargs etags -a

#
# Turn on C++ linking if we have any .cpp or .mm files
# (moved this from config.mk so that config.mk can be included
#  before the CPPSRCS are defined)
#
ifneq ($(HOST_CPPSRCS)$(HOST_CMMSRCS),)
HOST_CPP_PROG_LINK	= 1
endif

#
# This will strip out symbols that the component should not be
# exporting from the .dynsym section.
#
ifdef IS_COMPONENT
EXTRA_DSO_LDOPTS += $(MOZ_COMPONENTS_VERSION_SCRIPT_LDFLAGS)
endif # IS_COMPONENT

#
# MacOS X specific stuff
#

ifeq ($(OS_ARCH),Darwin)
ifdef SHARED_LIBRARY
ifdef IS_COMPONENT
EXTRA_DSO_LDOPTS	+= -bundle
else
ifdef MOZ_IOS
_LOADER_PATH := @rpath
else
_LOADER_PATH := @executable_path
endif
EXTRA_DSO_LDOPTS	+= -dynamiclib -install_name $(_LOADER_PATH)/$(SHARED_LIBRARY) -compatibility_version 1 -current_version 1 -single_module
endif
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

ifeq ($(OS_ARCH),FreeBSD)
ifdef IS_COMPONENT
EXTRA_DSO_LDOPTS += -Wl,-Bsymbolic
endif
endif

ifeq ($(OS_ARCH),NetBSD)
ifneq (,$(filter arc cobalt hpcmips mipsco newsmips pmax sgimips,$(OS_TEST)))
ifneq (,$(filter layout/%,$(relativesrcdir)))
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
MKCSHLIB += +eNSGetModule +eerrno
ifneq ($(OS_TEST),ia64)
MKSHLIB  += -Wl,+e_shlInit
MKCSHLIB += +e_shlInit
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
# Linux: add -Bsymbolic flag for components
#
ifeq ($(OS_ARCH),Linux)
ifdef IS_COMPONENT
EXTRA_DSO_LDOPTS += -Wl,-Bsymbolic
endif
ifdef LD_VERSION_SCRIPT
EXTRA_DSO_LDOPTS += -Wl,--version-script,$(LD_VERSION_SCRIPT)
EXTRA_DEPS += $(LD_VERSION_SCRIPT)
endif
endif

ifdef SYMBOLS_FILE
ifeq ($(OS_TARGET),WINNT)
ifndef GNU_CC
EXTRA_DSO_LDOPTS += -DEF:$(call normalizepath,$(SYMBOLS_FILE))
else
EXTRA_DSO_LDOPTS += $(call normalizepath,$(SYMBOLS_FILE))
endif
else
ifdef GCC_USE_GNU_LD
EXTRA_DSO_LDOPTS += -Wl,--version-script,$(SYMBOLS_FILE)
else
ifeq ($(OS_TARGET),Darwin)
EXTRA_DSO_LDOPTS += -Wl,-exported_symbols_list,$(SYMBOLS_FILE)
endif
endif
endif
EXTRA_DEPS += $(SYMBOLS_FILE)
endif
#
# GNU doesn't have path length limitation
#

ifeq ($(OS_ARCH),GNU)
OS_CPPFLAGS += -DPATH_MAX=1024 -DMAXPATHLEN=1024
endif

#
# MINGW32
#
ifeq ($(OS_ARCH),WINNT)
ifdef GNU_CC
ifndef IS_COMPONENT
DSO_LDOPTS += -Wl,--out-implib -Wl,$(IMPORT_LIBRARY)
endif
endif
endif

ifeq ($(USE_TVFS),1)
IFLAGS1 = -rb
IFLAGS2 = -rb
else
IFLAGS1 = -m 644
IFLAGS2 = -m 755
endif

ifeq (_WINNT,$(GNU_CC)_$(OS_ARCH))
OUTOPTION = -Fo# eol
else
OUTOPTION = -o # eol
endif # WINNT && !GNU_CC

ifneq (,$(filter ml%,$(AS)))
ASOUTOPTION = -Fo# eol
else
ASOUTOPTION = -o # eol
endif

ifeq (,$(CROSS_COMPILE))
HOST_OUTOPTION = $(OUTOPTION)
else
HOST_OUTOPTION = -o # eol
endif
################################################################################

# Ensure the build config is up to date. This is done automatically when builds
# are performed through |mach build|. The check here is to catch people not
# using mach. If we ever enforce builds through mach, this code can be removed.
ifndef MOZBUILD_BACKEND_CHECKED
ifndef MACH
ifndef TOPLEVEL_BUILD
BUILD_BACKEND_FILES := $(addprefix $(DEPTH)/backend.,$(addsuffix Backend,$(BUILD_BACKENDS)))
$(DEPTH)/backend.%Backend:
	$(error Build configuration changed. Build with |mach build| or run |mach build-backend| to regenerate build config)

define build_backend_rule
$(1): $$(shell cat $(1).in)

endef
$(foreach file,$(BUILD_BACKEND_FILES),$(eval $(call build_backend_rule,$(file))))

default:: $(BUILD_BACKEND_FILES)

export MOZBUILD_BACKEND_CHECKED=1
endif
endif
endif

# The root makefile doesn't want to do a plain export/libs, because
# of the tiers and because of libxul. Suppress the default rules in favor
# of something else. Makefiles which use this var *must* provide a sensible
# default rule before including rules.mk
default all::
	$(foreach tier,$(TIERS),$(call SUBMAKE,$(tier)))

ifeq ($(findstring s,$(filter-out --%, $(MAKEFLAGS))),)
ECHO := echo
QUIET :=
else
ECHO := true
QUIET := -q
endif

# Do everything from scratch
everything::
	$(MAKE) clean
	$(MAKE) all

STATIC_LIB_DEP = $(if $(wildcard $(1).$(LIBS_DESC_SUFFIX)),$(1).$(LIBS_DESC_SUFFIX),$(1))
STATIC_LIBS_DEPS := $(foreach l,$(STATIC_LIBS),$(call STATIC_LIB_DEP,$(l)))

# Dependencies which, if modified, should cause everything to rebuild
GLOBAL_DEPS += Makefile $(addprefix $(DEPTH)/config/,$(INCLUDED_AUTOCONF_MK)) $(MOZILLA_DIR)/config/config.mk

##############################################
ifdef COMPILE_ENVIRONMENT
OBJ_TARGETS = $(OBJS) $(PROGOBJS) $(HOST_OBJS) $(HOST_PROGOBJS)

compile:: host target

host:: $(HOST_LIBRARY) $(HOST_PROGRAM) $(HOST_SIMPLE_PROGRAMS) $(HOST_RUST_PROGRAMS) $(HOST_RUST_LIBRARY_FILE)

target:: $(LIBRARY) $(SHARED_LIBRARY) $(PROGRAM) $(SIMPLE_PROGRAMS) $(RUST_LIBRARY_FILE) $(RUST_PROGRAMS)

syms::

include $(MOZILLA_DIR)/config/makefiles/target_binaries.mk
endif

##############################################
ifneq (1,$(NO_PROFILE_GUIDED_OPTIMIZE))
ifdef MOZ_PROFILE_USE
ifeq ($(OS_ARCH)_$(GNU_CC), WINNT_)
# When building with PGO, we have to make sure to re-link
# in the MOZ_PROFILE_USE phase if we linked in the
# MOZ_PROFILE_GENERATE phase. We'll touch this pgo.relink
# file in the link rule in the GENERATE phase to indicate
# that we need a relink.
ifdef SHARED_LIBRARY
$(SHARED_LIBRARY): pgo.relink
endif
ifdef PROGRAM
$(PROGRAM): pgo.relink
endif

# In the second pass, we need to merge the pgc files into the pgd file.
# The compiler would do this for us automatically if they were in the right
# place, but they're in dist/bin.
ifneq (,$(SHARED_LIBRARY)$(PROGRAM))
export::
ifdef PROGRAM
	$(PYTHON) $(MOZILLA_DIR)/build/win32/pgomerge.py \
	  $(PROGRAM:$(BIN_SUFFIX)=) $(DIST)/bin
endif
ifdef SHARED_LIBRARY
	$(PYTHON) $(MOZILLA_DIR)/build/win32/pgomerge.py \
	  $(patsubst $(DLL_PREFIX)%$(DLL_SUFFIX),%,$(SHARED_LIBRARY)) $(DIST)/bin
endif
endif # SHARED_LIBRARY || PROGRAM
endif # WINNT_
endif # MOZ_PROFILE_USE
ifdef MOZ_PROFILE_GENERATE
# Clean up profiling data during PROFILE_GENERATE phase
export::
ifeq ($(OS_ARCH)_$(GNU_CC), WINNT_)
	$(foreach pgd,$(wildcard *.pgd),pgomgr -clear $(pgd);)
else
ifdef GNU_CC
	-$(RM) *.gcda
endif
endif
endif

ifneq (,$(MOZ_PROFILE_GENERATE)$(MOZ_PROFILE_USE))
ifneq (,$(filter target,$(MAKECMDGOALS)))
ifdef GNU_CC
# Force rebuilding libraries and programs in both passes because each
# pass uses different object files.
$(PROGRAM) $(SHARED_LIBRARY) $(LIBRARY): FORCE
endif
endif
endif

endif # NO_PROFILE_GUIDED_OPTIMIZE

##############################################

checkout:
	$(MAKE) -C $(topsrcdir) -f client.mk checkout

clean clobber realclean clobber_all::
	-$(RM) $(ALL_TRASH)
	-$(RM) -r $(ALL_TRASH_DIRS)

clean clobber realclean clobber_all distclean::
	$(foreach dir,$(DIRS),-$(call SUBMAKE,$@,$(dir)))

distclean::
	-$(RM) -r $(ALL_TRASH_DIRS)
	-$(RM) $(ALL_TRASH)  \
	Makefile .HSancillary \
	$(wildcard *.$(OBJ_SUFFIX)) $(wildcard *.ho) $(wildcard host_*.o*) \
	$(wildcard *.$(LIB_SUFFIX)) $(wildcard *$(DLL_SUFFIX)) \
	$(wildcard *.$(IMPORT_LIB_SUFFIX))

alltags:
	$(RM) TAGS
	find $(topsrcdir) -name dist -prune -o \( -name '*.[hc]' -o -name '*.cp' -o -name '*.cpp' -o -name '*.idl' \) -print | $(TAG_PROGRAM)

#
# PROGRAM = Foo
# creates OBJS, links with LIBS to create Foo
#
$(PROGRAM): $(PROGOBJS) $(STATIC_LIBS_DEPS) $(EXTRA_DEPS) $(EXE_DEF_FILE) $(RESFILE) $(GLOBAL_DEPS)
	$(REPORT_BUILD)
	@$(RM) $@.manifest
ifeq (_WINNT,$(GNU_CC)_$(OS_ARCH))
	$(EXPAND_LINK) -NOLOGO -OUT:$@ -PDB:$(LINK_PDBFILE) $(WIN32_EXE_LDFLAGS) $(LDFLAGS) $(MOZ_PROGRAM_LDFLAGS) $(PROGOBJS) $(RESFILE) $(STATIC_LIBS) $(SHARED_LIBS) $(EXTRA_LIBS) $(OS_LIBS)
ifdef MSMANIFEST_TOOL
	@if test -f $@.manifest; then \
		if test -f '$(srcdir)/$@.manifest'; then \
			echo 'Embedding manifest from $(srcdir)/$@.manifest and $@.manifest'; \
			$(MT) -NOLOGO -MANIFEST '$(win_srcdir)/$@.manifest' $@.manifest -OUTPUTRESOURCE:$@\;1; \
		else \
			echo 'Embedding manifest from $@.manifest'; \
			$(MT) -NOLOGO -MANIFEST $@.manifest -OUTPUTRESOURCE:$@\;1; \
		fi; \
	elif test -f '$(srcdir)/$@.manifest'; then \
		echo 'Embedding manifest from $(srcdir)/$@.manifest'; \
		$(MT) -NOLOGO -MANIFEST '$(win_srcdir)/$@.manifest' -OUTPUTRESOURCE:$@\;1; \
	fi
endif	# MSVC with manifest tool
ifdef MOZ_PROFILE_GENERATE
# touch it a few seconds into the future to work around FAT's
# 2-second granularity
	touch -t `date +%Y%m%d%H%M.%S -d 'now+5seconds'` pgo.relink
endif
else # !WINNT || GNU_CC
	$(EXPAND_CCC) -o $@ $(CXXFLAGS) $(PROGOBJS) $(RESFILE) $(WIN32_EXE_LDFLAGS) $(LDFLAGS) $(WRAP_LDFLAGS) $(STATIC_LIBS) $(MOZ_PROGRAM_LDFLAGS) $(SHARED_LIBS) $(EXTRA_LIBS) $(OS_LIBS) $(BIN_FLAGS) $(EXE_DEF_FILE)
	$(call CHECK_BINARY,$@)
endif # WINNT && !GNU_CC

ifdef ENABLE_STRIP
	$(STRIP) $(STRIP_FLAGS) $@
endif
ifdef MOZ_POST_PROGRAM_COMMAND
	$(MOZ_POST_PROGRAM_COMMAND) $@
endif

$(HOST_PROGRAM): $(HOST_PROGOBJS) $(HOST_LIBS) $(HOST_EXTRA_DEPS) $(GLOBAL_DEPS)
	$(REPORT_BUILD)
ifeq (_WINNT,$(GNU_CC)_$(HOST_OS_ARCH))
	$(EXPAND_LIBS_EXEC) -- $(LINK) -NOLOGO -OUT:$@ -PDB:$(HOST_PDBFILE) $(HOST_OBJS) $(WIN32_EXE_LDFLAGS) $(HOST_LDFLAGS) $(HOST_LIBS) $(HOST_EXTRA_LIBS)
ifdef MSMANIFEST_TOOL
	@if test -f $@.manifest; then \
		if test -f '$(srcdir)/$@.manifest'; then \
			echo 'Embedding manifest from $(srcdir)/$@.manifest and $@.manifest'; \
			$(MT) -NOLOGO -MANIFEST '$(win_srcdir)/$@.manifest' $@.manifest -OUTPUTRESOURCE:$@\;1; \
		else \
			echo 'Embedding manifest from $@.manifest'; \
			$(MT) -NOLOGO -MANIFEST $@.manifest -OUTPUTRESOURCE:$@\;1; \
		fi; \
	elif test -f '$(srcdir)/$@.manifest'; then \
		echo 'Embedding manifest from $(srcdir)/$@.manifest'; \
		$(MT) -NOLOGO -MANIFEST '$(win_srcdir)/$@.manifest' -OUTPUTRESOURCE:$@\;1; \
	fi
endif	# MSVC with manifest tool
else
ifeq ($(HOST_CPP_PROG_LINK),1)
	$(EXPAND_LIBS_EXEC) -- $(HOST_CXX) -o $@ $(HOST_CXXFLAGS) $(HOST_LDFLAGS) $(HOST_PROGOBJS) $(HOST_LIBS) $(HOST_EXTRA_LIBS)
else
	$(EXPAND_LIBS_EXEC) -- $(HOST_CC) -o $@ $(HOST_CFLAGS) $(HOST_LDFLAGS) $(HOST_PROGOBJS) $(HOST_LIBS) $(HOST_EXTRA_LIBS)
endif # HOST_CPP_PROG_LINK
endif
ifndef CROSS_COMPILE
	$(call CHECK_STDCXX,$@)
endif

#
# This is an attempt to support generation of multiple binaries
# in one directory, it assumes everything to compile Foo is in
# Foo.o (from either Foo.c or Foo.cpp).
#
# SIMPLE_PROGRAMS = Foo Bar
# creates Foo.o Bar.o, links with LIBS to create Foo, Bar.
#
$(SIMPLE_PROGRAMS): %$(BIN_SUFFIX): %.$(OBJ_SUFFIX) $(STATIC_LIBS_DEPS) $(EXTRA_DEPS) $(GLOBAL_DEPS)
	$(REPORT_BUILD)
ifeq (_WINNT,$(GNU_CC)_$(OS_ARCH))
	$(EXPAND_LINK) -nologo -out:$@ -pdb:$(LINK_PDBFILE) $< $(WIN32_EXE_LDFLAGS) $(LDFLAGS) $(MOZ_PROGRAM_LDFLAGS) $(STATIC_LIBS) $(SHARED_LIBS) $(EXTRA_LIBS) $(OS_LIBS)
ifdef MSMANIFEST_TOOL
	@if test -f $@.manifest; then \
		$(MT) -NOLOGO -MANIFEST $@.manifest -OUTPUTRESOURCE:$@\;1; \
		rm -f $@.manifest; \
	fi
endif	# MSVC with manifest tool
else
	$(EXPAND_CCC) $(CXXFLAGS) -o $@ $< $(WIN32_EXE_LDFLAGS) $(LDFLAGS) $(WRAP_LDFLAGS) $(STATIC_LIBS) $(MOZ_PROGRAM_LDFLAGS) $(SHARED_LIBS) $(EXTRA_LIBS) $(OS_LIBS) $(BIN_FLAGS)
	$(call CHECK_BINARY,$@)
endif # WINNT && !GNU_CC

ifdef ENABLE_STRIP
	$(STRIP) $(STRIP_FLAGS) $@
endif
ifdef MOZ_POST_PROGRAM_COMMAND
	$(MOZ_POST_PROGRAM_COMMAND) $@
endif

$(HOST_SIMPLE_PROGRAMS): host_%$(HOST_BIN_SUFFIX): host_%.$(OBJ_SUFFIX) $(HOST_LIBS) $(HOST_EXTRA_DEPS) $(GLOBAL_DEPS)
	$(REPORT_BUILD)
ifeq (WINNT_,$(HOST_OS_ARCH)_$(GNU_CC))
	$(EXPAND_LIBS_EXEC) -- $(LINK) -NOLOGO -OUT:$@ -PDB:$(HOST_PDBFILE) $< $(WIN32_EXE_LDFLAGS) $(HOST_LIBS) $(HOST_EXTRA_LIBS)
else
ifneq (,$(HOST_CPPSRCS)$(USE_HOST_CXX))
	$(EXPAND_LIBS_EXEC) -- $(HOST_CXX) $(HOST_OUTOPTION)$@ $(HOST_CXXFLAGS) $(INCLUDES) $< $(HOST_LIBS) $(HOST_EXTRA_LIBS)
else
	$(EXPAND_LIBS_EXEC) -- $(HOST_CC) $(HOST_OUTOPTION)$@ $(HOST_CFLAGS) $(INCLUDES) $< $(HOST_LIBS) $(HOST_EXTRA_LIBS)
endif
endif
ifndef CROSS_COMPILE
	$(call CHECK_STDCXX,$@)
endif

$(filter %.$(LIB_SUFFIX),$(LIBRARY)): $(OBJS) $(STATIC_LIBS_DEPS) $(filter %.$(LIB_SUFFIX),$(EXTRA_LIBS)) $(EXTRA_DEPS) $(GLOBAL_DEPS)
	$(REPORT_BUILD)
# Always remove both library and library descriptor
	$(RM) $(REAL_LIBRARY) $(REAL_LIBRARY).$(LIBS_DESC_SUFFIX)
	$(EXPAND_AR) $(AR_FLAGS) $(OBJS) $(STATIC_LIBS) $(filter %.$(LIB_SUFFIX),$(EXTRA_LIBS))

$(filter-out %.$(LIB_SUFFIX),$(LIBRARY)): $(filter %.$(LIB_SUFFIX),$(LIBRARY)) $(OBJS) $(STATIC_LIBS_DEPS) $(filter %.$(LIB_SUFFIX),$(EXTRA_LIBS)) $(EXTRA_DEPS) $(GLOBAL_DEPS)
# When we only build a library descriptor, blow out any existing library
	$(REPORT_BUILD)
	$(if $(filter %.$(LIB_SUFFIX),$(LIBRARY)),,$(RM) $(REAL_LIBRARY))
	$(EXPAND_LIBS_GEN) -o $@ $(OBJS) $(STATIC_LIBS) $(filter %.$(LIB_SUFFIX),$(EXTRA_LIBS))

ifeq ($(OS_ARCH),WINNT)
# Import libraries are created by the rules creating shared libraries.
# The rules to copy them to $(DIST)/lib depend on $(IMPORT_LIBRARY),
# but make will happily consider the import library before it is refreshed
# when rebuilding the corresponding shared library. Defining an empty recipe
# for import libraries forces make to wait for the shared library recipe to
# have run before considering other targets that depend on the import library.
# See bug 795204.
$(IMPORT_LIBRARY): $(SHARED_LIBRARY) ;
endif

$(HOST_LIBRARY): $(HOST_OBJS) Makefile
	$(REPORT_BUILD)
	$(RM) $@
	$(EXPAND_LIBS_EXEC) --extract -- $(HOST_AR) $(HOST_AR_FLAGS) $(HOST_OBJS)

# On Darwin (Mac OS X), dwarf2 debugging uses debug info left in .o files,
# so instead of deleting .o files after repacking them into a dylib, we make
# symlinks back to the originals. The symlinks are a no-op for stabs debugging,
# so no need to conditionalize on OS version or debugging format.

$(SHARED_LIBRARY): $(OBJS) $(RESFILE) $(RUST_STATIC_LIB_FOR_SHARED_LIB) $(STATIC_LIBS_DEPS) $(EXTRA_DEPS) $(GLOBAL_DEPS)
	$(REPORT_BUILD)
ifndef INCREMENTAL_LINKER
	$(RM) $@
endif
	$(EXPAND_MKSHLIB) $(SHLIB_LDSTARTFILE) $(OBJS) $(SUB_SHLOBJS) $(RESFILE) $(LDFLAGS) $(WRAP_LDFLAGS) $(STATIC_LIBS) $(RUST_STATIC_LIB_FOR_SHARED_LIB) $(SHARED_LIBS) $(EXTRA_DSO_LDOPTS) $(MOZ_GLUE_LDFLAGS) $(EXTRA_LIBS) $(OS_LIBS) $(SHLIB_LDENDFILE)
	$(call CHECK_BINARY,$@)

ifeq (_WINNT,$(GNU_CC)_$(OS_ARCH))
ifdef MSMANIFEST_TOOL
ifdef EMBED_MANIFEST_AT
	@if test -f $@.manifest; then \
		if test -f '$(srcdir)/$@.manifest'; then \
			echo 'Embedding manifest from $(srcdir)/$@.manifest and $@.manifest'; \
			$(MT) -NOLOGO -MANIFEST '$(win_srcdir)/$@.manifest' $@.manifest -OUTPUTRESOURCE:$@\;$(EMBED_MANIFEST_AT); \
		else \
			echo 'Embedding manifest from $@.manifest'; \
			$(MT) -NOLOGO -MANIFEST $@.manifest -OUTPUTRESOURCE:$@\;$(EMBED_MANIFEST_AT); \
		fi; \
	elif test -f '$(srcdir)/$@.manifest'; then \
		echo 'Embedding manifest from $(srcdir)/$@.manifest'; \
		$(MT) -NOLOGO -MANIFEST '$(win_srcdir)/$@.manifest' -OUTPUTRESOURCE:$@\;$(EMBED_MANIFEST_AT); \
	fi
endif   # EMBED_MANIFEST_AT
endif	# MSVC with manifest tool
ifdef MOZ_PROFILE_GENERATE
	touch -t `date +%Y%m%d%H%M.%S -d 'now+5seconds'` pgo.relink
endif
endif	# WINNT && !GCC
	chmod +x $@
ifdef ENABLE_STRIP
	$(STRIP) $(STRIP_FLAGS) $@
endif

# The object file is in the current directory, and the source file can be any
# relative path. This macro adds the dependency obj: src for each source file.
# This dependency must be first for the $< flag to work correctly, and the
# rules that have commands for these targets must not list any other
# prerequisites, or they will override the $< variable.
define src_objdep
$(basename $2$(notdir $1)).$(OBJ_SUFFIX): $1 $$(call mkdir_deps,$$(MDDEPDIR))
endef
$(foreach f,$(CSRCS) $(SSRCS) $(CPPSRCS) $(CMSRCS) $(CMMSRCS) $(ASFILES),$(eval $(call src_objdep,$(f))))
$(foreach f,$(HOST_CSRCS) $(HOST_CPPSRCS) $(HOST_CMSRCS) $(HOST_CMMSRCS),$(eval $(call src_objdep,$(f),host_)))

# The Rust compiler only outputs library objects, and so we need different
# mangling to generate dependency rules for it.
mk_libname = $(basename lib$(notdir $1)).rlib
src_libdep = $(call mk_libname,$1): $1 $$(call mkdir_deps,$$(MDDEPDIR))
mk_global_crate_libname = $(basename lib$(notdir $1)).$(LIB_SUFFIX)
crate_src_libdep = $(call mk_global_crate_libname,$1): $1 $$(call mkdir_deps,$$(MDDEPDIR))
$(foreach f,$(RSSRCS),$(eval $(call src_libdep,$(f))))
$(foreach f,$(RS_STATICLIB_CRATE_SRC),$(eval $(call crate_src_libdep,$(f))))

$(OBJS) $(HOST_OBJS) $(PROGOBJS) $(HOST_PROGOBJS): $(GLOBAL_DEPS)

# Rules for building native targets must come first because of the host_ prefix
$(HOST_COBJS):
	$(REPORT_BUILD_VERBOSE)
	$(ELOG) $(HOST_CC) $(HOST_OUTOPTION)$@ -c $(HOST_CPPFLAGS) $(HOST_CFLAGS) $(INCLUDES) $(NSPR_CFLAGS) $(_VPATH_SRCS)

$(HOST_CPPOBJS):
	$(REPORT_BUILD_VERBOSE)
	$(call BUILDSTATUS,OBJECT_FILE $@)
	$(ELOG) $(HOST_CXX) $(HOST_OUTOPTION)$@ -c $(HOST_CPPFLAGS) $(HOST_CXXFLAGS) $(INCLUDES) $(NSPR_CFLAGS) $(_VPATH_SRCS)

$(HOST_CMOBJS):
	$(REPORT_BUILD_VERBOSE)
	$(ELOG) $(HOST_CC) $(HOST_OUTOPTION)$@ -c $(HOST_CPPFLAGS) $(HOST_CFLAGS) $(HOST_CMFLAGS) $(INCLUDES) $(NSPR_CFLAGS) $(_VPATH_SRCS)

$(HOST_CMMOBJS):
	$(REPORT_BUILD_VERBOSE)
	$(ELOG) $(HOST_CXX) $(HOST_OUTOPTION)$@ -c $(HOST_CPPFLAGS) $(HOST_CXXFLAGS) $(HOST_CMMFLAGS) $(INCLUDES) $(NSPR_CFLAGS) $(_VPATH_SRCS)

$(COBJS):
	$(REPORT_BUILD_VERBOSE)
	$(ELOG) $(CC) $(OUTOPTION)$@ -c $(COMPILE_CFLAGS) $($(notdir $<)_FLAGS) $(_VPATH_SRCS)

# DEFINES and ACDEFINES are needed here to enable conditional compilation of Q_OBJECTs:
# 'moc' only knows about #defines it gets on the command line (-D...), not in
# included headers like mozilla-config.h
$(filter moc_%.cpp,$(CPPSRCS)): moc_%.cpp: %.h
	$(REPORT_BUILD_VERBOSE)
	$(ELOG) $(MOC) $(DEFINES) $(ACDEFINES) $< $(OUTOPTION)$@

$(filter moc_%.cc,$(CPPSRCS)): moc_%.cc: %.cc
	$(REPORT_BUILD_VERBOSE)
	$(ELOG) $(MOC) $(DEFINES) $(ACDEFINES) $(_VPATH_SRCS:.cc=.h) $(OUTOPTION)$@

$(filter qrc_%.cpp,$(CPPSRCS)): qrc_%.cpp: %.qrc
	$(REPORT_BUILD_VERBOSE)
	$(ELOG) $(RCC) -name $* $< $(OUTOPTION)$@

ifdef ASFILES
# The AS_DASH_C_FLAG is needed cause not all assemblers (Solaris) accept
# a '-c' flag.
$(ASOBJS):
	$(REPORT_BUILD_VERBOSE)
	$(AS) $(ASOUTOPTION)$@ $(ASFLAGS) $($(notdir $<)_FLAGS) $(AS_DASH_C_FLAG) $(_VPATH_SRCS)
endif

define syms_template
syms:: $(2)
$(2): $(1)
	$$(call py_action,dumpsymbols,$$(abspath $$<) $$(abspath $$@))
endef

ifndef MOZ_PROFILE_GENERATE
ifneq (,$(filter $(DIST)/bin%,$(FINAL_TARGET)))
DUMP_SYMS_TARGETS := $(SHARED_LIBRARY) $(PROGRAM) $(SIMPLE_PROGRAMS)
endif
endif

ifdef MOZ_AUTOMATION
ifeq (,$(filter 1,$(MOZ_AUTOMATION_BUILD_SYMBOLS)))
DUMP_SYMS_TARGETS :=
endif
endif

$(foreach file,$(DUMP_SYMS_TARGETS),$(eval $(call syms_template,$(file),$(file)_syms.track)))

cargo_host_flag := --target=$(RUST_HOST_TARGET)
cargo_target_flag := --target=$(RUST_TARGET)

# Permit users to pass flags to cargo from their mozconfigs (e.g. --color=always).
cargo_build_flags = $(CARGOFLAGS)
ifndef MOZ_DEBUG_RUST
cargo_build_flags += --release
endif
cargo_build_flags += --frozen

cargo_build_flags += --manifest-path $(CARGO_FILE)
ifdef BUILD_VERBOSE_LOG
cargo_build_flags += --verbose
else
ifdef MOZ_AUTOMATION
cargo_build_flags += --verbose
endif # MOZ_AUTOMATION
endif # BUILD_VERBOSE_LOG

# Enable color output if original stdout was a TTY and color settings
# aren't already present. This essentially restores the default behavior
# of cargo when running via `mach`.
ifdef MACH_STDOUT_ISATTY
ifeq (,$(findstring --color,$(cargo_build_flags)))
cargo_build_flags += --color=always
endif
endif

# Cargo currently supports only two interesting profiles for building:
# development and release.  Those map (roughly) to --enable-debug and
# --disable-debug in Gecko, respectively, but there's another axis that we'd
# like to support: --{disable,enable}-optimize.  Since that would be four
# choices, and Cargo only supports two, we choose to enable various
# optimization levels in our Cargo.toml files all the time, and override the
# optimization level here, if necessary.  (The Cargo.toml files already
# specify debug-assertions appropriately for --{disable,enable}-debug.)
ifndef MOZ_OPTIMIZE
rustflags = -C opt-level=0
# Unfortunately, -C opt-level=0 implies -C debug-assertions, so we need
# to explicitly disable them when MOZ_DEBUG_RUST is not set.
ifndef MOZ_DEBUG_RUST
rustflags += -C debug-assertions=no
endif
rustflags_override = RUSTFLAGS='$(rustflags)'
endif

ifdef MOZ_MSVCBITS
# If we are building a MozillaBuild shell, we want to clear out the
# vcvars.bat environment variables for cargo builds. This is because
# a 32-bit MozillaBuild shell on a 64-bit machine will try to use
# the 32-bit compiler/linker for everything, while cargo/rustc wants
# to use the 64-bit linker for build.rs scripts. This conflict results
# in a build failure (see bug 1350001). So we clear out the environment
# variables that are actually relevant to 32- vs 64-bit builds.
environment_cleaner = PATH='' LIB='' LIBPATH=''
# The servo build needs to know where python is, and we're removing the PATH
# so we tell it explicitly via the PYTHON env var.
environment_cleaner += PYTHON='$(shell which $(PYTHON))'
else
environment_cleaner =
endif

# This function is intended to be called by:
#
#   $(call CARGO_BUILD,EXTRA_ENV_VAR1=X EXTRA_ENV_VAR2=Y ...)
#
# but, given the idiosyncracies of make, can also be called without arguments:
#
#   $(call CARGO_BUILD)
define CARGO_BUILD
env $(environment_cleaner) $(rustflags_override) \
	CARGO_TARGET_DIR=$(CARGO_TARGET_DIR) \
	RUSTC=$(RUSTC) \
	MOZ_SRC=$(topsrcdir) \
	MOZ_DIST=$(ABS_DIST) \
	LIBCLANG_PATH="$(MOZ_LIBCLANG_PATH)" \
	CLANG_PATH="$(MOZ_CLANG_PATH)" \
	PKG_CONFIG_ALLOW_CROSS=1 \
	RUST_BACKTRACE=1 \
	MOZ_TOPOBJDIR=$(topobjdir) \
	$(1) \
	$(CARGO) build $(cargo_build_flags)
endef

cargo_linker_env_var := CARGO_TARGET_$(RUST_TARGET_ENV_NAME)_LINKER

# Don't define a custom linker on Windows, as it's difficult to have a
# non-binary file that will get executed correctly by Cargo.  We don't
# have to worry about a cross-compiling (besides x86-64 -> x86, which
# already works with the current setup) setup on Windows, and we don't
# have to pass in any special linker options on Windows.
ifneq (WINNT,$(OS_ARCH))

# Defining all of this for ASan/TSan builds results in crashes while running
# some crates's build scripts (!), so disable it for now.
ifndef MOZ_ASAN
ifndef MOZ_TSAN
# Cargo needs the same linker flags as the C/C++ compiler,
# but not the final libraries. Filter those out because they
# cause problems on macOS 10.7; see bug 1365993 for details.
target_cargo_env_vars := \
	MOZ_CARGO_WRAP_LDFLAGS="$(filter-out -framework Cocoa -lobjc AudioToolbox ExceptionHandling,$(LDFLAGS))" \
	MOZ_CARGO_WRAP_LD="$(CC)" \
	$(cargo_linker_env_var)=$(topsrcdir)/build/cargo-linker
endif # MOZ_TSAN
endif # MOZ_ASAN

endif # ifneq WINNT

ifdef RUST_LIBRARY_FILE

ifdef RUST_LIBRARY_FEATURES
rust_features_flag := --features "$(RUST_LIBRARY_FEATURES)"
endif

# Assume any system libraries rustc links against are already in the target's LIBS.
#
# We need to run cargo unconditionally, because cargo is the only thing that
# has full visibility into how changes in Rust sources might affect the final
# build.
force-cargo-library-build:
	$(REPORT_BUILD)
	$(call CARGO_BUILD,$(target_cargo_env_vars)) --lib $(cargo_target_flag) $(rust_features_flag)

$(RUST_LIBRARY_FILE): force-cargo-library-build
endif # RUST_LIBRARY_FILE

ifdef HOST_RUST_LIBRARY_FILE

ifdef HOST_RUST_LIBRARY_FEATURES
host_rust_features_flag := --features "$(HOST_RUST_LIBRARY_FEATURES)"
endif

force-cargo-host-library-build:
	$(REPORT_BUILD)
	$(call CARGO_BUILD) --lib $(cargo_host_flag) $(host_rust_features_flag)

$(HOST_RUST_LIBRARY_FILE): force-cargo-host-library-build
endif # HOST_RUST_LIBRARY_FILE

ifdef RUST_PROGRAMS
force-cargo-program-build:
	$(REPORT_BUILD)
	$(call CARGO_BUILD,$(target_cargo_env_vars)) $(addprefix --bin ,$(RUST_CARGO_PROGRAMS)) $(cargo_target_flag)

$(RUST_PROGRAMS): force-cargo-program-build
endif # RUST_PROGRAMS
ifdef HOST_RUST_PROGRAMS
force-cargo-host-program-build:
	$(REPORT_BUILD)
	$(call CARGO_BUILD) $(addprefix --bin ,$(HOST_RUST_CARGO_PROGRAMS)) $(cargo_host_flag)

$(HOST_RUST_PROGRAMS): force-cargo-host-program-build
endif # HOST_RUST_PROGRAMS

$(SOBJS):
	$(REPORT_BUILD)
	$(AS) -o $@ $(DEFINES) $(ASFLAGS) $($(notdir $<)_FLAGS) $(LOCAL_INCLUDES) -c $<

$(CPPOBJS):
	$(REPORT_BUILD_VERBOSE)
	$(call BUILDSTATUS,OBJECT_FILE $@)
	$(ELOG) $(CCC) $(OUTOPTION)$@ -c $(COMPILE_CXXFLAGS) $($(notdir $<)_FLAGS) $(_VPATH_SRCS)

$(CMMOBJS):
	$(REPORT_BUILD_VERBOSE)
	$(ELOG) $(CCC) -o $@ -c $(COMPILE_CXXFLAGS) $(COMPILE_CMMFLAGS) $($(notdir $<)_FLAGS) $(_VPATH_SRCS)

$(CMOBJS):
	$(REPORT_BUILD_VERBOSE)
	$(ELOG) $(CC) -o $@ -c $(COMPILE_CFLAGS) $(COMPILE_CMFLAGS) $($(notdir $<)_FLAGS) $(_VPATH_SRCS)

$(filter %.s,$(CPPSRCS:%.cpp=%.s)): %.s: %.cpp $(call mkdir_deps,$(MDDEPDIR))
	$(REPORT_BUILD_VERBOSE)
	$(CCC) -S $(COMPILE_CXXFLAGS) $($(notdir $<)_FLAGS) $(_VPATH_SRCS)

$(filter %.s,$(CPPSRCS:%.cc=%.s)): %.s: %.cc $(call mkdir_deps,$(MDDEPDIR))
	$(REPORT_BUILD_VERBOSE)
	$(CCC) -S $(COMPILE_CXXFLAGS) $($(notdir $<)_FLAGS) $(_VPATH_SRCS)

$(filter %.s,$(CPPSRCS:%.cxx=%.s)): %.s: %.cpp $(call mkdir_deps,$(MDDEPDIR))
	$(REPORT_BUILD_VERBOSE)
	$(CCC) -S $(COMPILE_CXXFLAGS) $($(notdir $<)_FLAGS) $(_VPATH_SRCS)

$(filter %.s,$(CSRCS:%.c=%.s)): %.s: %.c $(call mkdir_deps,$(MDDEPDIR))
	$(REPORT_BUILD_VERBOSE)
	$(CC) -S $(COMPILE_CFLAGS) $($(notdir $<)_FLAGS) $(_VPATH_SRCS)

ifneq (,$(filter %.i,$(MAKECMDGOALS)))
# Call as $(call _group_srcs,extension,$(SRCS)) - this will create a list
# of the full sources, as well as the $(notdir) version. So:
#   foo.cpp sub/bar.cpp
# becomes:
#   foo.cpp sub/bar.cpp bar.cpp
#
# This way we can match both 'make sub/bar.i' and 'make bar.i'
_group_srcs = $(sort $(patsubst %.$1,%.i,$(filter %.$1,$2 $(notdir $2))))

define PREPROCESS_RULES
_PREPROCESSED_$1_FILES := $$(call _group_srcs,$1,$$($2))
# Make preprocessed files PHONY so they are always executed, since they are
# manual targets and we don't necessarily write to $@.
.PHONY: $$(_PREPROCESSED_$1_FILES)

# Hack up VPATH so we can reach the sources. Eg: 'make Parser.i' may need to
# reach $(srcdir)/frontend/Parser.i
vpath %.$1 $$(addprefix $$(srcdir)/,$$(sort $$(dir $$($2))))
vpath %.$1 $$(addprefix $$(CURDIR)/,$$(sort $$(dir $$($2))))

$$(_PREPROCESSED_$1_FILES): _DEPEND_CFLAGS=
$$(_PREPROCESSED_$1_FILES): %.i: %.$1
	$$(REPORT_BUILD_VERBOSE)
	$$(addprefix $$(MKDIR) -p ,$$(filter-out .,$$(@D)))
	$$($3) -C $$(PREPROCESS_OPTION)$$@ $(foreach var,$4,$$($(var))) $$($$(notdir $$<)_FLAGS) $$(_VPATH_SRCS)

endef

$(eval $(call PREPROCESS_RULES,cpp,CPPSRCS,CCC,COMPILE_CXXFLAGS))
$(eval $(call PREPROCESS_RULES,cc,CPPSRCS,CCC,COMPILE_CXXFLAGS))
$(eval $(call PREPROCESS_RULES,cxx,CPPSRCS,CCC,COMPILE_CXXFLAGS))
$(eval $(call PREPROCESS_RULES,c,CSRCS,CC,COMPILE_CFLAGS))
$(eval $(call PREPROCESS_RULES,mm,CMMSRCS,CCC,COMPILE_CXXFLAGS COMPILE_CMMFLAGS))

# Default to pre-processing the actual unified file. This can be overridden
# at the command-line to pre-process only the individual source file.
PP_UNIFIED ?= 1

# PP_REINVOKE gets set on the sub-make to prevent us from going in an
# infinite loop if the filename doesn't exist in the unified source files.
ifndef PP_REINVOKE

MATCH_cpp = \(cpp\|cc|cxx\)
UPPER_c = C
UPPER_cpp = CPP
UPPER_mm = CMM

# When building with PP_UNIFIED=0, we also have to look in the Unified files to
# find a matching pathname.
_get_all_sources = $1 $(if $(filter Unified%,$1),$(shell sed -n 's/\#include "\(.*\)"$$/\1/p' $(filter Unified%,$1)))
all_cpp_sources := $(call _get_all_sources,$(CPPSRCS))
all_mm_sources := $(call _get_all_sources,$(CMMSRCS))
all_c_sources := $(call _get_all_sources,$(CSRCS))
all_sources := $(all_cpp_sources) $(all_cmm_sources) $(all_c_sources)

# The catch-all %.i rule runs when we pass in a .i filename that doesn't match
# one of the *SRCS variables. The two code paths depend on whether or not
# we are requesting a unified file (PP_UNIFIED=1, the default) or not:
#
# PP_UNIFIED=1:
#  - Look for it in any of the Unified files, and re-exec make with
#    Unified_foo0.i as the target. This gets us the full unified preprocessed
#    file.
#
# PP_UNIFIED=0:
#  - If the .i filename is in *SRCS, or in a Unified filename, then we re-exec
#    make with that filename as the target. The *SRCS variables are modified
#    to have the Unified sources appended to them so that the static pattern
#    rules will match.
%.i: FORCE
ifeq ($(PP_UNIFIED),1)
	@$(MAKE) PP_REINVOKE=1 \
	    $(or $(addsuffix .i, \
              $(foreach type,c cpp mm, \
	        $(if $(filter Unified%,$($(UPPER_$(type))SRCS)), \
	          $(shell grep -l '#include "\(.*/\)\?$(basename $@).$(or $(MATCH_$(type)),$(type))"' Unified*.$(type) | sed 's/\.$(type)$$//') \
            ))),$(error "File not found for preprocessing: $@"))
else
	@$(MAKE) PP_REINVOKE=1 $@ \
	    $(foreach type,c cpp mm,$(UPPER_$(type))SRCS="$(all_$(type)_sources)")
endif

endif

endif

$(RESFILE): %.res: %.rc
	$(REPORT_BUILD)
	@echo Creating Resource file: $@
ifdef GNU_CC
	$(RC) $(RCFLAGS) $(filter-out -U%,$(DEFINES)) $(INCLUDES:-I%=--include-dir %) $(OUTOPTION)$@ $(_VPATH_SRCS)
else
	$(RC) $(RCFLAGS) -r $(DEFINES) $(INCLUDES) $(OUTOPTION)$@ $(_VPATH_SRCS)
endif

# Cancel GNU make built-in implicit rules
MAKEFLAGS += -r

ifneq (,$(filter WINNT,$(OS_ARCH)))
SEP := ;
else
SEP := :
endif

EMPTY :=
SPACE := $(EMPTY) $(EMPTY)

# MSYS has its own special path form, but javac expects the source and class
# paths to be in the DOS form (i.e. e:/builds/...).  This function does the
# appropriate conversion on Windows, but is a noop on other systems.
ifeq ($(HOST_OS_ARCH),WINNT)
#  We use 'pwd -W' to get DOS form of the path.  However, since the given path
#  could be a file or a non-existent path, we cannot call 'pwd -W' directly
#  on the path.  Instead, we extract the root path (i.e. "c:/"), call 'pwd -W'
#  on it, then merge with the rest of the path.
root-path = $(shell echo $(1) | sed -e 's|\(/[^/]*\)/\?\(.*\)|\1|')
non-root-path = $(shell echo $(1) | sed -e 's|\(/[^/]*\)/\?\(.*\)|\2|')
normalizepath = $(foreach p,$(1),$(if $(filter /%,$(1)),$(patsubst %/,%,$(shell cd $(call root-path,$(1)) && pwd -W))/$(call non-root-path,$(1)),$(1)))
else
normalizepath = $(1)
endif

###############################################################################
# Java rules
###############################################################################
ifneq (,$(JAVAFILES)$(ANDROID_RESFILES)$(ANDROID_APKNAME)$(JAVA_JAR_TARGETS))
  include $(MOZILLA_DIR)/config/makefiles/java-build.mk
endif

###############################################################################
# Bunch of things that extend the 'export' rule (in order):
###############################################################################

ifneq ($(XPI_NAME),)
$(FINAL_TARGET):
	$(NSINSTALL) -D $@

export:: $(FINAL_TARGET)
endif

################################################################################
# The default location for prefs is the gre prefs directory.
# PREF_DIR is used for L10N_PREF_JS_EXPORTS in various locales/ directories.
PREF_DIR = defaults/pref

# If DIST_SUBDIR is defined it indicates that app and gre dirs are
# different and that we are building app related resources. Hence,
# PREF_DIR should point to the app prefs location.
ifneq (,$(DIST_SUBDIR)$(XPI_NAME))
PREF_DIR = defaults/preferences
endif

################################################################################
# CHROME PACKAGING

chrome::
	$(MAKE) realchrome
	$(LOOP_OVER_DIRS)

$(FINAL_TARGET)/chrome: $(call mkdir_deps,$(FINAL_TARGET)/chrome)

ifneq (,$(JAR_MANIFEST))
ifndef NO_DIST_INSTALL

ifdef XPI_NAME
ifdef XPI_ROOT_APPID
# For add-on packaging we may specify that an application
# sub-dir should be added to the root chrome manifest with
# a specific application id.
MAKE_JARS_FLAGS += --root-manifest-entry-appid='$(XPI_ROOT_APPID)'
endif

# if DIST_SUBDIR is defined but XPI_ROOT_APPID is not there's
# no way langpacks will get packaged right, so error out.
ifneq (,$(DIST_SUBDIR))
ifndef XPI_ROOT_APPID
$(error XPI_ROOT_APPID is not defined - langpacks will break.)
endif
endif
endif

libs realchrome:: $(FINAL_TARGET)/chrome
	$(call py_action,jar_maker,\
	  $(QUIET) -d $(FINAL_TARGET) \
	  $(MAKE_JARS_FLAGS) $(DEFINES) $(ACDEFINES) \
	  $(JAR_MANIFEST))

endif

endif

# When you move this out of the tools tier, please remove the corresponding
# hacks in recursivemake.py that check if Makefile.in sets the variable.
ifneq ($(XPI_PKGNAME),)
tools realchrome::
ifdef STRIP_XPI
ifndef MOZ_DEBUG
	@echo 'Stripping $(XPI_PKGNAME) package directory...'
	@echo $(FINAL_TARGET)
	@cd $(FINAL_TARGET) && find . ! -type d \
			! -name '*.js' \
			! -name '*.xpt' \
			! -name '*.gif' \
			! -name '*.jpg' \
			! -name '*.png' \
			! -name '*.xpm' \
			! -name '*.txt' \
			! -name '*.rdf' \
			! -name '*.sh' \
			! -name '*.properties' \
			! -name '*.dtd' \
			! -name '*.html' \
			! -name '*.xul' \
			! -name '*.css' \
			! -name '*.xml' \
			! -name '*.jar' \
			! -name '*.dat' \
			! -name '*.tbl' \
			! -name '*.src' \
			! -name '*.reg' \
			$(PLATFORM_EXCLUDE_LIST) \
			-exec $(STRIP) $(STRIP_FLAGS) {} >/dev/null 2>&1 \;
endif
endif
	@echo 'Packaging $(XPI_PKGNAME).xpi...'
	$(call py_action,zip,-C $(FINAL_TARGET) ../$(XPI_PKGNAME).xpi '*')
endif

# See comment above about moving this out of the tools tier.
ifdef INSTALL_EXTENSION_ID
ifndef XPI_NAME
$(error XPI_NAME must be set for INSTALL_EXTENSION_ID)
endif

tools::
	$(RM) -r '$(DIST)/bin/distribution$(DIST_SUBDIR:%=/%)/extensions/$(INSTALL_EXTENSION_ID)'
	$(NSINSTALL) -D '$(DIST)/bin/distribution$(DIST_SUBDIR:%=/%)/extensions/$(INSTALL_EXTENSION_ID)'
	$(call copy_dir,$(FINAL_TARGET),$(DIST)/bin/distribution$(DIST_SUBDIR:%=/%)/extensions/$(INSTALL_EXTENSION_ID))

endif

#############################################################################
# MDDEPDIR is the subdirectory where all the dependency files are placed.
#   This uses a make rule (instead of a macro) to support parallel
#   builds (-jN). If this were done in the LOOP_OVER_DIRS macro, two
#   processes could simultaneously try to create the same directory.
#
#   We use $(CURDIR) in the rule's target to ensure that we don't find
#   a dependency directory in the source tree via VPATH (perhaps from
#   a previous build in the source tree) and thus neglect to create a
#   dependency directory in the object directory, where we really need
#   it.

ifneq (,$(filter-out all chrome default export realchrome clean clobber clobber_all distclean realclean,$(MAKECMDGOALS)))
MDDEPEND_FILES		:= $(strip $(wildcard $(addprefix $(MDDEPDIR)/,$(addsuffix .pp,$(notdir $(sort $(OBJS) $(PROGOBJS) $(HOST_OBJS) $(HOST_PROGOBJS)))))))

ifneq (,$(MDDEPEND_FILES))
-include $(MDDEPEND_FILES)
endif

endif

MDDEPEND_FILES		:= $(strip $(wildcard $(addprefix $(MDDEPDIR)/,$(EXTRA_MDDEPEND_FILES))))

ifneq (,$(MDDEPEND_FILES))
-include $(MDDEPEND_FILES)
endif

#############################################################################

-include $(topsrcdir)/$(MOZ_BUILD_APP)/app-rules.mk
-include $(MY_RULES)

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
# Install/copy rules
#
# The INSTALL_TARGETS variable contains a list of all install target
# categories. Each category defines a list of files and executables, and an
# install destination,
#
# FOO_FILES := foo bar
# FOO_EXECUTABLES := baz
# FOO_DEST := target_path
# INSTALL_TARGETS += FOO
#
# Additionally, a FOO_TARGET variable may be added to indicate the target for
# which the files and executables are installed. Default is "libs".
#
# Finally, a FOO_KEEP_PATH variable may be set to 1 to indicate the paths given
# in FOO_FILES/FOO_EXECUTABLES are to be kept at the destination. That is,
# if FOO_FILES is bar/baz/qux.h, and FOO_DEST is $(DIST)/include, the installed
# file would be $(DIST)/include/bar/baz/qux.h instead of $(DIST)/include/qux.h

# If we're using binary nsinstall and it's not built yet, fallback to python nsinstall.
ifneq (,$(filter $(DEPTH)/config/nsinstall$(HOST_BIN_SUFFIX),$(install_cmd)))
ifeq (,$(wildcard $(DEPTH)/config/nsinstall$(HOST_BIN_SUFFIX)))
nsinstall_is_usable = $(if $(wildcard $(DEPTH)/config/nsinstall$(HOST_BIN_SUFFIX)),yes)

define install_cmd_override
$(1): install_cmd = $$(if $$(nsinstall_is_usable),$$(INSTALL),$$(NSINSTALL_PY) -t) $$(1)
endef
endif
endif

install_target_tier = $(or $($(1)_TARGET),libs)
INSTALL_TARGETS_TIERS := $(sort $(foreach category,$(INSTALL_TARGETS),$(call install_target_tier,$(category))))

install_target_result = $($(1)_DEST:%/=%)/$(if $($(1)_KEEP_PATH),$(2),$(notdir $(2)))
install_target_files = $(foreach file,$($(1)_FILES),$(call install_target_result,$(category),$(file)))
install_target_executables = $(foreach file,$($(1)_EXECUTABLES),$(call install_target_result,$(category),$(file)))

# Work around a GNU make 3.81 bug where it gives $< the wrong value.
# See details in bug 934864.
define create_dependency
$(1): $(2)
$(1): $(2)
endef

define install_target_template
$(call install_cmd_override,$(2))
$(call create_dependency,$(2),$(1))
endef

$(foreach category,$(INSTALL_TARGETS),\
  $(if $($(category)_DEST),,$(error Missing $(category)_DEST)) \
  $(foreach tier,$(call install_target_tier,$(category)),\
    $(eval INSTALL_TARGETS_FILES_$(tier) += $(call install_target_files,$(category))) \
    $(eval INSTALL_TARGETS_EXECUTABLES_$(tier) += $(call install_target_executables,$(category))) \
  ) \
  $(foreach file,$($(category)_FILES) $($(category)_EXECUTABLES), \
    $(eval $(call install_target_template,$(file),$(call install_target_result,$(category),$(file)))) \
  ) \
)

$(foreach tier,$(INSTALL_TARGETS_TIERS), \
  $(eval $(tier):: $(INSTALL_TARGETS_FILES_$(tier)) $(INSTALL_TARGETS_EXECUTABLES_$(tier))) \
)

install_targets_sanity = $(if $(filter-out $(notdir $@),$(notdir $(<))),$(error Looks like $@ has an unexpected dependency on $< which breaks INSTALL_TARGETS))

$(sort $(foreach tier,$(INSTALL_TARGETS_TIERS),$(INSTALL_TARGETS_FILES_$(tier)))):
	$(install_targets_sanity)
	$(call install_cmd,$(IFLAGS1) '$<' '$(@D)')

$(sort $(foreach tier,$(INSTALL_TARGETS_TIERS),$(INSTALL_TARGETS_EXECUTABLES_$(tier)))):
	$(install_targets_sanity)
	$(call install_cmd,$(IFLAGS2) '$<' '$(@D)')

################################################################################
# Preprocessing rules
#
# The PP_TARGETS variable contains a list of all preprocessing target
# categories. Each category has associated variables listing input files, the
# output directory, extra preprocessor flags, and so on. For example:
#
#   FOO := input-file
#   FOO_PATH := target-directory
#   FOO_FLAGS := -Dsome_flag
#   PP_TARGETS += FOO
#
# If PP_TARGETS lists a category name <C> (like FOO, above), then we consult the
# following make variables to see what to do:
#
# - <C> lists input files to be preprocessed with mozbuild.action.preprocessor.
#   We search VPATH for the names given here. If an input file name ends in
#   '.in', that suffix is omitted from the output file name.
#
# - <C>_PATH names the directory in which to place the preprocessed output
#   files. We create this directory if it does not already exist. Setting
#   this variable is optional; if unset, we install the files in $(CURDIR).
#
# - <C>_FLAGS lists flags to pass to mozbuild.action.preprocessor, in addition
#   to the usual bunch. Setting this variable is optional.
#
# - <C>_TARGET names the 'make' target that should depend on creating the output
#   files. Setting this variable is optional; if unset, we preprocess the
#   files for the 'libs' target.
#
# - <C>_KEEP_PATH may be set to 1 to indicate the paths given in <C> are to be
#   kept under <C>_PATH. That is, if <C> is bar/baz/qux.h.in and <C>_PATH is
#   $(DIST)/include, the preprocessed file would be $(DIST)/include/bar/baz/qux.h
#   instead of $(DIST)/include/qux.h.

pp_target_tier = $(or $($(1)_TARGET),libs)
PP_TARGETS_TIERS := $(sort $(foreach category,$(PP_TARGETS),$(call pp_target_tier,$(category))))

pp_target_result = $(or $($(1)_PATH:%/=%),$(CURDIR))/$(if $($(1)_KEEP_PATH),$(2:.in=),$(notdir $(2:.in=)))
pp_target_results = $(foreach file,$($(1)),$(call pp_target_result,$(category),$(file)))

$(foreach category,$(PP_TARGETS), \
  $(foreach tier,$(call pp_target_tier,$(category)), \
    $(eval PP_TARGETS_RESULTS_$(tier) += $(call pp_target_results,$(category))) \
  ) \
  $(foreach file,$($(category)), \
    $(eval $(call create_dependency,$(call pp_target_result,$(category),$(file)), \
                                    $(file) $(GLOBAL_DEPS))) \
  ) \
  $(eval $(call pp_target_results,$(category)): PP_TARGET_FLAGS=$($(category)_FLAGS)) \
)

$(foreach tier,$(PP_TARGETS_TIERS), \
  $(eval $(tier):: $(PP_TARGETS_RESULTS_$(tier))) \
)

PP_TARGETS_ALL_RESULTS := $(sort $(foreach tier,$(PP_TARGETS_TIERS),$(PP_TARGETS_RESULTS_$(tier))))
$(PP_TARGETS_ALL_RESULTS):
	$(if $(filter-out $(notdir $@),$(notdir $(<:.in=))),$(error Looks like $@ has an unexpected dependency on $< which breaks PP_TARGETS))
	$(RM) '$@'
	$(call py_action,preprocessor,--depend $(MDDEPDIR)/$(@F).pp $(PP_TARGET_FLAGS) $(DEFINES) $(ACDEFINES) '$<' -o '$@')

$(filter %.css,$(PP_TARGETS_ALL_RESULTS)): PP_TARGET_FLAGS+=--marker %

# The depfile is based on the filename, and we don't want conflicts. So check
# there's only one occurrence of any given filename in PP_TARGETS_ALL_RESULTS.
PP_TARGETS_ALL_RESULT_NAMES := $(notdir $(PP_TARGETS_ALL_RESULTS))
$(foreach file,$(sort $(PP_TARGETS_ALL_RESULT_NAMES)), \
  $(if $(filter-out 1,$(words $(filter $(file),$(PP_TARGETS_ALL_RESULT_NAMES)))), \
    $(error Multiple preprocessing rules are creating a $(file) file) \
  ) \
)

ifneq (,$(filter $(PP_TARGETS_TIERS) $(PP_TARGETS_ALL_RESULTS),$(MAKECMDGOALS)))
# If the depfile for a preprocessed file doesn't exist, add a dep to force
# re-preprocessing.
$(foreach file,$(PP_TARGETS_ALL_RESULTS), \
  $(if $(wildcard $(MDDEPDIR)/$(notdir $(file)).pp), \
    , \
    $(eval $(file): FORCE) \
  ) \
)

MDDEPEND_FILES := $(strip $(wildcard $(addprefix $(MDDEPDIR)/,$(addsuffix .pp,$(notdir $(PP_TARGETS_ALL_RESULTS))))))

ifneq (,$(MDDEPEND_FILES))
-include $(MDDEPEND_FILES)
endif

endif

# Pull in non-recursive targets if this is a partial tree build.
ifndef TOPLEVEL_BUILD
include $(MOZILLA_DIR)/config/makefiles/nonrecursive.mk
endif

################################################################################
# Special gmake rules.
################################################################################


#
# Re-define the list of default suffixes, so gmake won't have to churn through
# hundreds of built-in suffix rules for stuff we don't need.
#
.SUFFIXES:

#
# Fake targets.  Always run these rules, even if a file/directory with that
# name already exists.
#
.PHONY: all alltags boot checkout chrome realchrome clean clobber clobber_all export install libs makefiles realclean run_apprunner tools $(DIRS) FORCE

# Used as a dependency to force targets to rebuild
FORCE:

# Delete target if error occurs when building target
.DELETE_ON_ERROR:

tags: TAGS

TAGS: $(CSRCS) $(CPPSRCS) $(wildcard *.h)
	-etags $(CSRCS) $(CPPSRCS) $(wildcard *.h)
	$(LOOP_OVER_DIRS)

ifndef INCLUDED_DEBUGMAKE_MK #{
  ## Only parse when an echo* or show* target is requested
  ifneq (,$(call isTargetStem,echo,show))
    include $(MOZILLA_DIR)/config/makefiles/debugmake.mk
  endif #}
endif #}

documentation:
	@cd $(DEPTH)
	$(DOXYGEN) $(DEPTH)/config/doxygen.cfg

FREEZE_VARIABLES = \
  CSRCS \
  CPPSRCS \
  EXPORTS \
  DIRS \
  LIBRARY \
  MODULE \
  $(NULL)

$(foreach var,$(FREEZE_VARIABLES),$(eval $(var)_FROZEN := '$($(var))'))

CHECK_FROZEN_VARIABLES = $(foreach var,$(FREEZE_VARIABLES), \
  $(if $(subst $($(var)_FROZEN),,'$($(var))'),$(error Makefile variable '$(var)' changed value after including rules.mk. Was $($(var)_FROZEN), now $($(var)).)))

libs export::
	$(CHECK_FROZEN_VARIABLES)

PURGECACHES_DIRS ?= $(DIST)/bin

PURGECACHES_FILES = $(addsuffix /.purgecaches,$(PURGECACHES_DIRS))

default all:: $(PURGECACHES_FILES)

$(PURGECACHES_FILES):
	if test -d $(@D) ; then touch $@ ; fi

.DEFAULT_GOAL := $(or $(OVERRIDE_DEFAULT_GOAL),default)

#############################################################################
# Derived targets and dependencies

include $(MOZILLA_DIR)/config/makefiles/autotargets.mk
ifneq ($(NULL),$(AUTO_DEPS))
  default all libs tools export:: $(AUTO_DEPS)
endif
