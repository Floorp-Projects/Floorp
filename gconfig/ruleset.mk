# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

#######################################################################
#                                                                     #
# Parameters to this makefile (set these in this file):               #
#                                                                     #
# a)                                                                  #
#	TARGETS	-- the target to create                               #
#			(defaults to $LIBRARY $PROGRAM)               #
# b)                                                                  #
#	DIRS	-- subdirectories for make to recurse on              #
#			(the 'all' rule builds $TARGETS $DIRS)        #
# c)                                                                  #
#	CSRCS, CPPSRCS -- .c and .cpp files to compile                #
#			(used to define $OBJS)                        #
# d)                                                                  #
#	PROGRAM	-- the target program name to create from $OBJS       #
#			($OBJDIR automatically prepended to it)       #
# e)                                                                  #
#	LIBRARY	-- the target library name to create from $OBJS       #
#			($OBJDIR automatically prepended to it)       #
# f)                                                                  #
#	JSRCS	-- java source files to compile into class files      #
#			(if you don't specify this it will default    #
#			 to *.java)                                   #
# g)                                                                  #
#	PACKAGE	-- the package to put the .class files into           #
#			(e.g. netscape/applet)                        #
# h)                                                                  #
#	JMC_EXPORT -- java files to be exported for use by JMC_GEN    #
#			(this is a list of Class names)               #
# i)                                                                  #
#	JRI_GEN	-- files to run through javah to generate headers     #
#                  and stubs                                          #
#			(output goes into the _jri sub-dir)           #
# j)                                                                  #
#	JMC_GEN	-- files to run through jmc to generate headers       #
#                  and stubs                                          #
#			(output goes into the _jmc sub-dir)           #
#                                                                     #
#######################################################################

LOOP_DIRS_LIBS = $(DIRS_LIBS) $(DIRS)
LOOP_DIRS_EXPORT = $(DIRS_EXPORT) $(DIRS)
LOOP_DIRS_ALL    = $(DIRS) $(DIRS_EXPORT) $(DIRS_LIBS)
#
#  At this time, the CPU_TAG value is actually assigned.
#

CPU_TAG =

#
# When the processor is NOT 386-based on Windows NT, override the
# value of $(CPU_TAG).
#
ifeq ($(OS_ARCH), WINNT)
	ifneq ($(CPU_ARCH),x386)
		CPU_TAG = _$(CPU_ARCH)
	endif
endif

#
#  At this time, the COMPILER_TAG value is actually assigned.
#

ifneq ($(DEFAULT_COMPILER), $(CC))
#
# XXX
# Temporary define for the Client; to be removed when binary release is used
#
	ifdef MOZILLA_CLIENT
		COMPILER_TAG =
	else
		COMPILER_TAG = _$(CC)
	endif
else
	COMPILER_TAG =
endif

#
# This makefile contains rules for building the following kinds of
# objects:
# - (1) LIBRARY: a static (archival) library
# - (2) SHARED_LIBRARY: a shared (dynamic link) library
# - (3) IMPORT_LIBRARY: an import library, used only on Windows
# - (4) PURE_LIBRARY: a library for Purify
# - (5) PROGRAM: an executable binary
#
# NOTE:  The names of libraries can be generated by simply specifying
# LIBRARY_NAME and LIBRARY_VERSION.
#

ifdef LIBRARY_NAME
	ifeq ($(OS_ARCH), WINNT)
		#
		# Win16 requires library names conforming to the 8.3 rule.
		# other platforms do not.
		#
		LIBRARY        = $(OBJDIR)/$(LIBRARY_NAME)$(LIBRARY_VERSION)$(ARCHIVE_SUFFIX)$(STATIC_LIB_SUFFIX)
ifndef ARCHIVE_ONLY
	    SHARED_LIBRARY = $(OBJDIR)/$(LIBRARY_NAME)$(LIBRARY_VERSION).dll
		IMPORT_LIBRARY = $(OBJDIR)/$(LIBRARY_NAME)$(LIBRARY_VERSION)$(STATIC_LIB_SUFFIX)
endif
	else
		LIBRARY = $(OBJDIR)/lib$(LIBRARY_NAME)$(LIBRARY_VERSION)$(STATIC_LIB_SUFFIX)
ifndef ARCHIVE_ONLY
		ifeq ($(OS_ARCH)$(OS_RELEASE), AIX4.1)
			SHARED_LIBRARY = $(OBJDIR)/lib$(LIBRARY_NAME)_shr.a
		else
			SHARED_LIBRARY = $(OBJDIR)/lib$(LIBRARY_NAME).$(DLL_SUFFIX)
		endif

		ifdef HAVE_PURIFY
			ifdef DSO_BACKEND
				PURE_LIBRARY = $(OBJDIR)/purelib$(LIBRARY_NAME).$(DLL_SUFFIX)
			else
				PURE_LIBRARY = $(OBJDIR)/purelib$(LIBRARY_NAME).$(LIB_SUFFIX)
			endif
		endif
endif
	endif
endif

#
# Common rules used by lots of makefiles...
#

ifdef PROGRAM
	PROGRAM := $(addprefix $(OBJDIR)/, $(PROGRAM)$(PROG_SUFFIX))
endif

MAKEDEPFILE := $(addprefix $(OBJDIR)/, make.dep)

#ifdef RCFILE
#	RCFILE := $(addprefix $(OBJDIR)/, $(RCFILE)$(RC_SUFFIX))
#	endif

ifndef LIBRARY
	ifdef LIBRARY_NAME
  	  ifeq ($(OS_ARCH), WINNT)
		LIBRARY = $(LIBRARY_NAME).$(LIB_SUFFIX)
      else
		LIBRARY = lib$(LIBRARY_NAME).$(LIB_SUFFIX)
      endif
	endif
endif

# Rules to convert EXTRA_LIBS to platform-dependent naming scheme
ifdef EXTRA_LIBS
	EXTRA_LIBS := $(addprefix $(CONFIG_DIST_LIB)$(OPT_SLASH)$(LIB_PREFIX), $(EXTRA_LIBS:%=%$(STATIC_LIB_SUFFIX)))
endif

# Rules to convert AR_LIBS to platform-dependent naming scheme
ifdef AR_LIBS
  ifeq ($(OS_ARCH), WINNT)
	AR_LIBS := $(addprefix $(CONFIG_DIST_LIB)$(OPT_SLASH), $(AR_LIBS:%=%$(LIBRARY_VERSION)$(ARCHIVE_SUFFIX)$(STATIC_LIB_SUFFIX)))
  else
	AR_LIBS := $(addprefix $(CONFIG_DIST_LIB)$(OPT_SLASH)lib, $(AR_LIBS:%=%$(LIBRARY_VERSION)$(ARCHIVE_SUFFIX)$(STATIC_LIB_SUFFIX)))
  endif
endif

ifdef STATIC_LIBS
  ifeq ($(OS_ARCH), WINNT)
	STATIC_LIBS := $(addprefix $(CONFIG_DIST_LIB)$(OPT_SLASH), $(STATIC_LIBS:%=%$(ARCHIVE_SUFFIX)$(STATIC_LIB_SUFFIX)))
  else
	STATIC_LIBS := $(addprefix $(CONFIG_DIST_LIB)$(OPT_SLASH)lib, $(STATIC_LIBS:%=%_s$(ARCHIVE_SUFFIX)$(STATIC_LIB_SUFFIX)))
  endif
endif

# Rules to convert LD_LIBS to platform-dependent naming scheme
ifdef LD_LIBS
  ifeq ($(OS_ARCH), WINNT)
	LD_LIBS := $(addprefix $(CONFIG_DIST_LIB)$(OPT_SLASH), $(LD_LIBS:%=%$(STATIC_LIB_SUFFIX)))
  else
	LD_LIBS := $(addprefix $(DIST)$(OPT_SLASH)bin$(OPT_SLASH)lib, $(LD_LIBS:%=%.$(DLL_SUFFIX)))
  endif
endif

ifdef LIBRARY
#	LIBRARY := $(addprefix $(OBJDIR)/, $(LIBRARY))
	ifndef ARCHIVE_ONLY
		ifeq ($(OS_ARCH),WINNT)
ifndef LIBRARY_NAME
			SHARED_LIBRARY = $(LIBRARY:.lib=.dll)
endif
		else
			ifeq ($(OS_ARCH),HP-UX)
				SHARED_LIBRARY = $(LIBRARY:.a=.sl)
			else
				# SunOS 4 _requires_ that shared libs have a version number.
				ifeq ($(OS_RELEASE),4.1.3_U1)
					SHARED_LIBRARY = $(LIBRARY:.a=.so.1.0)
				else
					SHARED_LIBRARY = $(LIBRARY:.a=.so)
				endif
			endif
		endif
	endif
endif

ifdef PROGRAMS
	PROGRAMS := $(addprefix $(OBJDIR)/, $(PROGRAMS:%=%$(PROG_SUFFIX)))
endif

ifndef TARGETS
	ifeq ($(OS_ARCH), WINNT)
		TARGETS = $(LIBRARY) $(SHARED_LIBRARY) $(IMPORT_LIBRARY) $(PROGRAM)
	else
		TARGETS = $(LIBRARY) $(SHARED_LIBRARY)
		ifdef HAVE_PURIFY
			TARGETS += $(PURE_LIBRARY)
		endif
		TARGETS += $(PROGRAM)
	endif
endif

ifdef CSRCS
DEPENDFILES += $(OBJS)
DODEPEND=1
endif
ifdef CPPSRCS
DEPENDFILES += $(OBJS)
DODEPEND=1
endif

ifndef OBJS
	OBJS :=	$(JRI_STUB_CFILES) $(addsuffix $(OBJ_SUFFIX), $(JMC_GEN)) $(CSRCS:.c=$(OBJ_SUFFIX)) \
		$(CPPSRCS:.cpp=$(OBJ_SUFFIX)) $(ASFILES:.s=$(OBJ_SUFFIX))
	OBJS := $(addprefix $(OBJDIR)/$(PROG_PREFIX), $(OBJS))
endif

ifeq ($(OS_TARGET), WIN16)
	comma   := ,
	empty   :=
	space   := $(empty) $(empty)
	W16OBJS := $(subst $(space),$(comma)$(space),$(strip $(OBJS)))
	W16TEMP  = $(OS_LIBS) $(EXTRA_LIBS)
	ifeq ($(strip $(W16TEMP)),)
		W16LIBS =
	else
		W16LIBS := library $(subst $(space),$(comma)$(space),$(strip $(W16TEMP)))
	endif
endif

ifeq ($(OS_ARCH),WINNT)
	ifneq ($(OS_TARGET), WIN16)
		OBJS += $(RES)
	endif
	ifdef DLL
		DLL := $(addprefix $(OBJDIR)/, $(DLL))
		LIB := $(addprefix $(OBJDIR)/, $(LIB))
	endif
	MAKE_OBJDIR		= mkdir $(OBJDIR)
else
	define MAKE_OBJDIR
	if test ! -d $(@D); then rm -rf $(@D); $(NSINSTALL) -D $(@D); fi
	endef
endif

ifndef PACKAGE
	PACKAGE = .
endif

ALL_TRASH :=	$(TARGETS) $(OBJS) $(OBJDIR) LOGS TAGS $(GARBAGE) \
		$(NOSUCHFILE) $(JDK_HEADER_CFILES) $(JDK_STUB_CFILES) \
		$(JRI_HEADER_CFILES) $(JRI_STUB_CFILES) $(JMC_STUBS) \
		$(JMC_HEADERS) $(JMC_EXPORT_FILES) so_locations \
		_gen _jmc _jri _stubs \
		$(wildcard $(JAVA_DESTPATH)/$(PACKAGE)/*.class)

ifdef JDIRS
	ALL_TRASH += $(addprefix $(JAVA_DESTPATH)/,$(JDIRS))
endif

ifdef NSBUILDROOT
	JDK_GEN_DIR  = $(SOURCE_XP_DIR)/_gen
	JMC_GEN_DIR  = $(SOURCE_XP_DIR)/_jmc
	JRI_GEN_DIR  = $(SOURCE_XP_DIR)/_jri
	JDK_STUB_DIR = $(SOURCE_XP_DIR)/_stubs
else
	JDK_GEN_DIR  = _gen
	JMC_GEN_DIR  = _jmc
	JRI_GEN_DIR  = _jri
	JDK_STUB_DIR = _stubs
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

ifdef REQUIRES
ifeq ($(OS_TARGET),WIN16)
	INCLUDES        += -I$(SOURCE_XP_DIR)/public/win16
else
	MODULE_INCLUDES := $(addprefix -I$(SOURCE_XP_DIR)/public/, $(REQUIRES))
	INCLUDES        += $(MODULE_INCLUDES)
	ifeq ($(MODULE), sectools)
		PRIVATE_INCLUDES := $(addprefix -I$(SOURCE_XP_DIR)/private/, $(REQUIRES))
		INCLUDES         += $(PRIVATE_INCLUDES)
	endif
endif
endif

ifdef DIRS
	LOOP_OVER_DIRS		=						\
		@for directory in $(DIRS); do					\
			if test -d $$directory; then				\
				set $(EXIT_ON_ERROR);				\
				echo "cd $$directory; $(MAKE) $@";		\
				$(MAKE) -C $$directory $@;			\
				set +e;						\
			else							\
				echo "Skipping non-directory $$directory...";	\
			fi;							\
			$(CLICK_STOPWATCH);					\
		done
endif


ifdef DIRS_EXPORT
	LOOP_OVER_EXPORT_DIRS		=						\
		@for directory in $(LOOP_DIRS_EXPORT); do					\
			if test -d $$directory; then				\
				set $(EXIT_ON_ERROR);				\
				echo "cd $$directory; $(MAKE) $@";		\
				$(MAKE) -C $$directory $@;			\
				set +e;						\
			else							\
				echo "Skipping non-directory $$directory...";	\
			fi;							\
			$(CLICK_STOPWATCH);					\
		done
else
ifdef DIRS
	LOOP_OVER_EXPORT_DIRS		=						\
		@for directory in $(DIRS); do					\
			if test -d $$directory; then				\
				set $(EXIT_ON_ERROR);				\
				echo "cd $$directory; $(MAKE) $@";		\
				$(MAKE) -C $$directory $@;			\
				set +e;						\
			else							\
				echo "Skipping non-directory $$directory...";	\
			fi;							\
			$(CLICK_STOPWATCH);					\
		done
endif
endif


ifdef DIRS_LIBS
	LOOP_OVER_LIBS_DIRS		=						\
		@for directory in $(LOOP_DIRS_LIBS); do					\
			if test -d $$directory; then				\
				set $(EXIT_ON_ERROR);				\
				echo "cd $$directory; $(MAKE) $@";		\
				$(MAKE) -C $$directory $@;			\
				set +e;						\
			else							\
				echo "Skipping non-directory $$directory...";	\
			fi;							\
			$(CLICK_STOPWATCH);					\
		done
else
ifdef DIRS
	LOOP_OVER_LIBS_DIRS		=						\
		@for directory in $(DIRS); do					\
			if test -d $$directory; then				\
				set $(EXIT_ON_ERROR);				\
				echo "cd $$directory; $(MAKE) $@";		\
				$(MAKE) -C $$directory $@;			\
				set +e;						\
			else							\
				echo "Skipping non-directory $$directory...";	\
			fi;							\
			$(CLICK_STOPWATCH);					\
		done
endif
endif

ifdef DIRS
	LOOP_OVER_ALL_DIRS		=						\
		@for directory in $(LOOP_DIRS_ALL); do					\
			if test -d $$directory; then				\
				set $(EXIT_ON_ERROR);				\
				echo "cd $$directory; $(MAKE) $@";		\
				$(MAKE) -C $$directory $@;			\
				set +e;						\
			else							\
				echo "Skipping non-directory $$directory...";	\
			fi;							\
			$(CLICK_STOPWATCH);					\
		done
else
ifdef DIRS_EXPORT
	LOOP_OVER_ALL_DIRS		=						\
		@for directory in $(LOOP_DIRS_ALL); do					\
			if test -d $$directory; then				\
				set $(EXIT_ON_ERROR);				\
				echo "cd $$directory; $(MAKE) $@";		\
				$(MAKE) -C $$directory $@;			\
				set +e;						\
			else							\
				echo "Skipping non-directory $$directory...";	\
			fi;							\
			$(CLICK_STOPWATCH);					\
		done
else
ifdef DIRS_LIBS
	LOOP_OVER_ALL_DIRS		=						\
		@for directory in $(LOOP_DIRS_ALL); do					\
			if test -d $$directory; then				\
				set $(EXIT_ON_ERROR);				\
				echo "cd $$directory; $(MAKE) $@";		\
				$(MAKE) -C $$directory $@;			\
				set +e;						\
			else							\
				echo "Skipping non-directory $$directory...";	\
			fi;							\
			$(CLICK_STOPWATCH);					\
		done
endif
endif
endif



ifeq ($(OS_ARCH),WINNT)
ifdef AR_LIBS
EXTRACT_OBJS =  \
    lib /list:$(OBJDIR)\\$(LIBRARY_NAME).lst $(LIBRARY) ; \
    perl -I$(GDEPTH)/gconfig $(GDEPTH)/gconfig/extract_objs.pl \
    "LIST=$(OBJDIR)\\$(LIBRARY_NAME).lst" \
    "LIBRARY=$(LIBRARY)"
endif
endif


# special stuff for tests rule in rules.mk

ifneq ($(OS_ARCH),WINNT)
	REGDATE = $(subst \ ,, $(shell perl  $(GDEPTH)/$(MODULE)/scripts/now))
else
	REGCOREDEPTH = $(subst \\,/,$(GDEPTH))
	REGDATE = $(subst \ ,, $(shell perl  $(GDEPTH)/$(MODULE)/scripts/now))
endif


