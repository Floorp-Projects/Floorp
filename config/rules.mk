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

# Make sure that anything that needs to be defined in moz.build wasn't
# overwritten.
_eval_for_side_effects := $(CHECK_MOZBUILD_VARIABLES)

ifndef MOZILLA_DIR
MOZILLA_DIR = $(topsrcdir)
endif

ifndef INCLUDED_CONFIG_MK
include $(topsrcdir)/config/config.mk
endif

ifndef INCLUDED_VERSION_MK
include $(topsrcdir)/config/version.mk
endif

USE_AUTOTARGETS_MK = 1
include $(topsrcdir)/config/makefiles/makeutils.mk

# Only build with Pymake (not GNU make) on Windows.
ifeq ($(HOST_OS_ARCH),WINNT)
ifndef L10NBASEDIR
ifndef .PYMAKE
$(error Pymake is required to build on Windows. Run |./mach build| to \
automatically use pymake or invoke pymake directly via \
|python build/pymake/make.py|.)
endif
endif
endif

ifdef REBUILD_CHECK
ifdef .PYMAKE
REPORT_BUILD = @%rebuild_check rebuild_check $@ $^
else
REPORT_BUILD = $(info $(shell $(PYTHON) $(MOZILLA_DIR)/config/rebuild_check.py $@ $^))
endif
else
REPORT_BUILD = $(info $(notdir $@))
endif

ifeq ($(OS_ARCH),OS2)
EXEC			=
else
EXEC			= exec
endif

# Don't copy xulrunner files at install time, when using system xulrunner
ifdef SYSTEM_LIBXUL
  SKIP_COPY_XULRUNNER=1
endif

# ELOG prints out failed command when building silently (gmake -s). Pymake
# prints out failed commands anyway, so ELOG just makes things worse by
# forcing shell invocations.
ifndef .PYMAKE
ifneq (,$(findstring s, $(filter-out --%, $(MAKEFLAGS))))
  ELOG := $(EXEC) sh $(BUILD_TOOLS)/print-failed-commands.sh
else
  ELOG :=
endif # -s
else
  ELOG :=
endif # ifndef .PYMAKE

_VPATH_SRCS = $(abspath $<)

################################################################################
# Testing frameworks support
################################################################################

testxpcobjdir = $(DEPTH)/_tests/xpcshell

ifdef ENABLE_TESTS

# Add test directories to the regular directories list. TEST_DIRS should
# arguably have the same status as TOOL_DIRS and other *_DIRS variables. It is
# coded this way until Makefiles stop using the "ifdef ENABLE_TESTS; DIRS +="
# convention.
#
# The current developer workflow expects tests to be updated when processing
# the default target. If we ever change this implementation, the behavior
# should be preserved or the change should be widely communicated. A
# consequence of not processing test dir targets during the default target is
# that changes to tests may not be updated and code could assume to pass
# locally against non-current test code.
DIRS += $(TEST_DIRS)

ifndef INCLUDED_TESTS_MOCHITEST_MK #{
  include $(topsrcdir)/config/makefiles/mochitest.mk
endif #}

ifdef CPP_UNIT_TESTS
ifdef COMPILE_ENVIRONMENT

# Compile the tests to $(DIST)/bin.  Make lots of niceties available by default
# through TestHarness.h, by modifying the list of includes and the libs against
# which stuff links.
CPPSRCS += $(CPP_UNIT_TESTS)
CPP_UNIT_TEST_BINS := $(CPP_UNIT_TESTS:.cpp=$(BIN_SUFFIX))
SIMPLE_PROGRAMS += $(CPP_UNIT_TEST_BINS)
INCLUDES += -I$(DIST)/include/testing
LIBS += $(XPCOM_GLUE_LDOPTS) $(NSPR_LIBS)

ifndef MOZ_PROFILE_GENERATE
libs:: $(CPP_UNIT_TEST_BINS) $(call mkdir_deps,$(DIST)/cppunittests)
	$(NSINSTALL) $(CPP_UNIT_TEST_BINS) $(DIST)/cppunittests
endif

check::
	@$(PYTHON) $(topsrcdir)/testing/runcppunittests.py --xre-path=$(DIST)/bin --symbols-path=$(DIST)/crashreporter-symbols $(subst .cpp,$(BIN_SUFFIX),$(CPP_UNIT_TESTS))

cppunittests-remote: DM_TRANS?=adb
cppunittests-remote:
	@if [ '${TEST_DEVICE}' != '' -o '$(DM_TRANS)' = 'adb' ]; then \
		$(PYTHON) -u $(topsrcdir)/testing/remotecppunittests.py \
			--xre-path=$(DEPTH)/dist/bin \
			--localLib=$(DEPTH)/dist/$(MOZ_APP_NAME) \
			--dm_trans=$(DM_TRANS) \
			--deviceIP=${TEST_DEVICE} \
			$(subst .cpp,$(BIN_SUFFIX),$(CPP_UNIT_TESTS)) $(EXTRA_TEST_ARGS); \
	else \
		echo 'please prepare your host with environment variables for TEST_DEVICE'; \
	fi

endif # COMPILE_ENVIRONMENT
endif # CPP_UNIT_TESTS

.PHONY: check

ifdef PYTHON_UNIT_TESTS

RUN_PYTHON_UNIT_TESTS := $(addsuffix -run,$(PYTHON_UNIT_TESTS))

.PHONY: $(RUN_PYTHON_UNIT_TESTS)

check:: $(RUN_PYTHON_UNIT_TESTS)

$(RUN_PYTHON_UNIT_TESTS): %-run: %
	@PYTHONDONTWRITEBYTECODE=1 $(PYTHON) $<

endif # PYTHON_UNIT_TESTS

endif # ENABLE_TESTS


#
# Library rules
#
# If FORCE_STATIC_LIB is set, build a static library.
# Otherwise, build a shared library.
#

ifndef LIBRARY
ifdef STATIC_LIBRARY_NAME
REAL_LIBRARY		:= $(LIB_PREFIX)$(STATIC_LIBRARY_NAME).$(LIB_SUFFIX)
# Only build actual library if it is installed in DIST/lib or SDK
ifeq (,$(SDK_LIBRARY)$(DIST_INSTALL)$(NO_EXPAND_LIBS))
LIBRARY			:= $(REAL_LIBRARY).$(LIBS_DESC_SUFFIX)
else
LIBRARY			:= $(REAL_LIBRARY) $(REAL_LIBRARY).$(LIBS_DESC_SUFFIX)
endif
endif # STATIC_LIBRARY_NAME
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

ifneq (,$(filter OS2 WINNT,$(OS_ARCH)))
IMPORT_LIBRARY		:= $(LIB_PREFIX)$(SHARED_LIBRARY_NAME).$(IMPORT_LIB_SUFFIX)
endif

ifdef MAKE_FRAMEWORK
SHARED_LIBRARY		:= $(SHARED_LIBRARY_NAME)
else
SHARED_LIBRARY		:= $(DLL_PREFIX)$(SHARED_LIBRARY_NAME)$(DLL_SUFFIX)
endif

ifeq ($(OS_ARCH),OS2)
DEF_FILE		:= $(SHARED_LIBRARY:.dll=.def)
endif

EMBED_MANIFEST_AT=2

endif # MKSHLIB
endif # FORCE_SHARED_LIB
endif # LIBRARY

ifdef FORCE_STATIC_LIB
ifndef FORCE_SHARED_LIB
SHARED_LIBRARY		:= $(NULL)
DEF_FILE		:= $(NULL)
IMPORT_LIBRARY		:= $(NULL)
endif
endif

ifdef FORCE_SHARED_LIB
ifndef FORCE_STATIC_LIB
LIBRARY := $(NULL)
endif
endif

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
COMPILE_PDBFILE = $(basename $(@F)).pdb
else
COMPILE_PDBFILE = generated.pdb
endif

LINK_PDBFILE = $(basename $(@F)).pdb
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

ifeq ($(SOLARIS_SUNPRO_CXX),1)
ifeq (86,$(findstring 86,$(OS_TEST)))
OS_LDFLAGS += -M $(topsrcdir)/config/solaris_ia32.map
endif # x86
endif # Solaris Sun Studio C++

ifeq ($(HOST_OS_ARCH),WINNT)
HOST_PDBFILE=$(basename $(@F)).pdb
endif

# Don't build SIMPLE_PROGRAMS during the MOZ_PROFILE_GENERATE pass
ifdef MOZ_PROFILE_GENERATE
EXCLUDED_OBJS := $(SIMPLE_PROGRAMS:$(BIN_SUFFIX)=.$(OBJ_SUFFIX))
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
ASOBJS = $(notdir $(ASFILES:.$(ASM_SUFFIX)=.$(OBJ_SUFFIX)))
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
SDK_BINARY := $(filter %.py,$(SDK_BINARY))
SDK_LIBRARY :=
endif

ALL_TRASH = \
	$(GARBAGE) $(TARGETS) $(OBJS) $(PROGOBJS) LOGS TAGS a.out \
	$(filter-out $(ASFILES),$(OBJS:.$(OBJ_SUFFIX)=.s)) $(OBJS:.$(OBJ_SUFFIX)=.ii) \
	$(OBJS:.$(OBJ_SUFFIX)=.i) $(OBJS:.$(OBJ_SUFFIX)=.i_o) \
	$(HOST_PROGOBJS) $(HOST_OBJS) $(IMPORT_LIBRARY) $(DEF_FILE)\
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

#
# the Solaris WorkShop template repository cache.  it occasionally can get
# out of sync, so targets like clobber should kill it.
#
ifeq ($(SOLARIS_SUNPRO_CXX),1)
GARBAGE_DIRS += SunWS_cache
endif

ifdef MOZ_UPDATE_XTERM
# Its good not to have a newline at the end of the titlebar string because it
# makes the make -s output easier to read.  Echo -n does not work on all
# platforms, but we can trick printf into doing it.
UPDATE_TITLE = printf '\033]0;%s in %s\007' $(1) $(relativesrcdir)/$(2) ;
endif

ifdef MACH
ifndef NO_BUILDSTATUS_MESSAGES
define BUILDSTATUS
@echo 'BUILDSTATUS $1'

endef
endif
endif

define SUBMAKE # $(call SUBMAKE,target,directory,static)
+@$(UPDATE_TITLE)
+$(MAKE) $(if $(2),-C $(2)) $(1)

endef # The extra line is important here! don't delete it

define TIER_DIR_SUBMAKE
$(call BUILDSTATUS,TIERDIR_START  $(1) $(2) $(3))
$(call SUBMAKE,$(4),$(3),$(5))
$(call BUILDSTATUS,TIERDIR_FINISH $(1) $(2) $(3))

endef # Ths empty line is important.

ifneq (,$(strip $(DIRS)))
LOOP_OVER_DIRS = \
  $(foreach dir,$(DIRS),$(call SUBMAKE,$@,$(dir)))
endif

# we only use this for the makefiles target and other stuff that doesn't matter
ifneq (,$(strip $(PARALLEL_DIRS)))
LOOP_OVER_PARALLEL_DIRS = \
  $(foreach dir,$(PARALLEL_DIRS),$(call SUBMAKE,$@,$(dir)))
endif

ifneq (,$(strip $(TOOL_DIRS)))
LOOP_OVER_TOOL_DIRS = \
  $(foreach dir,$(TOOL_DIRS),$(call SUBMAKE,$@,$(dir)))
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
EXTRA_DSO_LDOPTS	+= -dynamiclib -install_name @executable_path/$(SHARED_LIBRARY) -compatibility_version 1 -current_version 1 -single_module
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
PREPROCESS_OPTION = -P -Fi# eol
else
OUTOPTION = -o # eol
PREPROCESS_OPTION = -E -o #eol
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
$(DEPTH)/backend.RecursiveMakeBackend:
	$(error Build configuration changed. Build with |mach build| or run |mach build-backend| to regenerate build config)

include $(DEPTH)/backend.RecursiveMakeBackend.pp

default:: $(DEPTH)/backend.RecursiveMakeBackend

export MOZBUILD_BACKEND_CHECKED=1
endif
endif
endif

# The root makefile doesn't want to do a plain export/libs, because
# of the tiers and because of libxul. Suppress the default rules in favor
# of something else. Makefiles which use this var *must* provide a sensible
# default rule before including rules.mk
ifndef SUPPRESS_DEFAULT_RULES
default all::
	$(MAKE) export
ifdef MOZ_PSEUDO_DERECURSE
ifdef COMPILE_ENVIRONMENT
	$(MAKE) compile
endif
endif
	$(MAKE) libs
	$(MAKE) tools
endif # SUPPRESS_DEFAULT_RULES

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

ifneq (,$(filter-out %.$(LIB_SUFFIX),$(SHARED_LIBRARY_LIBS)))
$(error SHARED_LIBRARY_LIBS must contain .$(LIB_SUFFIX) files only)
endif

HOST_LIBS_DEPS = $(filter %.$(LIB_SUFFIX),$(HOST_LIBS))

# Dependencies which, if modified, should cause everything to rebuild
GLOBAL_DEPS += Makefile $(DEPTH)/config/autoconf.mk $(topsrcdir)/config/config.mk

##############################################
ifdef COMPILE_ENVIRONMENT
OBJ_TARGETS = $(OBJS) $(PROGOBJS) $(HOST_OBJS) $(HOST_PROGOBJS)

compile:: $(OBJ_TARGETS)

include $(topsrcdir)/config/makefiles/target_binaries.mk
endif

ifdef IS_TOOL_DIR
# One would think "tools:: libs" would work, but it turns out that combined with
# bug 907365, this makes make forget to run some rules sometimes.
tools::
	@$(MAKE) libs
endif

##############################################
ifndef NO_PROFILE_GUIDED_OPTIMIZE
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
	$(PYTHON) $(topsrcdir)/build/win32/pgomerge.py \
	  $(PROGRAM:$(BIN_SUFFIX)=) $(DIST)/bin
endif
ifdef SHARED_LIBRARY
	$(PYTHON) $(topsrcdir)/build/win32/pgomerge.py \
	  $(SHARED_LIBRARY_NAME) $(DIST)/bin
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
ifdef GNU_CC
# Force rebuilding libraries and programs in both passes because each
# pass uses different object files.
$(PROGRAM) $(SHARED_LIBRARY) $(LIBRARY): FORCE
endif
endif

endif # NO_PROFILE_GUIDED_OPTIMIZE

##############################################

checkout:
	$(MAKE) -C $(topsrcdir) -f client.mk checkout

clean clobber realclean clobber_all::
	-$(RM) $(ALL_TRASH)
	-$(RM) -r $(ALL_TRASH_DIRS)

ifdef TIERS
clean clobber realclean clobber_all distclean::
	$(foreach dir, \
		$(foreach tier, $(TIERS), $(tier_$(tier)_staticdirs) $(tier_$(tier)_dirs)), \
		-$(call SUBMAKE,$@,$(dir)))
else
clean clobber realclean clobber_all distclean::
	$(foreach dir,$(PARALLEL_DIRS) $(DIRS) $(TOOL_DIRS),-$(call SUBMAKE,$@,$(dir)))

distclean::
	$(foreach dir,$(PARALLEL_DIRS) $(DIRS) $(TOOL_DIRS),-$(call SUBMAKE,$@,$(dir)))
endif

distclean::
	-$(RM) -r $(ALL_TRASH_DIRS)
	-$(RM) $(ALL_TRASH)  \
	Makefile .HSancillary \
	$(wildcard *.$(OBJ_SUFFIX)) $(wildcard *.ho) $(wildcard host_*.o*) \
	$(wildcard *.$(LIB_SUFFIX)) $(wildcard *$(DLL_SUFFIX)) \
	$(wildcard *.$(IMPORT_LIB_SUFFIX))
ifeq ($(OS_ARCH),OS2)
	-$(RM) $(PROGRAM:.exe=.map)
endif

alltags:
	$(RM) TAGS
	find $(topsrcdir) -name dist -prune -o \( -name '*.[hc]' -o -name '*.cp' -o -name '*.cpp' -o -name '*.idl' \) -print | $(TAG_PROGRAM)

#
# PROGRAM = Foo
# creates OBJS, links with LIBS to create Foo
#
$(PROGRAM): $(PROGOBJS) $(EXTRA_DEPS) $(EXE_DEF_FILE) $(RESFILE) $(GLOBAL_DEPS)
	$(REPORT_BUILD)
	@$(RM) $@.manifest
ifeq (_WINNT,$(GNU_CC)_$(OS_ARCH))
	$(EXPAND_LD) -NOLOGO -OUT:$@ -PDB:$(LINK_PDBFILE) $(WIN32_EXE_LDFLAGS) $(LDFLAGS) $(MOZ_GLUE_PROGRAM_LDFLAGS) $(PROGOBJS) $(RESFILE) $(LIBS) $(EXTRA_LIBS) $(OS_LIBS)
ifdef MSMANIFEST_TOOL
	@if test -f $@.manifest; then \
		if test -f '$(srcdir)/$@.manifest'; then \
			echo 'Embedding manifest from $(srcdir)/$@.manifest and $@.manifest'; \
			mt.exe -NOLOGO -MANIFEST '$(win_srcdir)/$@.manifest' $@.manifest -OUTPUTRESOURCE:$@\;1; \
		else \
			echo 'Embedding manifest from $@.manifest'; \
			mt.exe -NOLOGO -MANIFEST $@.manifest -OUTPUTRESOURCE:$@\;1; \
		fi; \
	elif test -f '$(srcdir)/$@.manifest'; then \
		echo 'Embedding manifest from $(srcdir)/$@.manifest'; \
		mt.exe -NOLOGO -MANIFEST '$(win_srcdir)/$@.manifest' -OUTPUTRESOURCE:$@\;1; \
	fi
endif	# MSVC with manifest tool
ifdef MOZ_PROFILE_GENERATE
# touch it a few seconds into the future to work around FAT's
# 2-second granularity
	touch -t `date +%Y%m%d%H%M.%S -d 'now+5seconds'` pgo.relink
endif
else # !WINNT || GNU_CC
	$(EXPAND_CCC) -o $@ $(CXXFLAGS) $(PROGOBJS) $(RESFILE) $(WIN32_EXE_LDFLAGS) $(LDFLAGS) $(WRAP_LDFLAGS) $(LIBS_DIR) $(LIBS) $(MOZ_GLUE_PROGRAM_LDFLAGS) $(OS_LIBS) $(EXTRA_LIBS) $(BIN_FLAGS) $(EXE_DEF_FILE) $(STLPORT_LIBS)
	@$(call CHECK_STDCXX,$@)
endif # WINNT && !GNU_CC

ifdef ENABLE_STRIP
	$(STRIP) $(STRIP_FLAGS) $@
endif
ifdef MOZ_POST_PROGRAM_COMMAND
	$(MOZ_POST_PROGRAM_COMMAND) $@
endif

$(HOST_PROGRAM): $(HOST_PROGOBJS) $(HOST_LIBS_DEPS) $(HOST_EXTRA_DEPS) $(GLOBAL_DEPS)
	$(REPORT_BUILD)
ifeq (_WINNT,$(GNU_CC)_$(HOST_OS_ARCH))
	$(EXPAND_LIBS_EXEC) -- $(HOST_LD) -NOLOGO -OUT:$@ -PDB:$(HOST_PDBFILE) $(HOST_OBJS) $(WIN32_EXE_LDFLAGS) $(HOST_LDFLAGS) $(HOST_LIBS) $(HOST_EXTRA_LIBS)
ifdef MSMANIFEST_TOOL
	@if test -f $@.manifest; then \
		if test -f '$(srcdir)/$@.manifest'; then \
			echo 'Embedding manifest from $(srcdir)/$@.manifest and $@.manifest'; \
			mt.exe -NOLOGO -MANIFEST '$(win_srcdir)/$@.manifest' $@.manifest -OUTPUTRESOURCE:$@\;1; \
		else \
			echo 'Embedding manifest from $@.manifest'; \
			mt.exe -NOLOGO -MANIFEST $@.manifest -OUTPUTRESOURCE:$@\;1; \
		fi; \
	elif test -f '$(srcdir)/$@.manifest'; then \
		echo 'Embedding manifest from $(srcdir)/$@.manifest'; \
		mt.exe -NOLOGO -MANIFEST '$(win_srcdir)/$@.manifest' -OUTPUTRESOURCE:$@\;1; \
	fi
endif	# MSVC with manifest tool
else
ifeq ($(HOST_CPP_PROG_LINK),1)
	$(EXPAND_LIBS_EXEC) -- $(HOST_CXX) -o $@ $(HOST_CXXFLAGS) $(HOST_LDFLAGS) $(HOST_PROGOBJS) $(HOST_LIBS) $(HOST_EXTRA_LIBS)
else
	$(EXPAND_LIBS_EXEC) -- $(HOST_CC) -o $@ $(HOST_CFLAGS) $(HOST_LDFLAGS) $(HOST_PROGOBJS) $(HOST_LIBS) $(HOST_EXTRA_LIBS)
endif # HOST_CPP_PROG_LINK
endif

#
# This is an attempt to support generation of multiple binaries
# in one directory, it assumes everything to compile Foo is in
# Foo.o (from either Foo.c or Foo.cpp).
#
# SIMPLE_PROGRAMS = Foo Bar
# creates Foo.o Bar.o, links with LIBS to create Foo, Bar.
#
$(SIMPLE_PROGRAMS): %$(BIN_SUFFIX): %.$(OBJ_SUFFIX) $(EXTRA_DEPS) $(GLOBAL_DEPS)
	$(REPORT_BUILD)
ifeq (_WINNT,$(GNU_CC)_$(OS_ARCH))
	$(EXPAND_LD) -nologo -out:$@ -pdb:$(LINK_PDBFILE) $< $(WIN32_EXE_LDFLAGS) $(LDFLAGS) $(MOZ_GLUE_PROGRAM_LDFLAGS) $(LIBS) $(EXTRA_LIBS) $(OS_LIBS)
ifdef MSMANIFEST_TOOL
	@if test -f $@.manifest; then \
		mt.exe -NOLOGO -MANIFEST $@.manifest -OUTPUTRESOURCE:$@\;1; \
		rm -f $@.manifest; \
	fi
endif	# MSVC with manifest tool
else
	$(EXPAND_CCC) $(CXXFLAGS) -o $@ $< $(WIN32_EXE_LDFLAGS) $(LDFLAGS) $(WRAP_LDFLAGS) $(LIBS_DIR) $(LIBS) $(MOZ_GLUE_PROGRAM_LDFLAGS) $(OS_LIBS) $(EXTRA_LIBS) $(BIN_FLAGS) $(STLPORT_LIBS)
	@$(call CHECK_STDCXX,$@)
endif # WINNT && !GNU_CC

ifdef ENABLE_STRIP
	$(STRIP) $(STRIP_FLAGS) $@
endif
ifdef MOZ_POST_PROGRAM_COMMAND
	$(MOZ_POST_PROGRAM_COMMAND) $@
endif

$(HOST_SIMPLE_PROGRAMS): host_%$(HOST_BIN_SUFFIX): host_%.$(OBJ_SUFFIX) $(HOST_LIBS_DEPS) $(HOST_EXTRA_DEPS) $(GLOBAL_DEPS)
	$(REPORT_BUILD)
ifeq (WINNT_,$(HOST_OS_ARCH)_$(GNU_CC))
	$(EXPAND_LIBS_EXEC) -- $(HOST_LD) -NOLOGO -OUT:$@ -PDB:$(HOST_PDBFILE) $< $(WIN32_EXE_LDFLAGS) $(HOST_LIBS) $(HOST_EXTRA_LIBS)
else
ifneq (,$(HOST_CPPSRCS)$(USE_HOST_CXX))
	$(EXPAND_LIBS_EXEC) -- $(HOST_CXX) $(HOST_OUTOPTION)$@ $(HOST_CXXFLAGS) $(INCLUDES) $< $(HOST_LIBS) $(HOST_EXTRA_LIBS)
else
	$(EXPAND_LIBS_EXEC) -- $(HOST_CC) $(HOST_OUTOPTION)$@ $(HOST_CFLAGS) $(INCLUDES) $< $(HOST_LIBS) $(HOST_EXTRA_LIBS)
endif
endif

ifdef DTRACE_PROBE_OBJ
EXTRA_DEPS += $(DTRACE_PROBE_OBJ)
OBJS += $(DTRACE_PROBE_OBJ)
endif

$(filter %.$(LIB_SUFFIX),$(LIBRARY)): $(OBJS) $(EXTRA_DEPS) $(GLOBAL_DEPS)
	$(REPORT_BUILD)
	$(RM) $(LIBRARY)
	$(EXPAND_AR) $(AR_FLAGS) $(OBJS) $(SHARED_LIBRARY_LIBS)

$(filter-out %.$(LIB_SUFFIX),$(LIBRARY)): $(filter %.$(LIB_SUFFIX),$(LIBRARY)) $(OBJS) $(EXTRA_DEPS) $(GLOBAL_DEPS)
# When we only build a library descriptor, blow out any existing library
	$(REPORT_BUILD)
	$(if $(filter %.$(LIB_SUFFIX),$(LIBRARY)),,$(RM) $(REAL_LIBRARY) $(EXPORT_LIBRARY:%=%/$(REAL_LIBRARY)))
	$(EXPAND_LIBS_GEN) -o $@ $(OBJS) $(SHARED_LIBRARY_LIBS)

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

ifeq ($(OS_ARCH),OS2)
$(DEF_FILE): $(OBJS) $(SHARED_LIBRARY_LIBS)
	$(RM) $@
	echo LIBRARY $(SHARED_LIBRARY_NAME) INITINSTANCE TERMINSTANCE > $@
	echo PROTMODE >> $@
	echo CODE    LOADONCALL MOVEABLE DISCARDABLE >> $@
	echo DATA    PRELOAD MOVEABLE MULTIPLE NONSHARED >> $@
	echo EXPORTS >> $@

	$(ADD_TO_DEF_FILE)

$(IMPORT_LIBRARY): $(SHARED_LIBRARY)
	$(REPORT_BUILD)
	$(RM) $@
	$(IMPLIB) $@ $^
endif # OS/2

$(HOST_LIBRARY): $(HOST_OBJS) Makefile
	$(REPORT_BUILD)
	$(RM) $@
	$(EXPAND_LIBS_EXEC) --extract -- $(HOST_AR) $(HOST_AR_FLAGS) $(HOST_OBJS)

ifdef HAVE_DTRACE
ifndef XP_MACOSX
ifdef DTRACE_PROBE_OBJ
ifndef DTRACE_LIB_DEPENDENT
NON_DTRACE_OBJS := $(filter-out $(DTRACE_PROBE_OBJ),$(OBJS))
$(DTRACE_PROBE_OBJ): $(NON_DTRACE_OBJS)
	dtrace -G -C -s $(MOZILLA_DTRACE_SRC) -o $(DTRACE_PROBE_OBJ) $(NON_DTRACE_OBJS)
endif
endif
endif
endif

# On Darwin (Mac OS X), dwarf2 debugging uses debug info left in .o files,
# so instead of deleting .o files after repacking them into a dylib, we make
# symlinks back to the originals. The symlinks are a no-op for stabs debugging,
# so no need to conditionalize on OS version or debugging format.

$(SHARED_LIBRARY): $(OBJS) $(DEF_FILE) $(RESFILE) $(LIBRARY) $(EXTRA_DEPS) $(GLOBAL_DEPS)
	$(REPORT_BUILD)
ifndef INCREMENTAL_LINKER
	$(RM) $@
endif
ifdef DTRACE_LIB_DEPENDENT
ifndef XP_MACOSX
	dtrace -G -C -s $(MOZILLA_DTRACE_SRC) -o  $(DTRACE_PROBE_OBJ) $(shell $(EXPAND_LIBS) $(MOZILLA_PROBE_LIBS))
endif
	$(EXPAND_MKSHLIB) $(SHLIB_LDSTARTFILE) $(OBJS) $(SUB_SHLOBJS) $(DTRACE_PROBE_OBJ) $(MOZILLA_PROBE_LIBS) $(RESFILE) $(LDFLAGS) $(WRAP_LDFLAGS) $(SHARED_LIBRARY_LIBS) $(EXTRA_DSO_LDOPTS) $(MOZ_GLUE_LDFLAGS) $(OS_LIBS) $(EXTRA_LIBS) $(DEF_FILE) $(SHLIB_LDENDFILE) $(if $(LIB_IS_C_ONLY),,$(STLPORT_LIBS))
	@$(RM) $(DTRACE_PROBE_OBJ)
else # ! DTRACE_LIB_DEPENDENT
	$(EXPAND_MKSHLIB) $(SHLIB_LDSTARTFILE) $(OBJS) $(SUB_SHLOBJS) $(RESFILE) $(LDFLAGS) $(WRAP_LDFLAGS) $(SHARED_LIBRARY_LIBS) $(EXTRA_DSO_LDOPTS) $(MOZ_GLUE_LDFLAGS) $(OS_LIBS) $(EXTRA_LIBS) $(DEF_FILE) $(SHLIB_LDENDFILE) $(if $(LIB_IS_C_ONLY),,$(STLPORT_LIBS))
endif # DTRACE_LIB_DEPENDENT
	@$(call CHECK_STDCXX,$@)

ifeq (_WINNT,$(GNU_CC)_$(OS_ARCH))
ifdef MSMANIFEST_TOOL
ifdef EMBED_MANIFEST_AT
	@if test -f $@.manifest; then \
		mt.exe -NOLOGO -MANIFEST $@.manifest -OUTPUTRESOURCE:$@\;$(EMBED_MANIFEST_AT); \
		rm -f $@.manifest; \
	fi
endif   # EMBED_MANIFEST_AT
endif	# MSVC with manifest tool
ifdef MOZ_PROFILE_GENERATE
	touch -t `date +%Y%m%d%H%M.%S -d 'now+5seconds'` pgo.relink
endif
endif	# WINNT && !GCC
	@$(RM) foodummyfilefoo $(DELETE_AFTER_LINK)
	chmod +x $@
ifdef ENABLE_STRIP
	$(STRIP) $(STRIP_FLAGS) $@
endif
ifdef MOZ_POST_DSO_LIB_COMMAND
	$(MOZ_POST_DSO_LIB_COMMAND) $@
endif

ifeq ($(SOLARIS_SUNPRO_CC),1)
_MDDEPFILE = $(MDDEPDIR)/$(@F).pp

define MAKE_DEPS_AUTO_CC
if test -d $(@D); then \
	echo 'Building deps for $< using Sun Studio cc'; \
	$(CC) $(COMPILE_CFLAGS) -xM  $< >$(_MDDEPFILE) ; \
	$(PYTHON) $(topsrcdir)/build/unix/add_phony_targets.py $(_MDDEPFILE) ; \
fi
endef
define MAKE_DEPS_AUTO_CXX
if test -d $(@D); then \
	echo 'Building deps for $< using Sun Studio CC'; \
	$(CXX) $(COMPILE_CXXFLAGS) -xM $< >$(_MDDEPFILE) ; \
	$(PYTHON) $(topsrcdir)/build/unix/add_phony_targets.py $(_MDDEPFILE) ; \
fi
endef
endif # Sun Studio on Solaris

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

$(OBJS) $(HOST_OBJS) $(PROGOBJS) $(HOST_PROGOBJS): $(GLOBAL_DEPS)

# Rules for building native targets must come first because of the host_ prefix
$(HOST_COBJS):
	$(REPORT_BUILD)
	$(ELOG) $(HOST_CC) $(HOST_OUTOPTION)$@ -c $(HOST_CFLAGS) $(INCLUDES) $(NSPR_CFLAGS) $(_VPATH_SRCS)

$(HOST_CPPOBJS):
	$(REPORT_BUILD)
	$(ELOG) $(HOST_CXX) $(HOST_OUTOPTION)$@ -c $(HOST_CXXFLAGS) $(INCLUDES) $(NSPR_CFLAGS) $(_VPATH_SRCS)

$(HOST_CMOBJS):
	$(REPORT_BUILD)
	$(ELOG) $(HOST_CC) $(HOST_OUTOPTION)$@ -c $(HOST_CFLAGS) $(HOST_CMFLAGS) $(INCLUDES) $(NSPR_CFLAGS) $(_VPATH_SRCS)

$(HOST_CMMOBJS):
	$(REPORT_BUILD)
	$(ELOG) $(HOST_CXX) $(HOST_OUTOPTION)$@ -c $(HOST_CXXFLAGS) $(HOST_CMMFLAGS) $(INCLUDES) $(NSPR_CFLAGS) $(_VPATH_SRCS)

$(COBJS):
	$(REPORT_BUILD)
	@$(MAKE_DEPS_AUTO_CC)
	$(ELOG) $(CC) $(OUTOPTION)$@ -c $(COMPILE_CFLAGS) $(TARGET_LOCAL_INCLUDES) $(_VPATH_SRCS)

# DEFINES and ACDEFINES are needed here to enable conditional compilation of Q_OBJECTs:
# 'moc' only knows about #defines it gets on the command line (-D...), not in
# included headers like mozilla-config.h
$(filter moc_%.cpp,$(CPPSRCS)): moc_%.cpp: %.h
	$(REPORT_BUILD)
	$(ELOG) $(MOC) $(DEFINES) $(ACDEFINES) $< $(OUTOPTION)$@

$(filter moc_%.cc,$(CPPSRCS)): moc_%.cc: %.cc
	$(REPORT_BUILD)
	$(ELOG) $(MOC) $(DEFINES) $(ACDEFINES) $(_VPATH_SRCS:.cc=.h) $(OUTOPTION)$@

$(filter qrc_%.cpp,$(CPPSRCS)): qrc_%.cpp: %.qrc
	$(REPORT_BUILD)
	$(ELOG) $(RCC) -name $* $< $(OUTOPTION)$@

ifdef ASFILES
# The AS_DASH_C_FLAG is needed cause not all assemblers (Solaris) accept
# a '-c' flag.
$(ASOBJS):
	$(REPORT_BUILD)
	$(AS) $(ASOUTOPTION)$@ $(ASFLAGS) $(AS_DASH_C_FLAG) $(_VPATH_SRCS)
endif

$(SOBJS):
	$(REPORT_BUILD)
	$(AS) -o $@ $(ASFLAGS) $(LOCAL_INCLUDES) $(TARGET_LOCAL_INCLUDES) -c $<

$(CPPOBJS):
	$(REPORT_BUILD)
	@$(MAKE_DEPS_AUTO_CXX)
	$(ELOG) $(CCC) $(OUTOPTION)$@ -c $(COMPILE_CXXFLAGS) $(TARGET_LOCAL_INCLUDES) $(_VPATH_SRCS)

$(CMMOBJS):
	$(REPORT_BUILD)
	@$(MAKE_DEPS_AUTO_CXX)
	$(ELOG) $(CCC) -o $@ -c $(COMPILE_CXXFLAGS) $(COMPILE_CMMFLAGS) $(TARGET_LOCAL_INCLUDES) $(_VPATH_SRCS)

$(CMOBJS):
	$(REPORT_BUILD)
	@$(MAKE_DEPS_AUTO_CC)
	$(ELOG) $(CC) -o $@ -c $(COMPILE_CFLAGS) $(COMPILE_CMFLAGS) $(TARGET_LOCAL_INCLUDES) $(_VPATH_SRCS)

$(filter %.s,$(CPPSRCS:%.cpp=%.s)): %.s: %.cpp $(call mkdir_deps,$(MDDEPDIR))
	$(REPORT_BUILD)
	$(CCC) -S $(COMPILE_CXXFLAGS) $(TARGET_LOCAL_INCLUDES) $(_VPATH_SRCS)

$(filter %.s,$(CPPSRCS:%.cc=%.s)): %.s: %.cc $(call mkdir_deps,$(MDDEPDIR))
	$(REPORT_BUILD)
	$(CCC) -S $(COMPILE_CXXFLAGS) $(TARGET_LOCAL_INCLUDES) $(_VPATH_SRCS)

$(filter %.s,$(CSRCS:%.c=%.s)): %.s: %.c $(call mkdir_deps,$(MDDEPDIR))
	$(REPORT_BUILD)
	$(CC) -S $(COMPILE_CFLAGS) $(TARGET_LOCAL_INCLUDES) $(_VPATH_SRCS)

$(filter %.i,$(CPPSRCS:%.cpp=%.i)): %.i: %.cpp $(call mkdir_deps,$(MDDEPDIR))
	$(REPORT_BUILD)
	$(CCC) -C $(PREPROCESS_OPTION)$@ $(COMPILE_CXXFLAGS) $(TARGET_LOCAL_INCLUDES) $(_VPATH_SRCS)

$(filter %.i,$(CPPSRCS:%.cc=%.i)): %.i: %.cc $(call mkdir_deps,$(MDDEPDIR))
	$(REPORT_BUILD)
	$(CCC) -C $(PREPROCESS_OPTION)$@ $(COMPILE_CXXFLAGS) $(TARGET_LOCAL_INCLUDES) $(_VPATH_SRCS)

$(filter %.i,$(CSRCS:%.c=%.i)): %.i: %.c $(call mkdir_deps,$(MDDEPDIR))
	$(REPORT_BUILD)
	$(CC) -C $(PREPROCESS_OPTION)$@ $(COMPILE_CFLAGS) $(TARGET_LOCAL_INCLUDES) $(_VPATH_SRCS)

$(filter %.i,$(CMMSRCS:%.mm=%.i)): %.i: %.mm $(call mkdir_deps,$(MDDEPDIR))
	$(REPORT_BUILD)
	$(CCC) -C $(PREPROCESS_OPTION)$@ $(COMPILE_CXXFLAGS) $(COMPILE_CMMFLAGS) $(TARGET_LOCAL_INCLUDES) $(_VPATH_SRCS)

$(RESFILE): %.res: %.rc
	$(REPORT_BUILD)
	@echo Creating Resource file: $@
ifeq ($(OS_ARCH),OS2)
	$(RC) $(RCFLAGS:-D%=-d %) -i $(subst /,\,$(srcdir)) -r $< $@
else
ifdef GNU_CC
	$(RC) $(RCFLAGS) $(filter-out -U%,$(DEFINES)) $(INCLUDES:-I%=--include-dir %) $(OUTOPTION)$@ $(_VPATH_SRCS)
else
	$(RC) $(RCFLAGS) -r $(DEFINES) $(INCLUDES) $(OUTOPTION)$@ $(_VPATH_SRCS)
endif
endif

# Cancel GNU make built-in implicit rules
ifndef .PYMAKE
MAKEFLAGS += -r
endif

ifneq (,$(filter OS2 WINNT,$(OS_ARCH)))
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
  include $(topsrcdir)/config/makefiles/java-build.mk
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
# Copy each element of PREF_JS_EXPORTS

# The default location for PREF_JS_EXPORTS is the gre prefs directory.
PREF_DIR = defaults/pref

# If DIST_SUBDIR is defined it indicates that app and gre dirs are
# different and that we are building app related resources. Hence,
# PREF_DIR should point to the app prefs location.
ifneq (,$(DIST_SUBDIR)$(XPI_NAME)$(LIBXUL_SDK))
PREF_DIR = defaults/preferences
endif

# on win32, pref files need CRLF line endings... see bug 206029
ifeq (WINNT,$(OS_ARCH))
PREF_PPFLAGS += --line-endings=crlf
endif

ifneq ($(PREF_JS_EXPORTS),)
ifndef NO_DIST_INSTALL
PREF_JS_EXPORTS_PATH := $(FINAL_TARGET)/$(PREF_DIR)
PREF_JS_EXPORTS_FLAGS := $(PREF_PPFLAGS)
PP_TARGETS += PREF_JS_EXPORTS
endif
endif

################################################################################
# Copy each element of AUTOCFG_JS_EXPORTS to $(FINAL_TARGET)/defaults/autoconfig

ifneq ($(AUTOCFG_JS_EXPORTS),)
ifndef NO_DIST_INSTALL
AUTOCFG_JS_EXPORTS_FILES := $(AUTOCFG_JS_EXPORTS)
AUTOCFG_JS_EXPORTS_DEST := $(FINAL_TARGET)/defaults/autoconfig
AUTOCFG_JS_EXPORTS_TARGET := export
INSTALL_TARGETS += AUTOCFG_JS_EXPORTS
endif
endif

################################################################################
# Install a linked .xpt into the appropriate place.
# This should ideally be performed by the non-recursive idl make file. Some day.
ifdef XPT_NAME #{

ifndef NO_DIST_INSTALL
_XPT_NAME_FILES := $(DEPTH)/config/makefiles/xpidl/xpt/$(XPT_NAME)
_XPT_NAME_DEST := $(FINAL_TARGET)/components
INSTALL_TARGETS += _XPT_NAME

ifndef NO_INTERFACES_MANIFEST
libs:: $(call mkdir_deps,$(FINAL_TARGET)/components)
	$(call py_action,buildlist,$(FINAL_TARGET)/components/interfaces.manifest 'interfaces $(XPT_NAME)')
	$(call py_action,buildlist,$(FINAL_TARGET)/chrome.manifest 'manifest components/interfaces.manifest')
endif
endif

endif #} XPT_NAME

################################################################################
# Copy each element of EXTRA_COMPONENTS to $(FINAL_TARGET)/components
ifneq (,$(filter %.js,$(EXTRA_COMPONENTS) $(EXTRA_PP_COMPONENTS)))
ifeq (,$(filter %.manifest,$(EXTRA_COMPONENTS) $(EXTRA_PP_COMPONENTS)))
ifndef NO_JS_MANIFEST
$(error .js component without matching .manifest. See https://developer.mozilla.org/en/XPCOM/XPCOM_changes_in_Gecko_2.0)
endif
endif
endif

ifdef EXTRA_COMPONENTS
libs:: $(EXTRA_COMPONENTS)
ifndef NO_DIST_INSTALL
EXTRA_COMPONENTS_FILES := $(EXTRA_COMPONENTS)
EXTRA_COMPONENTS_DEST := $(FINAL_TARGET)/components
INSTALL_TARGETS += EXTRA_COMPONENTS
endif

endif

ifdef EXTRA_PP_COMPONENTS
ifndef NO_DIST_INSTALL
EXTRA_PP_COMPONENTS_PATH := $(FINAL_TARGET)/components
PP_TARGETS += EXTRA_PP_COMPONENTS
endif
endif

EXTRA_MANIFESTS = $(filter %.manifest,$(EXTRA_COMPONENTS) $(EXTRA_PP_COMPONENTS))
ifneq (,$(EXTRA_MANIFESTS))
libs:: $(call mkdir_deps,$(FINAL_TARGET))
	$(call py_action,buildlist,$(FINAL_TARGET)/chrome.manifest $(patsubst %,'manifest components/%',$(notdir $(EXTRA_MANIFESTS))))
endif

################################################################################
# Copy each element of EXTRA_JS_MODULES to
# $(FINAL_TARGET)/$(JS_MODULES_PATH). JS_MODULES_PATH defaults to "modules"
# if it is undefined.
JS_MODULES_PATH ?= modules
FINAL_JS_MODULES_PATH := $(FINAL_TARGET)/$(JS_MODULES_PATH)

ifdef EXTRA_JS_MODULES
ifndef NO_DIST_INSTALL
EXTRA_JS_MODULES_FILES := $(EXTRA_JS_MODULES)
EXTRA_JS_MODULES_DEST := $(FINAL_JS_MODULES_PATH)
INSTALL_TARGETS += EXTRA_JS_MODULES
endif
endif

ifdef EXTRA_PP_JS_MODULES
ifndef NO_DIST_INSTALL
EXTRA_PP_JS_MODULES_PATH := $(FINAL_JS_MODULES_PATH)
PP_TARGETS += EXTRA_PP_JS_MODULES
endif
endif

################################################################################
# Copy testing-only JS modules to appropriate destination.
#
# For each file defined in TESTING_JS_MODULES, copy it to
# objdir/_tests/modules/. If TESTING_JS_MODULE_DIR is defined, that path
# wlll be appended to the output directory.

ifdef ENABLE_TESTS
ifdef TESTING_JS_MODULES
testmodulesdir = $(DEPTH)/_tests/modules/$(TESTING_JS_MODULE_DIR)

GENERATED_DIRS += $(testmodulesdir)

ifndef NO_DIST_INSTALL
TESTING_JS_MODULES_FILES := $(TESTING_JS_MODULES)
TESTING_JS_MODULES_DEST := $(testmodulesdir)
INSTALL_TARGETS += TESTING_JS_MODULES
endif

endif
endif

################################################################################
# SDK

ifneq (,$(SDK_LIBRARY))
ifndef NO_DIST_INSTALL
SDK_LIBRARY_FILES := $(SDK_LIBRARY)
SDK_LIBRARY_DEST := $(SDK_LIB_DIR)
INSTALL_TARGETS += SDK_LIBRARY
endif
endif # SDK_LIBRARY

ifneq (,$(strip $(SDK_BINARY)))
ifndef NO_DIST_INSTALL
SDK_BINARY_FILES := $(SDK_BINARY)
SDK_BINARY_DEST := $(SDK_BIN_DIR)
INSTALL_TARGETS += SDK_BINARY
endif
endif # SDK_BINARY

################################################################################
# CHROME PACKAGING

JAR_MANIFEST := $(srcdir)/jar.mn

chrome::
	$(MAKE) realchrome
	$(LOOP_OVER_PARALLEL_DIRS)
	$(LOOP_OVER_DIRS)
	$(LOOP_OVER_TOOL_DIRS)

$(FINAL_TARGET)/chrome: $(call mkdir_deps,$(FINAL_TARGET)/chrome)

ifneq (,$(wildcard $(JAR_MANIFEST)))
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
	  $(QUIET) -j $(FINAL_TARGET)/chrome \
	  $(MAKE_JARS_FLAGS) $(XULPPFLAGS) $(DEFINES) $(ACDEFINES) \
	  $(JAR_MANIFEST))

endif
endif

ifneq ($(DIST_FILES),)
DIST_FILES_PATH := $(FINAL_TARGET)
DIST_FILES_FLAGS := $(XULAPP_DEFINES)
PP_TARGETS += DIST_FILES
endif

ifneq ($(DIST_CHROME_FILES),)
DIST_CHROME_FILES_PATH := $(FINAL_TARGET)/chrome
DIST_CHROME_FILES_FLAGS := $(XULAPP_DEFINES)
PP_TARGETS += DIST_CHROME_FILES
endif

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
	cd $(FINAL_TARGET) && $(ZIP) -qr ../$(XPI_PKGNAME).xpi *
endif

ifdef INSTALL_EXTENSION_ID
ifndef XPI_NAME
$(error XPI_NAME must be set for INSTALL_EXTENSION_ID)
endif

tools::
	$(RM) -r '$(DIST)/bin$(DIST_SUBDIR:%=/%)/extensions/$(INSTALL_EXTENSION_ID)'
	$(NSINSTALL) -D '$(DIST)/bin$(DIST_SUBDIR:%=/%)/extensions/$(INSTALL_EXTENSION_ID)'
	$(call copy_dir,$(FINAL_TARGET),$(DIST)/bin$(DIST_SUBDIR:%=/%)/extensions/$(INSTALL_EXTENSION_ID))

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
MDDEPEND_FILES		:= $(strip $(wildcard $(addprefix $(MDDEPDIR)/,$(EXTRA_MDDEPEND_FILES) $(addsuffix .pp,$(notdir $(sort $(OBJS) $(PROGOBJS) $(HOST_OBJS) $(HOST_PROGOBJS))) $(TARGETS)))))

ifneq (,$(MDDEPEND_FILES))
$(call include_deps,$(MDDEPEND_FILES))
endif

endif


ifneq (,$(filter export,$(MAKECMDGOALS)))
MDDEPEND_FILES		:= $(strip $(wildcard $(addprefix $(MDDEPDIR)/,$(EXTRA_EXPORT_MDDEPEND_FILES))))

ifneq (,$(MDDEPEND_FILES))
$(call include_deps,$(MDDEPEND_FILES))
endif

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
ifneq (,$(filter $(CONFIG_TOOLS)/nsinstall$(HOST_BIN_SUFFIX),$(install_cmd)))
ifeq (,$(wildcard $(CONFIG_TOOLS)/nsinstall$(HOST_BIN_SUFFIX)))
nsinstall_is_usable = $(if $(wildcard $(CONFIG_TOOLS)/nsinstall$(HOST_BIN_SUFFIX)),yes)

define install_cmd_override
$(1): install_cmd = $$(if $$(nsinstall_is_usable),$$(INSTALL),$$(NSINSTALL_PY)) $$(1)
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
	$(call py_action,preprocessor,--depend $(MDDEPDIR)/$(@F).pp $(PP_TARGET_FLAGS) $(DEFINES) $(ACDEFINES) $(XULPPFLAGS) '$<' -o '$@')

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
$(call include_deps,$(MDDEPEND_FILES))
endif

endif

# Pull in non-recursive targets if this is a partial tree build.
ifndef TOPLEVEL_BUILD
include $(topsrcdir)/config/makefiles/nonrecursive.mk
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
.PHONY: all alltags boot checkout chrome realchrome clean clobber clobber_all export install libs makefiles realclean run_apprunner tools $(DIRS) $(TOOL_DIRS) FORCE

# Used as a dependency to force targets to rebuild
FORCE:

# Delete target if error occurs when building target
.DELETE_ON_ERROR:

tags: TAGS

TAGS: $(CSRCS) $(CPPSRCS) $(wildcard *.h)
	-etags $(CSRCS) $(CPPSRCS) $(wildcard *.h)
	$(LOOP_OVER_PARALLEL_DIRS)
	$(LOOP_OVER_DIRS)

ifndef INCLUDED_DEBUGMAKE_MK #{
  ## Only parse when an echo* or show* target is requested
  ifneq (,$(call isTargetStem,echo,show))
    include $(topsrcdir)/config/makefiles/debugmake.mk
  endif #}
endif #}

documentation:
	@cd $(DEPTH)
	$(DOXYGEN) $(DEPTH)/config/doxygen.cfg

ifdef ENABLE_TESTS
check::
	$(LOOP_OVER_PARALLEL_DIRS)
	$(LOOP_OVER_DIRS)
	$(LOOP_OVER_TOOL_DIRS)
endif


FREEZE_VARIABLES = \
  CSRCS \
  CPPSRCS \
  EXPORTS \
  DIRS \
  LIBRARY \
  MODULE \
  TIERS \
  EXTRA_COMPONENTS \
  EXTRA_PP_COMPONENTS \
  MOCHITEST_FILES \
  MOCHITEST_CHROME_FILES \
  MOCHITEST_BROWSER_FILES \
  MOCHITEST_A11Y_FILES \
  MOCHITEST_METRO_FILES \
  MOCHITEST_ROBOCOP_FILES \
  $(NULL)

$(foreach var,$(FREEZE_VARIABLES),$(eval $(var)_FROZEN := '$($(var))'))

CHECK_FROZEN_VARIABLES = $(foreach var,$(FREEZE_VARIABLES), \
  $(if $(subst $($(var)_FROZEN),,'$($(var))'),$(error Makefile variable '$(var)' changed value after including rules.mk. Was $($(var)_FROZEN), now $($(var)).)))

libs export::
	$(CHECK_FROZEN_VARIABLES)

PURGECACHES_DIRS ?= $(DIST)/bin
ifdef MOZ_WEBAPP_RUNTIME
PURGECACHES_DIRS += $(DIST)/bin/webapprt
endif

PURGECACHES_FILES = $(addsuffix /.purgecaches,$(PURGECACHES_DIRS))

default all:: $(PURGECACHES_FILES)

$(PURGECACHES_FILES):
	if test -d $(@D) ; then touch $@ ; fi

.DEFAULT_GOAL := $(or $(OVERRIDE_DEFAULT_GOAL),default)

#############################################################################
# Derived targets and dependencies

include $(topsrcdir)/config/makefiles/autotargets.mk
ifneq ($(NULL),$(AUTO_DEPS))
  default all libs tools export:: $(AUTO_DEPS)
endif

export:: $(GENERATED_FILES)

GARBAGE += $(GENERATED_FILES)
