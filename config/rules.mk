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

################################################################################
#
# We now use a 3-pass build system.  This needs to be re-thought....
#
# Pass 1. export  - Create generated headers and stubs.  Publish public headers
#                   to dist/<arch>/include.
#
# Pass 2. libs    - Create libraries.  Publish libraries to dist/<arch>/lib.
#
# Pass 3. install - Create programs.  Publish them to dist/<arch>/bin.
#
# 'gmake' will build each of these properly, but 'gmake -jN' will break (need
# to do each pass explicitly when using -j).
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
#			($OBJDIR automatically prepended to it)
# d2)
#	SIMPLE_PROGRAMS	-- Compiles Foo.cpp Bar.cpp into Foo, Bar executables.
#			($OBJDIR automatically prepended to it)
# e)
#	LIBRARY_NAME	-- the target library name to create from $OBJS
#			($OBJDIR automatically prepended to it)
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
topsrcdir = $(DEPTH)
endif

#
# Common rules used by lots of makefiles...
#
ifndef NS_CONFIG_MK
include $(topsrcdir)/config/config.mk
endif

ifdef PROGRAM
PROGRAM			:= $(addprefix $(OBJDIR)/, $(PROGRAM))
endif

ifdef SIMPLE_PROGRAMS
SIMPLE_PROGRAMS := $(addprefix $(OBJDIR)/, $(SIMPLE_PROGRAMS))
endif

#
# Library rules
#
# If NO_STATIC_LIB is set, the static library will not be built.
# If NO_SHARED_LIB is set, the shared library will not be built.
#
ifndef LIBRARY
ifdef LIBRARY_NAME
LIBRARY			:= lib$(LIBRARY_NAME).$(LIB_SUFFIX)
endif # LIBRARY_NAME
endif # LIBRARY

ifdef LIBRARY
ifeq ($(OS_ARCH),OS2)
ifndef DEF_FILE
DEF_FILE		:= $(LIBRARY:.lib=.def)
endif # DEF_FILE
endif # LIBRARY

LIBRARY			:= $(addprefix $(OBJDIR)/, $(LIBRARY))

ifndef NO_SHARED_LIB
ifdef MKSHLIB
ifeq ($(OS_ARCH),OS2)
SHARED_LIBRARY		:= $(LIBRARY:.lib=.dll)
MAPS			:= $(LIBRARY:.lib=.map)
else  # OS2
ifeq ($(OS_ARCH),WINNT)
SHARED_LIBRARY		:= $(LIBRARY:.lib=.dll)
else  # WINNT
ifeq ($(OS_ARCH),HP-UX)
SHARED_LIBRARY		:= $(LIBRARY:.a=.sl)
else  # HPUX
ifeq ($(OS_ARCH)$(OS_RELEASE),SunOS4.1)
SHARED_LIBRARY		:= $(LIBRARY:.a=.so.1.0)
else  # SunOS4
ifeq ($(OS_ARCH)$(OS_RELEASE),AIX4.1)
SHARED_LIBRARY		:= $(LIBRARY:.a=)_shr.a
else  # AIX
SHARED_LIBRARY		:= $(LIBRARY:.a=.$(DLL_SUFFIX))
endif # AIX
endif # SunOS4
endif # HPUX
endif # WINNT
endif # OS2
endif # MKSHLIB
endif # !NO_SHARED_LIB
endif

ifdef NO_STATIC_LIB
LIBRARY			= $(NULL)
endif

ifdef NO_SHARED_LIB
DLL_SUFFIX		= a
endif

ifndef TARGETS
TARGETS			= $(LIBRARY) $(SHARED_LIBRARY) $(PROGRAM) $(SIMPLE_PROGRAMS)
endif

ifndef OBJS
OBJS			= $(JRI_STUB_CFILES) $(addsuffix .o, $(JMC_GEN)) $(CSRCS:.c=.o) $(CPPSRCS:.cpp=.o) $(ASFILES:.s=.o)
endif

OBJS			:= $(addprefix $(OBJDIR)/, $(OBJS))

ifneq (,$(filter OS2 WINNT,$(OS_ARCH)))
ifdef DLL
DLL			:= $(addprefix $(OBJDIR)/, $(DLL))
ifeq ($(OS_ARCH),WINNT)
LIB			:= $(addprefix $(OBJDIR)/, $(LIB))
endif # WINNT
endif # DLL
endif # OS2, WINNT

ifndef OS2_IMPLIB
LIBOBJS			:= $(addprefix \", $(OBJS))
LIBOBJS			:= $(addsuffix \", $(LIBOBJS))
endif

ifndef PACKAGE
PACKAGE			= .
endif

ifdef JAVA_OR_NSJVM
ALL_TRASH		= $(TARGETS) $(OBJS) LOGS TAGS $(GARBAGE) \
			  $(NOSUCHFILE) $(JDK_HEADER_CFILES) $(JDK_STUB_CFILES) \
			  $(JRI_HEADER_CFILES) $(JRI_STUB_CFILES) $(JMC_STUBS) \
			  $(JMC_HEADERS) $(JMC_EXPORT_FILES) so_locations \
			  _gen _jmc _jri _stubs $(MDDEPDIR) $(wildcard gts_tmp_*) \
			  $(wildcard $(JAVA_DESTPATH)/$(PACKAGE)/*.class)
else
ALL_TRASH		= $(TARGETS) $(OBJS) LOGS TAGS $(GARBAGE) \
			  $(NOSUCHFILE) \
			  $(JMC_STUBS) \
			  so_locations \
			  _gen _stubs $(MDDEPDIR) $(wildcard gts_tmp_*)
endif

ifndef USE_AUTOCONF
ALL_TRASH		+= $(OBJDIR)
endif

ifdef JAVA_OR_NSJVM
ifdef JDIRS
ALL_TRASH		+= $(addprefix $(JAVA_DESTPATH)/,$(JDIRS))
endif
endif

ifdef JAVA_OR_NSJVM
JMC_SUBDIR              = _jmc
else
JMC_SUBDIR              = $(LOCAL_JMC_SUBDIR)
endif

ifdef NSBUILDROOT
JDK_GEN_DIR		= $(XPDIST)/_gen
JMC_GEN_DIR		= $(XPDIST)/$(JMC_SUBDIR)
JRI_GEN_DIR		= $(XPDIST)/_jri
JNI_GEN_DIR		= $(XPDIST)/_jni
JDK_STUB_DIR		= $(XPDIST)/_stubs
else
JDK_GEN_DIR		= _gen
JMC_GEN_DIR		= $(JMC_SUBDIR)
JRI_GEN_DIR		= _jri
JNI_GEN_DIR		= _jni
JDK_STUB_DIR		= _stubs
endif

#
# If this is an "official" build, try to build everything.
# I.e., don't exit on errors.
#
ifdef BUILD_OFFICIAL
EXIT_ON_ERROR		= +e
CLICK_STOPWATCH		= date
else
EXIT_ON_ERROR		= -e
CLICK_STOPWATCH		= true
endif

ifdef MOZ_UPDATE_XTERM
UPDATE_TITLE		= echo "]2;gmake: $@ in $(SRCDIR)/$$d"
else
UPDATE_TITLE		= true
endif

ifdef DIRS
LOOP_OVER_DIRS		=					\
	@for d in $(DIRS); do					\
		$(UPDATE_TITLE);                    \
		if test -d $$d; then				\
			set $(EXIT_ON_ERROR);			\
			echo "cd $$d; $(MAKE) $@";		\
			oldDir=`pwd`;				\
			cd $$d; $(MAKE) $@; cd $$oldDir;	\
			set +e;					\
		else						\
			echo "Skipping non-directory $$d...";	\
		fi;						\
		$(CLICK_STOPWATCH);				\
	done
endif

#
# Now we can differentiate between objects used to build a library, and
# objects used to build an executable in the same directory.
#
ifndef PROGOBJS
PROGOBJS		= $(OBJS)
endif

################################################################################

all:: export libs install

# Do depend as well
alldep:: export depend libs install

# Do everything from scratch
everything:: clobber_all alldep

#
# Rules to make OBJDIR and MDDEPDIR (for --enable-md).
# These rules replace the MAKE_OBJDIR and MAKE_DEPDIR macros.
# The macros often failed with parallel builds (-jN),
# because two processes would simultaneously try to make the same  directory.
# Using these rules insures that 'make' will have only one process
# create the directories.
#
MAKE_DIRS =

ifdef MDDEPDIR
$(MDDEPDIR):
	@if test ! -d $@; then echo Creating $@; rm -rf $@; mkdir $@; else true; fi

ifdef OBJS
MAKE_DIRS += $(MDDEPDIR)
endif
endif

ifneq "$(OBJDIR)" "."
$(OBJDIR):
	@if test ! -d $@; then echo Creating $@; rm -rf $@; $(NSINSTALL) -D $@; else true; fi

MAKE_DIRS += $(OBJDIR)
endif

ifdef ALL_PLATFORMS
all_platforms:: $(NFSPWD)
	@d=`$(NFSPWD)`;							\
	if test ! -d LOGS; then rm -rf LOGS; mkdir LOGS; else true; fi;		\
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


export:: $(MAKE_DIRS)
	+$(LOOP_OVER_DIRS)

ifndef LIBS_NEQ_INSTALL
libs install:: $(MAKE_DIRS) $(LIBRARY) $(SHARED_LIBRARY) $(PROGRAM) $(SIMPLE_PROGRAMS) $(MAPS)
ifndef NO_STATIC_LIB
ifdef LIBRARY
	$(INSTALL) -m 444 $(LIBRARY) $(DIST)/lib
endif
endif
ifdef MAPS
	$(INSTALL) -m 444 $(MAPS) $(DIST)/bin
endif
ifdef SHARED_LIBRARY
ifdef IS_COMPONENT
	$(INSTALL) -m 555 $(SHARED_LIBRARY) $(DIST)/lib/components
	$(INSTALL) -m 555 $(SHARED_LIBRARY) $(DIST)/bin/components
else
	$(INSTALL) -m 555 $(SHARED_LIBRARY) $(DIST)/lib
	$(INSTALL) -m 555 $(SHARED_LIBRARY) $(DIST)/bin
endif
endif
ifdef PROGRAM
	$(INSTALL) -m 555 $(PROGRAM) $(DIST)/bin
endif
ifdef SIMPLE_PROGRAMS
	$(INSTALL) -m 555 $(SIMPLE_PROGRAMS) $(DIST)/bin
endif
	+$(LOOP_OVER_DIRS)
else
libs:: $(MAKE_DIRS) $(LIBRARY) $(SHARED_LIBRARY) $(SHARED_LIBRARY_LIBS)
ifndef NO_STATIC_LIB
ifdef LIBRARY
	$(INSTALL) -m 444 $(LIBRARY) $(DIST)/lib
endif
endif
ifdef SHARED_LIBRARY
ifdef IS_COMPONENT
	$(INSTALL) -m 555 $(SHARED_LIBRARY) $(DIST)/lib/components
	$(INSTALL) -m 555 $(SHARED_LIBRARY) $(DIST)/bin/components
else
	$(INSTALL) -m 555 $(SHARED_LIBRARY) $(DIST)/lib
	$(INSTALL) -m 555 $(SHARED_LIBRARY) $(DIST)/bin
endif
endif
	+$(LOOP_OVER_DIRS)

install:: $(PROGRAM) $(SIMPLE_PROGRAMS)
ifdef MAPS
	$(INSTALL) -m 444 $(MAPS) $(DIST)/bin
endif
ifdef PROGRAM
	$(INSTALL) -m 555 $(PROGRAM) $(DIST)/bin
endif
ifdef SIMPLE_PROGRAMS
	$(INSTALL) -m 555 $(SIMPLE_PROGRAMS) $(DIST)/bin
endif
	+$(LOOP_OVER_DIRS)
endif

checkout:
	cd $(topsrcdir); $(MAKE) -f client.mk checkout

run_viewer: $(DIST)/bin/viewer
	cd $(DIST)/bin; \
	MOZILLA_FIVE_HOME=`pwd` \
	LD_LIBRARY_PATH=".:$(LIBS_PATH):$$LD_LIBRARY_PATH" \
	viewer

run_apprunner: $(DIST)/bin/apprunner
	cd $(DIST)/bin; \
	MOZILLA_FIVE_HOME=`pwd` \
	LD_LIBRARY_PATH=".:$(LIBS_PATH):$$LD_LIBRARY_PATH" \
	apprunner

clean clobber::
	rm -rf $(ALL_TRASH)
	+$(LOOP_OVER_DIRS)

realclean clobber_all::
	rm -rf $(wildcard *.OBJ) dist $(ALL_TRASH)
	+$(LOOP_OVER_DIRS)

alltags:
	rm -f TAGS
	find . -name dist -prune -o \( -name '*.[hc]' -o -name '*.cp' -o -name '*.cpp' \) -print | xargs etags -a

#
# Turn on C++ linking if we have any .cpp files
# (moved this from config.mk so that config.mk can be included 
#  before the CPPSRCS are defined)
#
ifdef CPPSRCS
CPP_PROG_LINK = 1
endif

#
# PROGRAM = Foo
# creates OBJS, links with LIBS to create Foo
#
$(PROGRAM): $(PROGOBJS)
ifeq ($(OS_ARCH),OS2)
	$(LINK) -FREE -OUT:$@ $(LDFLAGS) $(OS_LFLAGS) $(PROGOBJS)  $(EXTRA_LIBS) -MAP:$(@:.exe=.map) $(OS_LIBS) $(DEF_FILE)
else
ifeq ($(OS_ARCH),WINNT)
	$(CC) $(PROGOBJS) -Fe$@ -link $(LDFLAGS) $(OS_LIBS) $(EXTRA_LIBS)
else
ifeq ($(CPP_PROG_LINK),1)
	$(CCC) $(WRAP_MALLOC_CFLAGS) -o $@ $(PROGOBJS) $(LDFLAGS) $(LIBS_DIR) $(LIBS) $(OS_LIBS) $(EXTRA_LIBS) $(WRAP_MALLOC_LIB)
else
	$(CC) -o $@ $(PROGOBJS) $(LDFLAGS) $(LIBS_DIR) $(LIBS) $(OS_LIBS) $(EXTRA_LIBS)
endif
endif
endif

#
# This is an attempt to support generation of multiple binaries
# in one directory, it assumes everything to compile Foo is in
# Foo.o (from either Foo.c or Foo.cpp).
#
# SIMPLE_PROGRAMS = Foo Bar
# creates Foo.o Bar.o, links with LIBS to create Foo, Bar.
#
#
$(SIMPLE_PROGRAMS):$(OBJDIR)/%: $(OBJDIR)/%.o
ifeq ($(CPP_PROG_LINK),1)
	$(CCC) $(WRAP_MALLOC_CFLAGS) -o $@ $< $(LDFLAGS) $(LIBS_DIR) $(LIBS) $(OS_LIBS) $(EXTRA_LIBS) $(WRAP_MALLOC_LIB)
else
	$(CC) -o $@ $< $(LDFLAGS) $(LIBS_DIR) $(LIBS) $(OS_LIBS) $(EXTRA_LIBS) $(WRAP_MALLOC_LIB)
endif

#
# Purify target.  Solaris/sparc only to start.
# Purify does not recognize "egcs" or "c++" so we go with 
# "gcc" and "g++" for now.
#
pure:	$(PROGRAM)
ifeq ($(CPP_PROG_LINK),1)
	$(PURIFY) $(CCC) -o $^.pure $(PROGOBJS) $(LDFLAGS) $(LIBS_DIR) $(LIBS) $(OS_LIBS) $(EXTRA_LIBS)
else
	$(PURIFY) $(CC) -o $^.pure $(PROGOBJS) $(LDFLAGS) $(LIBS_DIR) $(LIBS) $(OS_LIBS) $(EXTRA_LIBS)
endif
	$(INSTALL) -m 555 $^.pure $(DIST)/bin

quantify: $(PROGRAM)
ifeq ($(CPP_PROG_LINK),1)
	$(QUANTIFY) $(CCC) -o $^.quantify $(PROGOBJS) $(LDFLAGS) $(LIBS_DIR) $(LIBS) $(OS_LIBS) $(EXTRA_LIBS)
else
	$(QUANTIFY) $(CC) -o $^.quantify $(PROGOBJS) $(LDFLAGS) $(LIBS_DIR) $(LIBS) $(OS_LIBS) $(EXTRA_LIBS)
endif
	$(INSTALL) -m 555 $^.quantify $(DIST)/bin

ifneq ($(OS_ARCH),OS2)

#
# This allows us to create static versions of the shared libraries
# that are built using other static libraries.  Confused...?
#
ifdef SHARED_LIBRARY_LIBS
ifeq ($(OS_ARCH),NTO)
AR_LIST		:= ar.elf t
AR_EXTRACT	:= ar.elf x
else
AR_LIST		:= ar t
AR_EXTRACT	:= ar x
endif
ifeq ($(OS_ARCH),OSF1)
CLEANUP1	:= | egrep -v '(________64ELEL_)'
CLEANUP2	:= rm -f ________64ELEL_
else
CLEANUP2	:= true
endif
SUB_LOBJS	= $(shell for lib in $(SHARED_LIBRARY_LIBS); do $(AR_LIST) $${lib} $(CLEANUP1); done;)
endif

$(LIBRARY): $(OBJS) $(LOBJS)
	rm -f $@
ifdef SHARED_LIBRARY_LIBS
	@rm -f $(SUB_LOBJS)
	@for lib in $(SHARED_LIBRARY_LIBS); do $(AR_EXTRACT) $${lib}; $(CLEANUP2); done
endif
	$(AR) $(OBJS) $(LOBJS) $(SUB_LOBJS)
	$(RANLIB) $@
	@rm -f foodummyfilefoo $(SUB_LOBJS)
else
ifdef OS2_IMPLIB
$(LIBRARY): $(OBJS) $(DEF_FILE)
	rm -f $@
	$(IMPLIB) $@ $(DEF_FILE)
	$(RANLIB) $@
else
$(LIBRARY): $(OBJS)
	rm -f $@
	$(AR) $(LIBOBJS),,
	$(RANLIB) $@
endif
endif

ifneq ($(OS_ARCH),OS2)
$(SHARED_LIBRARY): $(OBJS) $(LOBJS)
	rm -f $@
	$(MKSHLIB) -o $@ $(OBJS) $(LOBJS) $(EXTRA_DSO_LDOPTS)
	chmod +x $@
else
$(SHARED_LIBRARY): $(OBJS) $(DEF_FILE)
	rm -f $@
	$(LINK_DLL) $(OBJS) $(OS_LIBS) $(EXTRA_LIBS) $(DEF_FILE)
	chmod +x $@
endif

ifneq (,$(filter OS2 WINNT,$(OS_ARCH)))
$(DLL): $(OBJS) $(EXTRA_LIBS)
	rm -f $@
ifeq ($(OS_ARCH),OS2)
	$(LINK_DLL) $(OBJS) $(EXTRA_LIBS) $(OS_LIBS)
else
	$(LINK_DLL) $(OBJS) $(OS_LIBS) $(EXTRA_LIBS)
endif
endif

$(OBJDIR)/%: %.c
ifneq (,$(filter OS2 WINNT,$(OS_ARCH)))
	$(CC) -Fo$@ -c $(CFLAGS) $<
else
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<
endif

$(OBJDIR)/%.o: %.c
ifneq (,$(filter OS2 WINNT,$(OS_ARCH)))
	$(CC) -Fo$@ -c $(CFLAGS) $<
else
	$(CC) -o $@ -c $(CFLAGS) $<
endif

# The AS_DASH_C_FLAG is needed cause not all assemblers (Solaris) accept
# a '-c' flag.
$(OBJDIR)/%.o: %.s
	$(AS) -o $@ $(ASFLAGS) $(AS_DASH_C_FLAG) $<

$(OBJDIR)/%.o: %.S
	$(AS) -o $@ $(ASFLAGS) -c $<

$(OBJDIR)/%: %.cpp
	$(CCC) -o $@ $(CXXFLAGS) $< $(LDFLAGS)

#
# Please keep the next two rules in sync.
#
$(OBJDIR)/%.o: %.cc
	$(CCC) -o $@ -c $(CXXFLAGS) $<

$(OBJDIR)/%.o: %.cpp
ifdef STRICT_CPLUSPLUS_SUFFIX
	echo "#line 1 \"$*.cpp\"" | cat - $*.cpp > $(OBJDIR)/t_$*.cc
	$(CCC) -o $@ -c $(CXXFLAGS) $(OBJDIR)/t_$*.cc
	rm -f $(OBJDIR)/t_$*.cc
else
ifneq (,$(filter OS2 WINNT,$(OS_ARCH)))
	$(CCC) -Fo$@ -c $(CXXFLAGS) $<
else
	$(CCC) -o $@ -c $(CXXFLAGS) $<
endif
endif #STRICT_CPLUSPLUS_SUFFIX

%.i: %.cpp
	$(CCC) -C -E $(CXXFLAGS) $< > $*.i

%.i: %.c
	$(CC) -C -E $(CFLAGS) $< > $*.i

%: %.pl
	rm -f $@; cp $< $@; chmod +x $@

%: %.sh
	rm -f $@; cp $< $@; chmod +x $@

# Cancel these implicit rules
#
%: %,v

%: RCS/%,v

%: s.%

%: SCCS/s.%



# these rules only apply when the XPIDL compiler is available
ifdef USE_XPIDL
%.h: %.idl $(IDL_COMPILE)
	$(IDL_COMPILE) -m header -w -I $(DIST)/idl $<


%.cpp: %.idl $(IDL_COMPILE)
	$(IDL_COMPILE) -m stub -w -I $(DIST)/idl $<

endif

ifdef DIRS
$(DIRS)::
	@if test -d $@; then				\
		set $(EXIT_ON_ERROR);			\
		echo "cd $@; $(MAKE)";			\
		cd $@; $(MAKE);				\
		set +e;					\
	else						\
		echo "Skipping non-directory $@...";	\
	fi;						\
	$(CLICK_STOPWATCH)
endif

###############################################################################
# Update Makefiles
###############################################################################
#
$(OBJDIR)/Makefile: Makefile.in
	@echo Updating $@
	$(topsrcdir)/build/autoconf/update-makefile.sh

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

clobber::
	rm -f $(XPDIST)/classes/$(PACKAGE)/*.class

endif
endif

#
# JDIRS -- like JSRCS, except you can give a list of directories and it will
# compile all the out-of-date java files in those directories.
#
# NOTE: recursing through these can speed things up, but they also cause
# some builds to run out of memory
#
ifdef JDIRS
ifdef JAVA_OR_NSJVM
export:: $(JAVA_DESTPATH) $(JAVA_DESTPATH)/$(PACKAGE)
	@for d in $(JDIRS); do							\
		if test -d $$d; then						\
			set $(EXIT_ON_ERROR);					\
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
		fi;								\
		$(CLICK_STOPWATCH);						\
	done
endif
endif

#
# JDK_GEN -- for generating "old style" native methods
#
# Generate JDK Headers and Stubs into the '_gen' and '_stubs' directory
#
ifneq ($(JDK_GEN),)
ifdef JAVA_OR_NSJVM
ifdef NSBUILDROOT
INCLUDES		+= -I$(JDK_GEN_DIR) -I$(XPDIST)
else
INCLUDES		+= -I$(JDK_GEN_DIR)
endif
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
ifdef NSBUILDROOT
INCLUDES		+= -I$(JRI_GEN_DIR) -I$(XPDIST)
else
INCLUDES		+= -I$(JRI_GEN_DIR)
endif
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
ifdef NSBUILDROOT
INCLUDES		+= -I$(JNI_GEN_DIR) -I$(XPDIST)
else
INCLUDES		+= -I$(JNI_GEN_DIR)
endif
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
	$(NSINSTALL) -t -m 444 $(JMC_EXPORT_FILES) $(JMCSRCDIR)
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
JMC_OBJS		= $(patsubst %,$(OBJDIR)/%.o,$(JMC_GEN))

$(JMC_GEN_DIR)/M%.h: $(JMCSRCDIR)/%.class
	$(JMC) -d $(JMC_GEN_DIR) -interface $(JMC_GEN_FLAGS) $(?F:.class=)

$(JMC_GEN_DIR)/M%.c: $(JMCSRCDIR)/%.class
	$(JMC) -d $(JMC_GEN_DIR) -module $(JMC_GEN_FLAGS) $(?F:.class=)

$(OBJDIR)/M%.o: $(JMC_GEN_DIR)/M%.h $(JMC_GEN_DIR)/M%.c
ifeq ($(OS_ARCH),OS2)
	$(CC) -Fo$@ -c $(CFLAGS) $(JMC_GEN_DIR)/M$*.c
else
	$(CC) -o $@ -c $(CFLAGS) $(JMC_GEN_DIR)/M$*.c
endif

export:: $(JMC_HEADERS) $(JMC_STUBS)
endif # JAVA_OR_NSJVM
endif

################################################################################
# Copy each element of EXPORTS to $(XPDIST)/include

ifneq ($(EXPORTS),)
$(XPDIST)/include::
	@if test ! -d $@; then echo Creating $@; rm -rf $@; $(NSINSTALL) -D $@; else true; fi

export:: $(EXPORTS) $(XPDIST)/include
	$(INSTALL) -m 444 $^
endif

################################################################################
# Copy each element of IDLSRCS to $(XPDIST)/idl

ifneq ($(IDLSRCS),)
$(XPDIST)/idl::
	@if test ! -d $@; then echo Creating $@; rm -rf $@; $(NSINSTALL) -D $@; else true; fi

export:: $(IDLSRCS) $(XPDIST)/idl
	$(INSTALL) -m 444 $^

endif

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
		$$tmpname = "$(OBJDIR)/fix.md" . $$$$;                        \
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
# X dependency system
#############################################################################
ifdef COMPILER_DEPEND
depend::
	@echo "$(MAKE): No need to run depend target.\
			Using compiler-based depend." 1>&2
ifeq ($(GNU_CC)$(GNU_CXX),)
# Non-GNU compilers
	@echo "`echo '$(MAKE):'|sed 's/./ /g'`"\
	'(Compiler-based depend was turned on by "--enable-depend".)' 1>&2
else
# GNU compilers
	@space="`echo '$(MAKE): '|sed 's/./ /g'`";\
	echo "$$space"'Since you are using a GNU compiler,\
		it is on by default.' 1>&2; \
	echo "$$space"'To turn it off, pass --disable-md to configure.' 1>&2
endif
else
$(MKDEPENDENCIES)::
	touch $(MKDEPENDENCIES)
ifdef USE_AUTOCONF
	$(MKDEPEND) -p$(OBJDIR_NAME)/ -o'.o' -f$(MKDEPENDENCIES) $(DEFINES) $(ACDEFINES) $(INCLUDES) $(addprefix $(srcdir)/,$(CSRCS) $(CPPSRCS)) >/dev/null 2>&1
	@mv depend.mk depend.mk.old && cat depend.mk.old | sed "s|^$(OBJDIR_NAME)/$(srcdir)/|$(OBJDIR_NAME)/|g" > depend.mk && rm -f depend.mk.old
else
	$(MKDEPEND) -p$(OBJDIR_NAME)/ -o'.o' -f$(MKDEPENDENCIES) $(INCLUDES) $(CSRCS) $(CPPSRCS)
endif

ifndef MOZ_NATIVE_MAKEDEPEND
$(MKDEPEND):
	cd $(DEPTH)/config; $(MAKE) nsinstall
	cd $(MKDEPEND_DIR); $(MAKE)
endif

# Dont do the detect hackery for autoconf builds.  It makes them painfully
# slow and its not needed anyway, since autoconf does it much better.
ifndef USE_AUTOCONF

# Rules to for detection
$(MOZILLA_DETECT_GEN):
	cd $(MOZILLA_DETECT_DIR); $(MAKE)

detect: $(MOZILLA_DETECT_GEN)

endif # ! USE_AUTOCONF

ifndef MOZ_NATIVE_MAKEDEPEND
MKDEPEND_BUILTIN = $(MKDEPEND) 
else
MKDEPEND_BUILTIN =
endif

ifdef OBJS
depend::  $(MAKE_DIRS) $(MKDEPEND_BUILTIN) $(MKDEPENDENCIES)
else
depend::
endif
	+$(LOOP_OVER_DIRS)

dependclean::
	rm -f $(MKDEPENDENCIES)
	+$(LOOP_OVER_DIRS)

-include $(OBJDIR)/depend.mk

endif # ! COMPILER_DEPEND

#############################################################################
# Yet another depend system: -MD
ifdef COMPILER_DEPEND
ifdef OBJS
MDDEPEND_FILES := $(foreach obj, $(OBJS), \
                    $(MDDEPDIR)/$(basename $(notdir $(obj))).pp)
MDDEPEND_FILES := $(wildcard $(MDDEPEND_FILES))
ifdef MDDEPEND_FILES
ifdef PERL
# The script mddepend.pl checks the dependencies and writes to stdout
# one rule to force out-of-date objects. For example,
#   foo.o boo.o: FORCE
# The script has an advantage over including the *.pp files directly
# because it handles missing header files. 'make' would complain that
# there is no way to build missing headers.
$(MDDEPDIR)/.all.pp: FORCE
	@$(PERL) $(topsrcdir)/config/mddepend.pl $(MDDEPEND_FILES) >$@
-include $(MDDEPDIR)/.all.pp
else
include $(MDDEPEND_FILES)
endif
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
.SUFFIXES: .out .a .ln .o .c .cc .C .cpp .y .l .s .S .h .sh .i .pl .class .java .html .pp .mk .in

#
# Don't delete these files if we get killed.
#
.PRECIOUS: .java $(JDK_HEADERS) $(JDK_STUBS) $(JRI_HEADERS) $(JRI_STUBS) $(JMC_HEADERS) $(JMC_STUBS)

#
# Fake targets.  Always run these rules, even if a file/directory with that
# name already exists.
#
.PHONY: all all_platforms alltags boot checkout clean clobber clobber_all export install libs realclean run_viewer run_apprunner $(DIRS) FORCE

# Used as a dependency to force targets to rebuild
FORCE:

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
