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

#
# config.mk
#
# Determines the platform and builds the macros needed to load the
# appropriate platform-specific .mk file, then defines all (most?)
# of the generic macros.
#

# This wastes time.
include $(DEPTH)/config/common.mk

#
# Important internal static macros
#
OS_ARCH		:= $(subst /,_,$(shell uname -s))
OS_RELEASE	:= $(shell uname -r)
OS_TEST		:= $(shell uname -m)

#
# Tweak the default OS_ARCH and OS_RELEASE macros as needed.
#
ifeq ($(OS_ARCH),AIX)
OS_RELEASE	:= $(shell uname -v).$(shell uname -r)
endif
ifeq ($(OS_ARCH),BSD_386)
OS_ARCH		:= BSD_OS
endif
ifeq ($(OS_ARCH),IRIX64)
OS_ARCH		:= IRIX
endif
ifeq ($(OS_ARCH),UNIX_SV)
ifneq ($(findstring NCR,$(shell grep NCR /etc/bcheckrc | head -1 )),)
OS_ARCH		:= NCR
else
OS_ARCH		:= UNIXWARE
OS_RELEASE	:= $(shell uname -v)
endif
endif
ifeq ($(OS_ARCH),ncr)
OS_ARCH		:= NCR
endif
# This is the only way to correctly determine the actual OS version on NCR boxes.
ifeq ($(OS_ARCH),NCR)
OS_RELEASE	:= $(shell awk '{print $$3}' /etc/.relid | sed 's/^\([0-9]\)\(.\)\(..\)\(.*\)$$/\2.\3/')
endif
ifeq ($(OS_ARCH),UNIX_System_V)
OS_ARCH		:= NEC
endif
ifeq ($(OS_ARCH),SCO_SV)
OS_ARCH		:= SCOOS
OS_RELEASE	:= 5.0
endif
ifeq ($(OS_ARCH),SINIX-N)
OS_ARCH		:= SINIX
endif
# SINIX changes name to ReliantUNIX with 5.43
ifeq ($(OS_ARCH),ReliantUNIX-N)
OS_ARCH		:= SINIX
endif
ifeq ($(OS_ARCH),UnixWare)
OS_ARCH		:= UNIXWARE
endif

#
# Strip off the excessively long version numbers on these platforms,
# but save the version to allow multiple versions of the same base
# platform to be built in the same tree.
#
ifneq (,$(filter FreeBSD HP-UX IRIX Linux OSF1 SunOS,$(OS_ARCH)))
OS_VERS		:= $(suffix $(OS_RELEASE))
OS_RELEASE	:= $(basename $(OS_RELEASE))
OS_VERSION	:= $(shell echo $(OS_VERS) | sed 's/-.*//')
endif

OS_CONFIG	:= $(OS_ARCH)$(OS_RELEASE)

#
# Personal makefile customizations go in these optional make include files.
#
MY_CONFIG	:= $(DEPTH)/config/myconfig.mk
MY_RULES	:= $(DEPTH)/config/myrules.mk

#
# Relative pathname from top-of-tree to current source directory
#
ifneq (,$(filter-out OS2 WINNT,$(OS_ARCH)))
REVDEPTH	:= $(DEPTH)/config/revdepth
SRCDIR		= $(shell $(PERL) $(REVDEPTH).pl $(DEPTH))
endif

#
# Define an include-at-most-once flag
#
NS_CONFIG_MK	= 1

#
# Provide the means to easily override our tool directory locations.
#
ifdef NETSCAPE_HIERARCHY
CONTRIB_BIN	:= /tools/contrib/bin/
JAVA_BIN	:= /usr/local/java/bin/
LOCAL_BIN	:= /usr/local/bin/
LOCAL_SUN4	:= /usr/local/sun4/bin/
NS_BIN		:= /tools/ns/bin/
NS_LIB		:= /tools/ns/lib
JAVA_LIB	:= /usr/local/netscape/java/lib
XFEPRIVDIR	:= $(DEPTH)/../ns_priv/xfe/
else
NS_LIB		:= .
JAVA_LIB	:= .
endif

#
# Default command macros; can be overridden in <arch>.mk.
#
AS		= $(CC)
ASFLAGS		= $(CFLAGS)
BSDECHO		= echo
CC		= gcc
CCC		= g++
CCF		= $(CC) $(CFLAGS)
LINK_EXE	= $(LINK) $(OS_LFLAGS) $(LFLAGS)
LINK_DLL	= $(LINK) $(OS_DLLFLAGS) $(DLLFLAGS)
NFSPWD		= $(DEPTH)/config/nfspwd
PURIFY		= purify $(PURIFYOPTIONS)
QUANTIFY	= quantify $(QUANTIFYOPTIONS)
RANLIB		= /bin/true
SDKINSTALL	= $(NSINSTALL) -t
UNZIP_PROG	= $(LOCAL_BIN)/unzip
ZIP_PROG	= $(LOCAL_BIN)/zip
ZIP_FLAGS	= -0rq

ifeq ($(OS_ARCH),OS2)
EMPTY		:=
SLASH		:= /$(EMPTY)
BSLASH		:= \$(EMPTY)
SEMICOLON	:= ;$(EMPTY)
SPACE		:= $(EMPTY) $(EMPTY)
PATH_SEPARATOR	:= \;
RC		= flipper rc$(BIN_SUFFIX)
XP_DEFINE	= -DXP_PC
LIB_SUFFIX	= lib
DLL_SUFFIX	= dll
MAP_SUFFIX	= map
BIN_SUFFIX	= .exe
AR		= flipper ILibo //noignorecase //nologo $@
IMPLIB		= flipper ILibo //noignorecase //nologo $@
DLLFLAGS	= -DLL -OUT:$@ $(XLFLAGS) -MAP:$(@:.dll=.map)
LFLAGS		= $(OBJS) -OUT:$@ $(XLFLAGS) $(DEPLIBS) $(EXTRA_LIBS) -MAP:$(@:.dll=.map) $(DEF_FILE)
NSINSTALL	= nsinstall
INSTALL		= $(NSINSTALL)
JAVA_PROG	= flipper java -norestart
JAVAC_ZIP	= $(subst $(BSLASH),$(SLASH),$(JAVA_HOME))/lib/classes.zip
else
ifeq ($(OS_ARCH),WINNT)
PATH_SEPARATOR	:= :
RC		= rc$(BIN_SUFFIX)
XP_DEFINE	= -DXP_PC
LIB_SUFFIX	= lib
DLL_SUFFIX	= dll
BIN_SUFFIX	= .exe
AR		= lib -NOLOGO -OUT:"$@"
DLLFLAGS	= $(XLFLAGS) -OUT:"$@"
LFLAGS		= $(OBJS) $(DEPLIBS) $(EXTRA_LIBS) -OUT:"$@"
NSINSTALL	= nsinstall
INSTALL		= $(NSINSTALL)
JAVA_PROG	= java
else
PATH_SEPARATOR	:= :
XP_DEFINE	= -DXP_UNIX
AR		= ar cr $@
DLL_SUFFIX	= so
LIB_SUFFIX	= a
ifeq ($(AWT_11),1)
JAVA_PROG	= $(NS_BIN)java
JAVAC_ZIP	= $(NS_LIB)/classes.zip
else
JAVA_PROG	= $(LOCAL_BIN)java
JAVAC_ZIP	= $(JAVA_LIB)/javac.zip
endif
PERL		= $(NS_BIN)perl
TAR		= tar
EMACS		= xemacs
WHOAMI		= /usr/bin/whoami
endif
endif

#
# Debug by default.
#
OBJDIR_TAG	= _DBG
OPTIMIZER	= -g
JAVA_OPTIMIZER	= -g
XBCFLAGS	= -FR$*
XCFLAGS		= $(LCFLAGS)
XLFLAGS		= $(LLFLAGS)

ifeq ($(OS_ARCH),OS2)
OPTIMIZER	= -Ti+
XLFLAGS		+= -DEBUG
ifdef BUILD_PROFILE
OPTIMIZER	+= -Gh+ 
OBJDIR_TAG	= _PRF
else
OPTIMIZER	+= -DDEBUG
ifdef BUILD_MEMDBG
OPTIMIZER	+= -Tm+ -DXP_OS2_MEMDEBUG=1
OBJDIR_TAG	= _MEM
endif
endif
else
ifeq ($(OS_ARCH),WINNT)
OPTIMIZER	= -Od -Z7
JAVA_OPTIMIZER	= $(OPTIMIZER)
XLFLAGS		+= -DEBUG
else
DEFINES		= -DDEBUG -UNDEBUG -DDEBUG_$(shell $(WHOAMI)) -DTRACING
endif
endif

ifdef BUILD_OPT
OBJDIR_TAG	= _OPT
XBCFLAGS	=
ifeq ($(OS_ARCH),OS2)
OPTIMIZER	= -O+ -Oi -DNDEBUG
else
ifeq ($(OS_ARCH),WINNT)
OPTIMIZER	= -O2
else
OPTIMIZER	= -O
DEFINES		= -UDEBUG -DNDEBUG -DTRIMMED
endif
endif
endif

#
# XXX For now, we're including $(DEPTH)/include directly instead of
# getting this stuff from dist. This stuff is old and will eventually
# be put in the library directories where it belongs so that it can
# get exported to dist properly.
#
INCLUDES	= $(LOCAL_PREINCLUDES) $(MODULE_PREINCLUDES) -I$(DEPTH)/include $(LOCAL_INCLUDES) $(OS_INCLUDES)

LIBNT		= $(DIST)/lib/libnt.$(LIB_SUFFIX)
LIBAWT		= $(DIST)/lib/libawt.$(LIB_SUFFIX)
LIBMMEDIA	= $(DIST)/lib/libmmedia.$(LIB_SUFFIX)

#
# NSPR 2.0 is now the default, "setenv NSPR10 1" to go back to 1.0
#
ifndef NSPR10
NSPR20		= 1
NSPRDIR		= nsprpub
DEFINES		+= -DNSPR20
INCLUDES	+= -I$(DIST)/include
LIBNSPR		= $(DIST)/lib/libplds21.$(LIB_SUFFIX) \
		  $(DIST)/lib/libmsgc21.$(LIB_SUFFIX) \
		  $(DIST)/lib/libnspr21.$(LIB_SUFFIX)
PURELIBNSPR	= $(DIST)/lib/purelibplds21.$(LIB_SUFFIX) \
		  $(DIST)/lib/purelibmsgc21.$(LIB_SUFFIX) \
		  $(DIST)/lib/purelibnspr21.$(LIB_SUFFIX)
else
NSPRDIR		= nspr
LIBNSPR		= $(DIST)/lib/libnspr.$(LIB_SUFFIX)
PURELIBNSPR	= $(DIST)/lib/libpurenspr.$(LIB_SUFFIX)
endif

ifdef DBMALLOC
LIBNSPR		+= $(DIST)/lib/libdbmalloc.$(LIB_SUFFIX)
endif

ifeq ($(OS_ARCH),OS2)
LIBNSJAVA	= $(DIST)/lib/jrt$(MOZ_BITS)$(VERSION_NUMBER).$(LIB_SUFFIX)
LIBMD		= $(DIST)/lib/libjmd.$(LIB_SUFFIX)
LIBJAVA		= $(DIST)/lib/libjrt.$(LIB_SUFFIX)
LIBNSPR		= $(DIST)/lib/pr$(MOZ_BITS)$(VERSION_NUMBER).$(LIB_SUFFIX)
LIBXP		= $(DIST)/lib/libxp.$(LIB_SUFFIX)
else
ifeq ($(OS_ARCH),WINNT)
LIBNSJAVA	= $(DIST)/lib/jrt3221.$(LIB_SUFFIX)
else
LIBNSJAVA	= $(DIST)/lib/nsjava32.$(LIB_SUFFIX)
endif
endif

CFLAGS		= $(XP_DEFINE) $(OPTIMIZER) $(OS_CFLAGS) $(MDUPDATE_FLAGS) $(DEFINES) $(INCLUDES) $(XCFLAGS) $(PROF_FLAGS)
NOMD_CFLAGS	= $(XP_DEFINE) $(OPTIMIZER) $(OS_CFLAGS) $(DEFINES) $(INCLUDES) $(XCFLAGS)

#
# Include the binary distrib stuff, if necessary.
#
ifdef NS_BUILD_CORE
include $(DEPTH)/config/coreconf.mk
endif

#
# Now include the platform-specific stuff.
#
include $(DEPTH)/config/$(OS_ARCH).mk

#
# Some platforms (Solaris) might require builds using either
# (or both) compiler(s).
#
ifdef SHOW_CC_TYPE
COMPILER	= _$(notdir $(CC))
endif

#
# Name of the binary code directories
#
ifeq ($(OS_ARCH)_$(PROCESSOR_ARCHITECTURE),WINNT_x86)
OBJDIR_NAME	= $(OS_CONFIG)$(OS_VERSION)$(OBJDIR_TAG).OBJ
else
OBJDIR_NAME	= $(OS_CONFIG)$(OS_VERSION)$(PROCESSOR_ARCHITECTURE)$(COMPILER)$(IMPL_STRATEGY)$(OBJDIR_TAG).OBJ
endif

# Figure out where the binary code lives. It either lives in the src
# tree (NSBUILDROOT is undefined) or somewhere else.
ifdef NSBUILDROOT
BUILD		= $(NSBUILDROOT)/$(OBJDIR_NAME)/build
OBJDIR		= $(BUILD)/$(SRCDIR)
XPDIST		= $(NSBUILDROOT)
DIST		= $(NSBUILDROOT)/$(OBJDIR_NAME)/dist
else
BUILD		= $(OBJDIR_NAME)
OBJDIR		= $(OBJDIR_NAME)
XPDIST		= $(DEPTH)/dist
DIST		= $(DEPTH)/dist/$(OBJDIR_NAME)
endif

# all public include files go in subdirectories of PUBLIC:
PUBLIC		= $(XPDIST)/public

VPATH		= $(OBJDIR)
DEPENDENCIES	= $(OBJDIR)/.md

ifneq ($(OS_ARCH),WINNT)
MKDEPEND_DIR	= $(DEPTH)/config/mkdepend
MKDEPEND	= $(MKDEPEND_DIR)/$(OBJDIR_NAME)/mkdepend
MKDEPENDENCIES	= $(OBJDIR)/depend.mk
endif

#
# Include any personal overrides the user might think are needed.
#
-include $(MY_CONFIG)

######################################################################
# Now test variables that might have been set or overridden by $(MY_CONFIG).

DEFINES		+= -DNETSCAPE -DOSTYPE=\"$(OS_CONFIG)\"

# Specify that we are building a client.
# This will instruct the cross platform libraries to
# include all the client specific cruft.
ifdef SERVER_BUILD
DEFINES		+= -DSERVER_BUILD
ifdef LIVEWIRE
DEFINES		+= -DLIVEWIRE
endif
STATIC_JAVA	= yes
else
MOZILLA_CLIENT	= 1
DEFINES		+= -DMOZILLA_CLIENT
endif

ifdef MOZ_LITE
NO_EDITOR	= 1
NO_UNIX_LDAP	= 1
MOZ_JSD		= 1
MOZ_NAV_BUILD_PREFIX = 1
endif
ifdef MOZ_MEDIUM
DEFINES		+= -DNO_SECURITY -DEDITOR -DMOZ_COMMUNICATOR_IIDS
EDITOR		= 1
MOZ_JSD		= 1
MOZ_COMMUNICATOR_IIDS	= 1
MOZ_COMMUNICATOR_CONFIG_JS	= 1
MOZ_COPY_ALL_JARS	= 1
NO_SECURITY	= 1
endif
ifdef MOZ_GOLD
DEFINES		+= -DGOLD
BUILD_GOLD	= 1
endif


ifdef EDITOR
DEFINES		+= -DEDITOR -DEDITOR_UI
BUILD_EDITOR	= 1
BUILD_EDITOR_UI	= 1
BUILD_EDT	= 1
endif

# Build layers by default
ifndef NO_LAYERS
DEFINES		+= -DLAYERS
endif

ifdef BUILD_DEBUG_GC
DEFINES		+= -DDEBUG_GC
endif

ifdef BUILD_UNIX_PLUGINS
# UNIX_EMBED Should not be needed. For now these two defines go
# together until I talk with jg.  --dp
DEFINES		+= -DUNIX_EMBED -DX_PLUGINS
endif

ifndef NO_UNIX_LDAP
DEFINES		+= -DUNIX_LDAP
endif

#
# Platform dependent switching off of NSPR, JAVA and MOCHA
#
ifndef NO_NSPR
DEFINES		+= -DNSPR
endif

ifdef MOZ_JAVA
DEFINES		+= -DJAVA
endif

ifndef NO_MOCHA
DEFINES		+= -DMOCHA
endif

ifdef FORTEZZA
DEFINES		+= -DFORTEZZA
endif

ifdef UNIX_SKIP_ASSERTS
DEFINES		+= -DUNIX_SKIP_ASSERTS
endif

ifdef SHACK
DEFINES		+= -DSHACK
endif

ifndef NO_UNIX_ASYNC_DNS
DEFINES		+= -DUNIX_ASYNC_DNS
endif

# For profiling
ifdef MOZILLA_GPROF
# Don't want profiling on build tools..
ifneq ($(SRCDIR),config)
PROF_FLAGS	= $(OS_GPROF_FLAGS) -DMOZILLA_GPROF
endif
endif

# This compiles in heap dumping utilities and other good stuff 
# for developers -- maybe we only want it in for a special SDK 
# nspr/java runtime(?):
DEFINES		+= -DDEVELOPER_DEBUG

#
# For the standalone image lib
#

ifdef STANDALONE_IMAGE_LIB
DEFINES		+= -DSTANDALONE_IMAGE_LIB
endif

######################################################################

GARBAGE		= $(DEPENDENCIES) core $(wildcard core.[0-9]*)

ifndef SDK
SDK		= $(DEPTH)/dist/sdk
endif

ifneq ($(OS_ARCH),WINNT)
NSINSTALL	= $(DEPTH)/config/$(OBJDIR_NAME)/nsinstall

ifeq ($(NSDISTMODE),copy)
# copy files, but preserve source mtime
INSTALL		= $(NSINSTALL) -t
else
ifeq ($(NSDISTMODE),absolute_symlink)
# install using absolute symbolic links
INSTALL		= $(NSINSTALL) -L `$(NFSPWD)`
else
# install using relative symbolic links
INSTALL		= $(NSINSTALL) -R
endif
endif
endif

######################################################################
### Java Stuff - see common.mk
######################################################################

# where the bytecode will go
JAVA_DESTPATH	= $(XPDIST)/classes

# where the sources for the module you are compiling are
# default is sun-java/classsrc, override for other modules
ifndef JAVA_SOURCEPATH
JAVA_SOURCEPATH	= $(DEPTH)/sun-java/classsrc
endif

ifndef JAVAH_IN_JAVA
ifeq ($(OS_ARCH),OS2)
JAVAH_PROG	= flipper $(DIST)/bin/javah
else
JAVAH_PROG	= $(DIST)/bin/javah
endif
else
JAVAH_PROG	= $(JAVA) netscape.tools.jric.Main
endif

ifeq ($(STAND_ALONE_JAVA),1)
STAND_ALONE_JAVA_DLL_SUFFIX	= s
endif

ifeq ($(OS_ARCH),OS2)
AWTDLL		= awt$(MOZ_BITS)$(VERSION_NUMBER).$(DLL_SUFFIX)
AWTSDLL		= awt$(MOZ_BITS)$(VERSION_NUMBER)$(STAND_ALONE_JAVA_DLL_SUFFIX).$(DLL_SUFFIX)
CONDLL		= con.$(MOZ_BITS)$(VERSION_NUMBER)(DLL_SUFFIX)
JBNDLL		= jbn.$(MOZ_BITS)$(VERSION_NUMBER)(DLL_SUFFIX)
JDBCDLL		= jdb.$(MOZ_BITS)$(VERSION_NUMBER)(DLL_SUFFIX)
JITDLL		= jit.$(MOZ_BITS)$(VERSION_NUMBER)(DLL_SUFFIX)
JPWDLL		= jpw.$(MOZ_BITS)$(VERSION_NUMBER)(DLL_SUFFIX)
JRTDLL		= jrt$(MOZ_BITS)$(VERSION_NUMBER).$(DLL_SUFFIX)
JSJDLL		= jsj.$(MOZ_BITS)$(VERSION_NUMBER)(DLL_SUFFIX)
MMDLL		= mm$(MOZ_BITS)$(VERSION_NUMBER).$(DLL_SUFFIX)
NETDLL		= net.$(MOZ_BITS)$(VERSION_NUMBER)(DLL_SUFFIX)
NSCDLL		= nsc.$(MOZ_BITS)$(VERSION_NUMBER)(DLL_SUFFIX)
ZIPDLL		= zip.$(MOZ_BITS)$(VERSION_NUMBER)(DLL_SUFFIX)
ZPWDLL		= zpw.$(MOZ_BITS)$(VERSION_NUMBER)(DLL_SUFFIX)
else
AWTDLL		= libawt.$(DLL_SUFFIX)
AWTSDLL		= libawt$(STAND_ALONE_JAVA_DLL_SUFFIX).$(DLL_SUFFIX)
CONDLL		= libcon.$(DLL_SUFFIX)
JBNDLL		= libjbn.$(DLL_SUFFIX)
JDBCDLL		= libjdb.$(DLL_SUFFIX)
JITDLL		= libjit.$(DLL_SUFFIX)
JPWDLL		= libjpw.$(DLL_SUFFIX)
JRTDLL		= libjrt.$(DLL_SUFFIX)
JSJDLL		= libjsj.$(DLL_SUFFIX)
MMDLL		= libmm.$(DLL_SUFFIX)
NETDLL		= libnet.$(DLL_SUFFIX)
NSCDLL		= libnsc.$(DLL_SUFFIX)
ZIPDLL		= libzip.$(DLL_SUFFIX)
ZPWDLL		= libzpw.$(DLL_SUFFIX)
endif

JAVA_DEFINES	+= -DAWTSDLL=\"$(AWTSDLL)\" -DCONDLL=\"$(CONDLL)\" -DJBNDLL=\"$(JBNDLL)\" -DJDBDLL=\"$(JDBDLL)\" \
		   -DJSJDLL=\"$(JSJDLL)\" -DNETDLL=\"$(NETDLL)\" -DNSCDLL=\"$(NSCDLL)\" -DZPWDLL=\"$(ZPWDLL)\"

