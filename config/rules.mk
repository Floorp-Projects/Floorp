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
# Pass 2. install - Create libraries & programs.  Publish them to dist/bin.
#
# `gmake` will build each of these properly, but `gmake -jN` may break
# use `gmake MAKE='gmake -jN'` instead
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
# f)
#	JSRCS	-- java source files to compile into class files
#			(if you don't specify this it will default to *.java)
# g)
#	PACKAGE	-- the package to put the .class files into
#			(e.g. netscape/applet)
# h)
#	JMC_EXPORT -- java files to be exported for use by JMC_GEN
#			(this is a list of Class names)
# i)
#	JRI_GEN	-- files to run through javah to generate headers and stubs
#			(output goes into the _jri sub-dir)
# j)
#	JNI_GEN	-- files to run through javah to generate headers and stubs
#			(output goes into the _jni sub-dir)
# k)
#	JMC_GEN	-- files to run through jmc to generate headers and stubs
#			(output goes into the _jmc sub-dir)
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

ifeq ($(OS_ARCH),OS2)
STATIC_LIBS		= $(filter %_s, $(EXTRA_DSO_LIBS))
SHARED_LIBS		= $(filter-out %_s, $(EXTRA_DSO_LIBS))
EXTRA_DSO_LIBS		:= $(addprefix lib,$(STATIC_LIBS)) $(SHARED_LIBS)
endif

ifeq ($(MOZ_OS2_TOOLS),VACPP)
EXTRA_DSO_LIBS		:= $(addsuffix .$(LIB_SUFFIX),$(addprefix $(DIST)/lib/,$(EXTRA_DSO_LIBS)))
EXTRA_DSO_LIBS		:= $(filter-out %/bin %/lib,$(EXTRA_DSO_LIBS))
EXTRA_DSO_LDOPTS    := $(patsubst -l%,$(DIST)/lib/%.$(LIB_SUFFIX),$(EXTRA_DSO_LDOPTS))
LIBS                := $(patsubst -l%,$(DIST)/lib/lib%.$(LIB_SUFFIX),$(LIBS))
else
EXTRA_DSO_LIBS		:= $(addprefix -l,$(EXTRA_DSO_LIBS))
endif

ifndef LIBRARY
ifdef LIBRARY_NAME
ifeq ($(OS_ARCH),OS2)
ifdef SHORT_LIBNAME
LIBRARY_NAME		:= $(SHORT_LIBNAME)
endif
endif
LIBRARY			:= lib$(LIBRARY_NAME).$(LIB_SUFFIX)
endif
endif

ifndef HOST_LIBRARY
ifdef HOST_LIBRARY_NAME
HOST_LIBRARY		:= lib$(HOST_LIBRARY_NAME).$(LIB_SUFFIX)
endif
endif

ifdef LIBRARY
ifneq (,$(BUILD_SHARED_LIBS)$(FORCE_SHARED_LIB))
ifdef MKSHLIB

ifeq ($(OS_ARCH),OS2)
DEF_OBJS		= $(OBJS)
ifneq ($(EXPORT_OBJS),1)
DEF_OBJS		+= $(SHARED_LIBRARY_LIBS)
endif
SHARED_LIBRARY		:= $(LIBRARY_NAME)$(DLL_SUFFIX)
DEF_FILE		:= $(SHARED_LIBRARY:.dll=.def)
IMPORT_LIBRARY		:= $(SHARED_LIBRARY:.dll=.lib)
else  # OS2


ifeq (,$(filter-out BeOS, $(OS_ARCH)))
# Unix only
ifdef LIB_IS_C_ONLY
MKSHLIB			= $(MKCSHLIB)
endif
endif

SHARED_LIBRARY		:= $(LIBRARY:.$(LIB_SUFFIX)=$(DLL_SUFFIX))
IMPORT_LIBRARY		:= 

endif # OS2
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

ifndef TARGETS
TARGETS			= $(LIBRARY) $(SHARED_LIBRARY) $(PROGRAM) $(SIMPLE_PROGRAMS) $(HOST_LIBRARY) $(HOST_PROGRAM) $(HOST_SIMPLE_PROGRAMS)
endif

ifndef OBJS
_OBJS			= \
	$(JRI_STUB_CFILES) \
	$(addsuffix .$(OBJ_SUFFIX), $(JMC_GEN)) \
	$(CSRCS:.c=.$(OBJ_SUFFIX)) \
	$(CPPSRCS:.cpp=.$(OBJ_SUFFIX)) \
	$(ASFILES:.$(ASM_SUFFIX)=.$(OBJ_SUFFIX))
OBJS	= $(strip $(addprefix $(OBJ_PREFIX),$(_OBJS)))
endif

ifndef HOST_OBJS
HOST_OBJS		= $(HOST_CSRCS:.c=.ho)
endif

ifeq ($(OS_ARCH),OS2)
LIBOBJS			:= $(OBJS)
else
LIBOBJS			:= $(addprefix \", $(OBJS))
LIBOBJS			:= $(addsuffix \", $(LIBOBJS))
endif

ifndef MOZ_AUTO_DEPS
ifneq (,$(OBJS))
MDDEPFILES		= $(addprefix $(MDDEPDIR)/,$(OBJS:.$(OBJ_SUFFIX)=.pp))
endif
endif

ifndef PACKAGE
PACKAGE			= .
endif

ifdef JAVA_OR_NSJVM
ALL_TRASH = \
	$(GARBAGE) $(TARGETS) $(OBJS) $(PROGOBJS) LOGS TAGS a.out \
	$(JDK_HEADER_CFILES) $(JDK_STUB_CFILES) \
	$(JRI_HEADER_CFILES) $(JRI_STUB_CFILES) $(JMC_STUBS) \
	$(JMC_HEADERS) $(JMC_EXPORT_FILES) \
	 so_locations _gen _stubs _jmc _jri \
	$(wildcard gts_tmp_*) \
	$(wildcard $(JAVA_DESTPATH)/$(PACKAGE)/*.class)	
ALL_TRASH_DIRS = \
	$(GARBAGE_DIRS)
else
ALL_TRASH = \
	$(GARBAGE) $(TARGETS) $(OBJS) $(PROGOBJS) LOGS TAGS a.out \
	$(HOST_PROGOBJS) $(HOST_OBJS) $(IMPORT_LIBRARY) $(DEF_FILE)\
	$(EXE_DEF_FILE) so_locations _gen _stubs $(wildcard *.res) \
	$(wildcard gts_tmp_*) $(LIBRARY:%.a=.%.timestamp)
ALL_TRASH_DIRS = \
	$(GARBAGE_DIRS)
endif

ifdef JAVA_OR_NSJVM
ifdef JDIRS
GARBAGE			+= $(addprefix $(JAVA_DESTPATH)/,$(JDIRS))
endif
endif

ifdef QTDIR
GARBAGE			+= $(MOCSRCS)
endif

ifdef SIMPLE_PROGRAMS
GARBAGE			+= $(SIMPLE_PROGRAMS:%=%.$(OBJ_SUFFIX))
endif

ifdef HOST_SIMPLE_PROGRAMS
GARBAGE			+= $(HOST_SIMPLE_PROGRAMS:%=%.ho)
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

ifdef JAVA_OR_NSJVM
JMC_SUBDIR              = _jmc
else
JMC_SUBDIR              = $(LOCAL_JMC_SUBDIR)
endif

JDK_GEN_DIR		= _gen
JMC_GEN_DIR		= $(JMC_SUBDIR)
JRI_GEN_DIR		= _jri
JNI_GEN_DIR		= _jni
JDK_STUB_DIR		= _stubs
XPIDL_GEN_DIR		= _xpidlgen

ifdef MOZ_UPDATE_XTERM
# Its good not to have a newline at the end of the titlebar string because it
# makes the make -s output easier to read.  Echo -n does not work on all
# platforms, but we can trick sed into doing it.
UPDATE_TITLE = sed -e "s!Y!$@ in $(shell $(BUILD_TOOLS)/print-depth-path.sh)/$$d!" $(topsrcdir)/config/xterm.str;
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

ifneq ($(EXPORT_DIRS),)
LOOP_OVER_EXPORT_DIRS = \
    @$(EXIT_ON_ERROR) \
    for d in $(EXPORT_DIRS); do \
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
MAKE_DIRS		=

ifneq (,$(OBJS))
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
# MacOS X specific stuff
#

ifeq ($(OS_ARCH),Darwin)
ifdef IS_COMPONENT
EXTRA_DSO_LDOPTS	+= -bundle
else
EXTRA_DSO_LDOPTS	+= -dynamiclib
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
endif # non-gnu compilers
endif # IS_COMPONENT
endif # HP-UX

ifeq ($(USE_TVFS),1)
IFLAGS1 = -rb
IFLAGS2 = -rb
else
IFLAGS1 = -m 644
IFLAGS2 = -m 755
endif

################################################################################

all:: export install

# Do depend as well
alldep:: export depend install

# Do everything from scratch
everything:: clean alldep

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

#
# JDIRS -- like JSRCS, except you can give a list of directories and it will
# compile all the out-of-date java files in those directories.
#
# NOTE: recursing through these can speed things up, but they also cause
# some builds to run out of memory
#
ifneq ($(JDIRS),)
ifeq ($(JAVA_OR_NSJVM),1)
export:: $(JAVA_DESTPATH) $(JAVA_DESTPATH)/$(PACKAGE)
	@for d in $(JDIRS); do							\
		if test -d $$d; then						\
			$(EXIT_ON_ERROR)					\
			files=`echo $$d/*.java`;				\
			list=`$(PERL) $(topsrcdir)/config/outofdate.pl $(PERLARG)	\
				    -d $(JAVA_DESTPATH)/$(PACKAGE) $$files`;	\
			if test "$${list}x" != "x"; then			\
			    echo Building all java files in $$d;		\
			    echo $(JAVAC) $$list;				\
			    $(JAVAC) $$list;					\
			else				\
				true;			\
			fi;							\
			set +e;							\
		else								\
			echo "Skipping non-directory $$d...";			\
		fi;	                                                        \
	done
endif
endif


ifneq ($(JDIRS),)
ifeq ($(JAVA_OR_NSJVM),1)
export:: $(JAVA_DESTPATH) $(JAVA_DESTPATH)/$(PACKAGE)
	@for d in $(JDIRS); do							\
		if test -d $$d; then						\
			$(EXIT_ON_ERROR)					\
			files=`echo $$d/*.java`;				\
			list=`$(PERL) $(topsrcdir)/config/outofdate.pl $(PERLARG)	\
				    -d $(JAVA_DESTPATH)/$(PACKAGE) $$files`;	\
			if test "$${list}x" != "x"; then			\
			    echo Building all java files in $$d;		\
			    echo $(JAVAC) $$list;				\
			    $(JAVAC) $$list;					\
			else				\
				true;			\
			fi;							\
			set +e;							\
		else	\
			echo "Skipping non-directory $$d...";			\
		fi;								\
	done
endif
endif

# Target to only regenerate makefiles
makefiles: $(SUBMAKEFILES)
ifdef DIRS
	@for d in $(filter-out $(STATIC_MAKEFILES), $(DIRS)); do\
		$(UPDATE_TITLE) 				\
		$(MAKE) -C $$d $@				\
	done
endif

export:: $(SUBMAKEFILES) $(MAKE_DIRS) $(MKDEPEND_BUILTIN)
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
install:: $(SUBMAKEFILES) $(MAKE_DIRS) $(HOST_LIBRARY) $(LIBRARY) $(SHARED_LIBRARY) $(IMPORT_LIBRARY) $(HOST_PROGRAM) $(PROGRAM) $(HOST_SIMPLE_PROGRAMS) $(SIMPLE_PROGRAMS) $(MAPS)
ifneq (,$(BUILD_STATIC_LIBS)$(FORCE_STATIC_LIB))
ifdef LIBRARY
ifeq ($(OS_ARCH),OS2)
	$(INSTALL) $(IFLAGS1) $(LIBRARY) $(DIST)/lib
else
ifdef IS_COMPONENT
	$(INSTALL) $(IFLAGS1) $(LIBRARY) $(DIST)/lib/components
else
	$(INSTALL) $(IFLAGS1) $(LIBRARY) $(DIST)/lib
endif
endif # OS2
endif # LIBRARY
endif # BUILD_STATIC_LIBS || FORCE_STATIC_LIB
ifdef MAPS
	$(INSTALL) $(IFLAGS1) $(MAPS) $(DIST)/bin
endif
ifdef SHARED_LIBRARY
ifdef IS_COMPONENT
ifeq ($(OS_ARCH),OS2)
	$(INSTALL) $(IFLAGS2) $(IMPORT_LIBRARY) $(DIST)/lib/components
else
	$(INSTALL) $(IFLAGS2) $(SHARED_LIBRARY) $(DIST)/lib/components
	$(ELF_DYNSTR_GC) $(DIST)/lib/components/$(SHARED_LIBRARY)
endif
	$(INSTALL) $(IFLAGS2) $(SHARED_LIBRARY) $(DIST)/bin/components
	$(ELF_DYNSTR_GC) $(DIST)/bin/components/$(SHARED_LIBRARY)
ifdef BEOS_ADDON_WORKAROUND
	( cd $(DIST)/bin/components && $(CC) -nostart -o $(SHARED_LIBRARY).stub $(SHARED_LIBRARY) )
endif
else # ! IS_COMPONENT
ifeq ($(OS_ARCH),OS2)
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
	+$(LOOP_OVER_DIRS)

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
	+$(LOOP_OVER_EXPORT_DIRS)

distclean:: $(SUBMAKEFILES)
	+$(LOOP_OVER_DIRS)
	+$(LOOP_OVER_EXPORT_DIRS)
	-rm -rf $(ALL_TRASH_DIRS) 
	-rm -f $(ALL_TRASH)  \
	Makefile .HSancillary \
	$(wildcard *.$(OBJ_SUFFIX)) $(wildcard *.ho) \
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
$(PROGRAM): $(PROGOBJS) $(EXTRA_DEPS) $(EXE_DEF_FILE) Makefile Makefile.in
ifeq ($(MOZ_OS2_TOOLS),VACPP)
	$(LD) -OUT:$@ $(LDFLAGS) $(PROGOBJS) $(LIBS) $(EXTRA_LIBS) $(OS_LIBS) $(EXE_DEF_FILE) /ST:0x100000
else
ifeq ($(CPP_PROG_LINK),1)
	$(CCC) -o $@ $(CXXFLAGS) $(WRAP_MALLOC_CFLAGS) $(PROGOBJS) $(LDFLAGS) $(LIBS_DIR) $(LIBS) $(OS_LIBS) $(EXTRA_LIBS) $(BIN_FLAGS) $(WRAP_MALLOC_LIB) $(PROFILER_LIBS)
	$(MOZ_POST_PROGRAM_COMMAND) $@
else # ! CPP_PROG_LINK
	$(CC) -o $@ $(CFLAGS) $(PROGOBJS) $(LDFLAGS) $(LIBS_DIR) $(LIBS) $(OS_LIBS) $(EXTRA_LIBS) $(BIN_FLAGS)
endif # CPP_PROG_LINK
endif # OS2
ifeq ($(OS_ARCH),BeOS)
ifdef BEOS_PROGRAM_RESOURCE
	xres -o $@ $(BEOS_PROGRAM_RESOURCE)
	mimeset $@
endif
endif # BeOS
ifeq ($(OS_ARCH),OS2)
ifdef OS2_PROGRAM_RESOURCE
	rc -n -x2 -i $(subst /,\,$(srcdir)) -r $(subst /,\,$(OS2_PROGRAM_RESOURCE)) $(patsubst %.rc, %.res, $(notdir $(OS2_PROGRAM_RESOURCE)))
	rc -n -x2 -i $(subst /,\,$(srcdir)) $(patsubst %.rc, %.res, $(notdir $(OS2_PROGRAM_RESOURCE))) $@
endif # os2_prog_rsrc
endif

$(HOST_PROGRAM): $(HOST_PROGOBJS) $(HOST_EXTRA_DEPS) Makefile Makefile.in
	$(HOST_CC) -o $@ $(HOST_CFLAGS) $(HOST_PROGOBJS) $(HOST_LIBS) $(HOST_EXTRA_LIBS)

#
# This is an attempt to support generation of multiple binaries
# in one directory, it assumes everything to compile Foo is in
# Foo.o (from either Foo.c or Foo.cpp).
#
# SIMPLE_PROGRAMS = Foo Bar
# creates Foo.o Bar.o, links with LIBS to create Foo, Bar.
#
$(SIMPLE_PROGRAMS): %$(BIN_SUFFIX): %.$(OBJ_SUFFIX) $(EXTRA_DEPS) Makefile Makefile.in
ifeq ($(CPP_PROG_LINK),1)
ifeq ($(MOZ_OS2_TOOLS),VACPP)
	$(LD) /Out:$@ $< $(LDFLAGS) $(LIBS) $(OS_LIBS) $(EXTRA_LIBS) $(WRAP_MALLOC_LIB) $(PROFILER_LIBS)
else
	$(CCC) $(WRAP_MALLOC_CFLAGS) $(CXXFLAGS) -o $@ $< $(LDFLAGS) $(LIBS_DIR) $(LIBS) $(OS_LIBS) $(EXTRA_LIBS) $(WRAP_MALLOC_LIB) $(PROFILER_LIBS)
endif
	$(MOZ_POST_PROGRAM_COMMAND) $@
else
ifeq ($(MOZ_OS2_TOOLS),VACPP)
	$(LD) /Out:$@ $< $(LDFLAGS) $(LIBS) $(OS_LIBS) $(EXTRA_LIBS) $(WRAP_MALLOC_LIB) $(PROFILER_LIBS)
else
	$(CC) $(WRAP_MALLOC_CFLAGS) $(CFLAGS) -o $@ $< $(LDFLAGS) $(LIBS_DIR) $(LIBS) $(OS_LIBS) $(EXTRA_LIBS) $(WRAP_MALLOC_LIB) $(PROFILER_LIBS)
endif
	$(MOZ_POST_PROGRAM_COMMAND) $@
endif

$(HOST_SIMPLE_PROGRAMS): host_%$(BIN_SUFFIX): %.ho $(HOST_EXTRA_DEPS) Makefile Makefile.in
	$(HOST_CC) -o $@ $(HOST_CFLAGS) $< $(HOST_LIBS) $(HOST_EXTRA_LIBS)

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
	$(INSTALL) $(IFLAGS2) $^.pure $(DIST)/bin

quantify: $(PROGRAM)
ifeq ($(CPP_PROG_LINK),1)
	$(QUANTIFY) $(CCC) -o $^.quantify $(CXXFLAGS) $(PROGOBJS) $(LDFLAGS) $(LIBS_DIR) $(LIBS) $(OS_LIBS) $(EXTRA_LIBS)
else
	$(QUANTIFY) $(CC) -o $^.quantify $(CFLAGS) $(PROGOBJS) $(LDFLAGS) $(LIBS_DIR) $(LIBS) $(OS_LIBS) $(EXTRA_LIBS)
endif
	$(INSTALL) $(IFLAGS2) $^.quantify $(DIST)/bin

ifneq ($(OS_ARCH),OS2)
#
# This allows us to create static versions of the shared libraries
# that are built using other static libraries.  Confused...?
#
ifdef SHARED_LIBRARY_LIBS
ifneq (,$(filter OSF1 BSD_OS FreeBSD NetBSD OpenBSD SunOS Rhapsody,$(OS_ARCH)))
CLEANUP1	:= | egrep -v '(________64ELEL_|__.SYMDEF)'
CLEANUP2	:= rm -f ________64ELEL_ __.SYMDEF
else
CLEANUP2	:= true
endif
SUB_LOBJS	= $(shell for lib in $(SHARED_LIBRARY_LIBS); do $(AR_LIST) $${lib} $(CLEANUP1); done;)
endif

$(LIBRARY): $(OBJS) $(LOBJS) $(SHARED_LIBRARY_LIBS) Makefile Makefile.in
	rm -f $@
ifdef SHARED_LIBRARY_LIBS
	@rm -f $(SUB_LOBJS)
	@for lib in $(SHARED_LIBRARY_LIBS); do $(AR_EXTRACT) $${lib}; $(CLEANUP2); done
endif
	$(AR) $(AR_FLAGS) $(OBJS) $(LOBJS) $(SUB_LOBJS)
	$(RANLIB) $@
	@rm -f foodummyfilefoo $(SUB_LOBJS)

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
endif

$(HOST_LIBRARY): $(HOST_OBJS) Makefile
	rm -f $@
	$(HOST_AR) $(HOST_AR_FLAGS) $@ $(HOST_OBJS)
	$(HOST_RANLIB) $@

ifdef NO_LD_ARCHIVE_FLAGS
SUB_SHLOBJS = $(SUB_LOBJS)
endif

$(SHARED_LIBRARY): $(OBJS) $(LOBJS) $(DEF_FILE) $(SHARED_LIBRARY_LIBS) $(EXTRA_DEPS) Makefile Makefile.in
	rm -f $@
ifneq ($(OS_ARCH),OS2)
ifneq ($(OS_ARCH),OpenVMS)
ifdef NO_LD_ARCHIVE_FLAGS
ifdef SHARED_LIBRARY_LIBS
	@rm -f $(SUB_SHLOBJS)
	@for lib in $(SHARED_LIBRARY_LIBS); do $(AR_EXTRACT) $${lib}; $(CLEANUP2); done
endif # SHARED_LIBRARY_LIBS
endif # NO_LD_ARCHIVE_FLAGS
	$(MKSHLIB) $(SHLIB_LDSTARTFILE) $(OBJS) $(LOBJS) $(SUB_SHLOBJS) $(LDFLAGS) $(EXTRA_DSO_LDOPTS) $(OS_LIBS) $(EXTRA_LIBS) $(DEF_FILE) $(SHLIB_LDENDFILE)
	@rm -f foodummyfilefoo $(SUB_SHLOBJS)
else
	@touch no-such-file.vms; rm -f no-such-file.vms $(SUB_LOBJS)
ifndef IS_COMPONENT
	@if test ! -f VMSuni.opt; then \
	    echo "Creating universal symbol option file VMSuni.opt"; \
	    for lib in $(SHARED_LIBRARY_LIBS); do $(AR_EXTRACT) $${lib}; $(CLEANUP2); done; \
	    create_opt_uni $(OBJS) $(SUB_LOBJS); \
	fi
	@touch no-such-file.vms; rm -f no-such-file.vms $(SUB_LOBJS)
endif
	$(MKSHLIB) -o $@ $(OBJS) $(LOBJS) $(EXTRA_DSO_LDOPTS) VMSuni.opt;
endif
else # OS2
ifeq ($(MOZ_OS2_TOOLS),VACPP)
	$(MKSHLIB) /O:$@ /DLL /INC:_dllentry $(LDFLAGS) $(OBJS) $(LOBJS) $(EXTRA_DSO_LDOPTS) $(OS_LIBS) $(EXTRA_LIBS) $(DEF_FILE)
else
	$(MKSHLIB) -o $@ $(OBJS) $(LOBJS) $(EXTRA_DSO_LDOPTS) $(OS_LIBS) $(EXTRA_LIBS) $(DEF_FILE)
endif
endif # OS2
	chmod +x $@
	$(MOZ_POST_DSO_LIB_COMMAND) $@

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
_MDDEPFILE = $(MDDEPDIR)/$(<F).pp

define MAKE_DEPS_AUTO
if test -d $(@D); then \
	set -e ; \
	touch $(_MDDEPFILE) && \
	$(MKDEPEND) -o'.$(OBJ_SUFFIX)' -f$(_MDDEPFILE) $(DEFINES) $(ACDEFINES) $(INCLUDES) $< >/dev/null 2>&1 && \
	mv $(_MDDEPFILE) $(_MDDEPFILE).old && \
	cat $(_MDDEPFILE).old | sed -e "s|^$(<D)/||g" > $(_MDDEPFILE) && rm -f $(_MDDEPFILE).old ; \
	echo "Building deps for $<"; \
fi
endef

endif # !COMPILER_DEPEND

endif # MOZ_AUTO_DEPS

%: %.c Makefile.in
	$(REPORT_BUILD)
	@$(MAKE_DEPS_AUTO)
ifeq ($(MOZ_OS2_TOOLS), VACPP)
	$(ELOG) $(CC) -Fo$@ -c $(CFLAGS) $<
else
	$(ELOG) $(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<
endif

$(OBJ_PREFIX)%.$(OBJ_SUFFIX): %.c Makefile.in
	$(REPORT_BUILD)
	@$(MAKE_DEPS_AUTO)
ifeq ($(MOZ_OS2_TOOLS),VACPP)
	$(ELOG) $(CC) -Fo$@ -c $(COMPILE_CFLAGS) $<
else
	$(ELOG) $(CC) -o $@ -c $(COMPILE_CFLAGS) $<
endif

$(OBJ_PREFIX)%.ho: %.c Makefile.in
	$(REPORT_BUILD)
	$(ELOG) $(HOST_CC) -o $@ -c $(HOST_CFLAGS) -I$(DIST)/include $(NSPR_CFLAGS) $<

moc_%.cpp: %.h Makefile.in
	$(MOC) $< -o $@ 

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
	$(CCC) -o $@ $(CXXFLAGS) $< $(LDFLAGS)

#
# Please keep the next two rules in sync.
#
$(OBJ_PREFIX)%.$(OBJ_SUFFIX): %.cc Makefile.in
	$(REPORT_BUILD)
	@$(MAKE_DEPS_AUTO)
	$(ELOG) $(CCC) -o $@ -c $(COMPILE_CXXFLAGS) $<

$(OBJ_PREFIX)%.$(OBJ_SUFFIX): %.cpp Makefile.in
	$(REPORT_BUILD)
	@$(MAKE_DEPS_AUTO)
ifdef STRICT_CPLUSPLUS_SUFFIX
	echo "#line 1 \"$*.cpp\"" | cat - $*.cpp > t_$*.cc
	$(ELOG) $(CCC) -o $@ -c $(COMPILE_CXXFLAGS) t_$*.cc
	rm -f t_$*.cc
else
ifeq ($(MOZ_OS2_TOOLS), VACPP)
	$(ELOG) $(CCC) -Fo$@ -c $(COMPILE_CXXFLAGS) $<
else
	$(ELOG) $(CCC) -o $@ -c $(COMPILE_CXXFLAGS) $<
endif
endif #STRICT_CPLUSPLUS_SUFFIX

%.i: %.cpp
	$(CCC) -C -E $(COMPILE_CXXFLAGS) $< > $*.i

%.i: %.c
	$(CC) -C -E $(COMPILE_CFLAGS) $< > $*.i

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
Makefile: Makefile.in $(topsrcdir)/configure
	@$(PERL) $(AUTOCONF_TOOLS)/make-makefile -t $(topsrcdir) -d $(DEPTH) 

ifdef SUBMAKEFILES
# VPATH does not work on some machines in this case, so add $(srcdir)
$(SUBMAKEFILES): % : $(srcdir)/%.in
	@$(PERL) $(AUTOCONF_TOOLS)/make-makefile -t $(topsrcdir) -d $(DEPTH) $@
endif

ifdef AUTOUPDATE_CONFIGURE
$(topsrcdir)/configure: $(topsrcdir)/configure.in
	(cd $(topsrcdir) && $(AUTOCONF)) && (cd $(DEPTH) && ./config.status --recheck)
endif

###############################################################################
# Bunch of things that extend the 'export' rule (in order):
###############################################################################

$(JAVA_DESTPATH) $(JAVA_DESTPATH)/$(PACKAGE) $(JMCSRCDIR)::
	@if test ! -d $@; then		\
		echo Creating $@;	\
		rm -rf $@;		\
		$(NSINSTALL) -D $@;	\
	else				\
		true;			\
	fi

################################################################################
### JSRCS -- for compiling java files

ifneq ($(JSRCS),)
ifdef JAVA_OR_NSJVM
export:: $(JAVA_DESTPATH) $(JAVA_DESTPATH)/$(PACKAGE)
	list=`$(PERL) $(topsrcdir)/config/outofdate.pl $(PERLARG)	\
		    -d $(JAVA_DESTPATH)/$(PACKAGE) $(JSRCS)`;	\
	if test "$$list"x != "x"; then				\
	    echo $(JAVAC) $$list;				\
	    $(JAVAC) $$list;					\
	else				\
		true;			\
	fi

all:: export

clean clobber::
	rm -f $(DIST)/classes/$(PACKAGE)/*.class

endif
endif

#
# JDK_GEN -- for generating "old style" native methods
#
# Generate JDK Headers and Stubs into the '_gen' and '_stubs' directory
#
ifneq ($(JDK_GEN),)
ifdef JAVA_OR_NSJVM
INCLUDES		+= -I$(JDK_GEN_DIR)
JDK_PACKAGE_CLASSES	= $(JDK_GEN)
JDK_PATH_CLASSES	= $(subst .,/,$(JDK_PACKAGE_CLASSES))
JDK_HEADER_CLASSFILES	= $(patsubst %,$(JAVA_DESTPATH)/%.class,$(JDK_PATH_CLASSES))
JDK_STUB_CLASSFILES	= $(patsubst %,$(JAVA_DESTPATH)/%.class,$(JDK_PATH_CLASSES))
JDK_HEADER_CFILES	= $(patsubst %,$(JDK_GEN_DIR)/%.h,$(JDK_GEN))
JDK_STUB_CFILES		= $(patsubst %,$(JDK_STUB_DIR)/%.c,$(JDK_GEN))

$(JDK_HEADER_CFILES): $(JDK_HEADER_CLASSFILES)
$(JDK_STUB_CFILES): $(JDK_STUB_CLASSFILES)

export::
	@echo Generating/Updating JDK headers
	$(JAVAH) -d $(JDK_GEN_DIR) $(JDK_PACKAGE_CLASSES)
	@echo Generating/Updating JDK stubs
	$(JAVAH) -stubs -d $(JDK_STUB_DIR) $(JDK_PACKAGE_CLASSES)
ifdef MOZ_GENMAC
	@if test ! -d $(DEPTH)/lib/mac/Java/; then						\
		echo "!!! You need to have a ns/lib/mac/Java directory checked out.";		\
		echo "!!! This allows us to automatically update generated files for the mac.";	\
		echo "!!! If you see any modified files there, please check them in.";		\
	else				\
		true;			\
	fi
	@echo Generating/Updating JDK headers for the Mac
	$(JAVAH) -mac -d $(DEPTH)/lib/mac/Java/_gen $(JDK_PACKAGE_CLASSES)
	@echo Generating/Updating JDK stubs for the Mac
	$(JAVAH) -mac -stubs -d $(DEPTH)/lib/mac/Java/_stubs $(JDK_PACKAGE_CLASSES)
endif
endif # JAVA_OR_NSJVM
endif

#
# JRI_GEN -- for generating JRI native methods
#
# Generate JRI Headers and Stubs into the 'jri' directory
#
ifneq ($(JRI_GEN),)
ifdef JAVA_OR_NSJVM
INCLUDES		+= -I$(JRI_GEN_DIR)
JRI_PACKAGE_CLASSES	= $(JRI_GEN)
JRI_PATH_CLASSES	= $(subst .,/,$(JRI_PACKAGE_CLASSES))
JRI_HEADER_CLASSFILES	= $(patsubst %,$(JAVA_DESTPATH)/%.class,$(JRI_PATH_CLASSES))
JRI_STUB_CLASSFILES	= $(patsubst %,$(JAVA_DESTPATH)/%.class,$(JRI_PATH_CLASSES))
JRI_HEADER_CFILES	= $(patsubst %,$(JRI_GEN_DIR)/%.h,$(JRI_GEN))
JRI_STUB_CFILES		= $(patsubst %,$(JRI_GEN_DIR)/%.c,$(JRI_GEN))

$(JRI_HEADER_CFILES): $(JRI_HEADER_CLASSFILES)
$(JRI_STUB_CFILES): $(JRI_STUB_CLASSFILES)

export::
	@echo Generating/Updating JRI headers
	$(JAVAH) -jri -d $(JRI_GEN_DIR) $(JRI_PACKAGE_CLASSES)
	@echo Generating/Updating JRI stubs
	$(JAVAH) -jri -stubs -d $(JRI_GEN_DIR) $(JRI_PACKAGE_CLASSES)
ifdef MOZ_GENMAC
	@if test ! -d $(DEPTH)/lib/mac/Java/; then						\
		echo "!!! You need to have a ns/lib/mac/Java directory checked out.";		\
		echo "!!! This allows us to automatically update generated files for the mac.";	\
		echo "!!! If you see any modified files there, please check them in.";		\
	else				\
		true;			\
	fi
	@echo Generating/Updating JRI headers for the Mac
	$(JAVAH) -jri -mac -d $(DEPTH)/lib/mac/Java/_jri $(JRI_PACKAGE_CLASSES)
	@echo Generating/Updating JRI stubs for the Mac
	$(JAVAH) -jri -mac -stubs -d $(DEPTH)/lib/mac/Java/_jri $(JRI_PACKAGE_CLASSES)
endif
endif # JAVA_OR_NSJVM
endif



#
# JNI_GEN -- for generating JNI native methods
#
# Generate JNI Headers and Stubs into the 'jni' directory
#
ifneq ($(JNI_GEN),)
ifdef JAVA_OR_NSJVM
INCLUDES		+= -I$(JNI_GEN_DIR)
JNI_PACKAGE_CLASSES	= $(JNI_GEN)
JNI_PATH_CLASSES	= $(subst .,/,$(JNI_PACKAGE_CLASSES))
JNI_HEADER_CLASSFILES	= $(patsubst %,$(JAVA_DESTPATH)/%.class,$(JNI_PATH_CLASSES))
JNI_HEADER_CFILES	= $(patsubst %,$(JNI_GEN_DIR)/%.h,$(JNI_GEN))
JNI_STUB_CFILES		= $(patsubst %,$(JNI_GEN_DIR)/%.c,$(JNI_GEN))

$(JNI_HEADER_CFILES): $(JNI_HEADER_CLASSFILES)

export::
	@echo Generating/Updating JNI headers
	$(JAVAH) -jni -d $(JNI_GEN_DIR) $(JNI_PACKAGE_CLASSES)
ifdef MOZ_GENMAC
	@if test ! -d $(DEPTH)/lib/mac/Java/; then						\
		echo "!!! You need to have a ns/lib/mac/Java directory checked out.";		\
		echo "!!! This allows us to automatically update generated files for the mac.";	\
		echo "!!! If you see any modified files there, please check them in.";		\
	else				\
		true;			\
	fi
	@echo Generating/Updating JNI headers for the Mac
	$(JAVAH) -jni -mac -d $(DEPTH)/lib/mac/Java/_jni $(JNI_PACKAGE_CLASSES)
endif
endif # JAVA_OR_NSJVM
endif # JNI_GEN



#
# JMC_EXPORT -- for declaring which java classes are to be exported for jmc
#
ifneq ($(JMC_EXPORT),)
ifdef JAVA_OR_NSJVM
JMC_EXPORT_PATHS	= $(subst .,/,$(JMC_EXPORT))
JMC_EXPORT_FILES	= $(patsubst %,$(JAVA_DESTPATH)/$(PACKAGE)/%.class,$(JMC_EXPORT_PATHS))

#
# We're doing NSINSTALL -t here (copy mode) because calling INSTALL will pick up
# your NSDISTMODE and make links relative to the current directory. This is a
# problem because the source isn't in the current directory:
#
export:: $(JMC_EXPORT_FILES) $(JMCSRCDIR)
	$(NSINSTALL) -t $(IFLAGS1) $(JMC_EXPORT_FILES) $(JMCSRCDIR)
endif # JAVA_OR_NSJVM
endif

#
# JMC_GEN -- for generating java modules
#
# Provide default export & install rules when using JMC_GEN
#
ifneq ($(JMC_GEN),)
INCLUDES		+= -I$(JMC_GEN_DIR) -I.
ifdef JAVA_OR_NSJVM
JMC_HEADERS		= $(patsubst %,$(JMC_GEN_DIR)/%.h,$(JMC_GEN))
JMC_STUBS		= $(patsubst %,$(JMC_GEN_DIR)/%.c,$(JMC_GEN))
JMC_OBJS		= $(patsubst %,%.$(OBJ_SUFFIX),$(JMC_GEN))

$(JMC_GEN_DIR)/M%.h: $(JMCSRCDIR)/%.class
	$(JMC) -d $(JMC_GEN_DIR) -interface $(JMC_GEN_FLAGS) $(?F:.class=)

$(JMC_GEN_DIR)/M%.c: $(JMCSRCDIR)/%.class
	$(JMC) -d $(JMC_GEN_DIR) -module $(JMC_GEN_FLAGS) $(?F:.class=)

M%.$(OBJ_SUFFIX): $(JMC_GEN_DIR)/M%.h $(JMC_GEN_DIR)/M%.c
ifeq ($(OS_ARCH),OS2)
	$(CC) -Fo$@ -c $(CFLAGS) $(JMC_GEN_DIR)/M$*.c
else
	$(CC) -o $@ -c $(CFLAGS) $(JMC_GEN_DIR)/M$*.c
endif

export:: $(JMC_HEADERS) $(JMC_STUBS)
endif # JAVA_OR_NSJVM
endif

################################################################################
# Copy each element of EXPORTS to $(PUBLIC)

ifneq ($(EXPORTS)$(XPIDLSRCS),)
$(PUBLIC)::
	@if test ! -d $@; then echo Creating $@; rm -rf $@; $(NSINSTALL) -D $@; else true; fi
endif

ifneq ($(EXPORTS),)
export:: $(EXPORTS) $(PUBLIC)
	$(INSTALL) $(IFLAGS1) $^
	$(PERL) -I$(MOZILLA_DIR)/config $(MOZILLA_DIR)/config/build-list.pl $(PUBLIC)/.headerlist $(notdir $(filter-out $(PUBLIC),$^))
endif 

################################################################################
# Copy each element of PREF_JS_EXPORTS to $(DIST)/bin/defaults/pref

ifneq ($(PREF_JS_EXPORTS),)
$(DIST)/bin/defaults/pref::
	@if test ! -d $@; then echo Creating $@; rm -rf $@; $(NSINSTALL) -D $@; else true; fi

export:: $(PREF_JS_EXPORTS) $(DIST)/bin/defaults/pref
	$(INSTALL) $(IFLAGS1) $^
endif 

################################################################################
# Export the elements of $(XPIDLSRCS), generating .h and .xpt files and
# moving them to the appropriate places.

ifneq ($(XPIDLSRCS),)

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

# export .idl files to $(DIST)/idl
$(DIST)/idl::
	@if test ! -d $@; then echo Creating $@; rm -rf $@; $(NSINSTALL) -D $@; else true; fi

export:: $(XPIDLSRCS) $(DIST)/idl
	$(INSTALL) $(IFLAGS1) $^

# generate .h files from into $(XPIDL_GEN_DIR), then export to $(PUBLIC);
# warn against overriding existing .h file. 
$(XPIDL_GEN_DIR)/.done:
	@if test ! -d $(XPIDL_GEN_DIR); then echo Creating $(XPIDL_GEN_DIR)/.done; rm -rf $(XPIDL_GEN_DIR); mkdir $(XPIDL_GEN_DIR); fi
	@touch $@

# don't depend on $(XPIDL_GEN_DIR), because the modification date changes
# with any addition to the directory, regenerating all .h files -> everything.

$(XPIDL_GEN_DIR)/%.h: %.idl $(XPIDL_COMPILE) $(XPIDL_GEN_DIR)/.done
	$(REPORT_BUILD)
	$(ELOG) $(XPIDL_COMPILE) -m header -w -I $(DIST)/idl -I$(srcdir) -o $(XPIDL_GEN_DIR)/$* $<
	@if test -n "$(findstring $*.h, $(EXPORTS))"; \
	  then echo "*** WARNING: file $*.h generated from $*.idl overrides $(srcdir)/$*.h"; else true; fi

export:: $(patsubst %.idl,$(XPIDL_GEN_DIR)/%.h, $(XPIDLSRCS)) $(PUBLIC)
	$(INSTALL) $(IFLAGS1) $^
	$(PERL) -I$(MOZILLA_DIR)/config $(MOZILLA_DIR)/config/build-list.pl $(PUBLIC)/.headerlist $(notdir $(filter-out $(PUBLIC),$^))

ifndef NO_GEN_XPT
# generate intermediate .xpt files into $(XPIDL_GEN_DIR), then link
# into $(XPIDL_MODULE).xpt and export it to $(DIST)/bin/components.
$(XPIDL_GEN_DIR)/%.xpt: %.idl $(XPIDL_COMPILE)
	$(REPORT_BUILD)
	$(ELOG) $(XPIDL_COMPILE) -m typelib -w -I $(DIST)/idl -I$(srcdir) -o $(XPIDL_GEN_DIR)/$* $<

$(XPIDL_GEN_DIR)/$(XPIDL_MODULE).xpt: $(patsubst %.idl,$(XPIDL_GEN_DIR)/%.xpt,$(XPIDLSRCS))
	$(XPIDL_LINK) $(XPIDL_GEN_DIR)/$(XPIDL_MODULE).xpt $^

install:: $(XPIDL_GEN_DIR)/$(XPIDL_MODULE).xpt
	$(INSTALL) $(IFLAGS1) $(XPIDL_GEN_DIR)/$(XPIDL_MODULE).xpt $(DIST)/bin/components

endif

GARBAGE_DIRS		+= $(XPIDL_GEN_DIR)
endif

################################################################################
# CHROME PACKAGING

JAR_MANIFEST := $(srcdir)/jar.mn

ifeq (flat,$(MOZ_CHROME_FILE_FORMAT))
_JAR_REGCHROME_DISABLE_JAR=1
else
_JAR_REGCHROME_DISABLE_JAR=0
endif

install:: $(CHROME_DEPS)
	@if test -f $(JAR_MANIFEST); then $(PERL) -I$(MOZILLA_DIR)/config $(MOZILLA_DIR)/config/make-jars.pl -f $(MOZ_CHROME_FILE_FORMAT) -d $(DIST)/bin/chrome -s $(srcdir) < $(JAR_MANIFEST); fi

REGCHROME = $(PERL) -I$(MOZILLA_DIR)/config $(MOZILLA_DIR)/config/add-chrome.pl $(DIST)/bin/chrome/installed-chrome.txt $(_JAR_REGCHROME_DISABLE_JAR)

##############################################################################

ifndef NO_MDUPDATE
ifneq (,$(filter-out OS2 WINNT,$(OS_ARCH)))
-include $(DEPENDENCIES)
# Can't use sed because of its 4000-char line length limit, so resort to perl
.DEFAULT:
	@$(PERL) -e '                                                         \
	    open(MD, "< $(DEPENDENCIES)");                                    \
	    while (<MD>) {                                                    \
		if (m@ \.*/*$< @) {                                           \
		    $$found = 1;                                              \
		    last;                                                     \
		}                                                             \
	    }                                                                 \
	    if ($$found) {                                                    \
		print "Removing stale dependency $< from $(DEPENDENCIES)\n";  \
		seek(MD, 0, 0);                                               \
		$$tmpname = "fix.md" . $$$$;                                  \
		open(TMD, "> " . $$tmpname);                                  \
		while (<MD>) {                                                \
		    s@ \.*/*$< @ @;                                           \
		    if (!print TMD "$$_") {                                   \
			unlink(($$tmpname));                                  \
			exit(1);                                              \
		    }                                                         \
		}                                                             \
		close(TMD);                                                   \
		if (!rename($$tmpname, "$(DEPENDENCIES)")) {                  \
		    unlink(($$tmpname));                                      \
		}                                                             \
	    } elsif ("$<" ne "$(DEPENDENCIES)") {                             \
		print "$(MAKE): *** No rule to make target $<.  Stop.\n";     \
		exit(1);                                                      \
	    }'
endif
endif

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

ifndef MOZ_NATIVE_MAKEDEPEND
$(MKDEPEND_BUILTIN):
	$(MAKE) -C $(DEPTH)/config nsinstall$(BIN_SUFFIX)
	$(MAKE) -C $(MKDEPEND_DIR) mkdepend
endif

ifndef MOZ_AUTO_DEPS

define MAKE_DEPS_NOAUTO
	set -e ; \
	touch $@ && \
	$(MKDEPEND) -o'.$(OBJ_SUFFIX)' -f$@ $(DEFINES) $(ACDEFINES) $(INCLUDES) $< >/dev/null 2>&1 && \
	mv $@ $@.old && cat $@.old | sed "s|^$(<D)/||g" > $@ && rm -f $@.old
endef

$(MDDEPDIR)/%.pp: %.c
	$(MAKE_DEPS_NOAUTO)

$(MDDEPDIR)/%.pp: %.cpp
	$(MAKE_DEPS_NOAUTO)

$(MDDEPDIR)/%.pp: %.s
	$(MAKE_DEPS_NOAUTO)

ifneq (,$(OBJS))
depend:: $(SUBMAKEFILES) $(MAKE_DIRS) $(MKDEPEND_BUILTIN) $(MDDEPFILES)
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

ifneq (,$(OBJS))
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
	@$(PERL) $(BUILD_TOOLS)/mddepend.pl $@ $(MDDEPEND_FILES) 
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
.SUFFIXES: .out .a .ln .o .ho .c .cc .C .cpp .y .l .s .S .h .sh .i .pl .class .java .html .pp .mk .in .$(OBJ_SUFFIX)

#
# Don't delete these files if we get killed.
#
.PRECIOUS: .java $(JDK_HEADERS) $(JDK_STUBS) $(JRI_HEADERS) $(JRI_STUBS) $(JMC_HEADERS) $(JMC_STUBS)

#
# Fake targets.  Always run these rules, even if a file/directory with that
# name already exists.
#
.PHONY: all all_platforms alltags boot checkout clean clobber clobber_all export install libs makefiles realclean run_viewer run_apprunner $(DIRS) FORCE

# Used as a dependency to force targets to rebuild
FORCE:

# Delete target if error occurs when building target
.DELETE_ON_ERROR:

tags: TAGS

TAGS: $(SUBMAKEFILES) $(CSRCS) $(CPPSRCS) $(wildcard *.h)
	-etags $(CSRCS) $(CPPSRCS) $(wildcard *.h)
	+$(LOOP_OVER_DIRS)

envirocheck::
	@echo -----------------------------------
	@echo "Enviro-Check (tm)"
	@echo -----------------------------------
	@echo "MOZILLA_CLIENT = $(MOZILLA_CLIENT)"
	@echo "NO_MDUPDATE    = $(NO_MDUPDATE)"
	@echo "BUILD_OPT      = $(BUILD_OPT)"
	@echo "MOZ_LITE       = $(MOZ_LITE)"
	@echo "MOZ_MEDIUM     = $(MOZ_MEDIUM)"
	@echo -----------------------------------

echo-dirs:
	@echo $(DIRS)

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
