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

#
# config.mk
#
# Determines the platform and builds the macros needed to load the
# appropriate platform-specific .mk file, then defines all (most?)
# of the generic macros.
#

# Define an include-at-most-once flag
INCLUDED_CONFIG_MK = 1

ifndef topsrcdir
topsrcdir	= $(DEPTH)
endif

ifndef INCLUDED_AUTOCONF_MK
include $(DEPTH)/config/autoconf.mk
endif
ifndef INCLUDED_INSURE_MK
ifdef MOZ_INSURIFYING
include $(topsrcdir)/config/insure.mk
endif
endif

ifndef INCLUDED_COMMON_MK
include $(topsrcdir)/config/common.mk
endif

BUILD_TOOLS	= $(topsrcdir)/build/unix
CONFIG_TOOLS	= $(DEPTH)/config
AUTOCONF_TOOLS	= $(topsrcdir)/build/autoconf

PERL            = perl

#
# Tweak the default OS_ARCH and OS_RELEASE macros as needed.
#
ifeq ($(OS_ARCH),AIX)
OS_RELEASE	:= $(shell uname -v).$(shell uname -r)
endif
ifeq ($(OS_ARCH),BSD_386)
OS_ARCH		:= BSD_OS
endif
ifeq ($(OS_ARCH),dgux)
OS_ARCH		:= DGUX
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
ifeq ($(OS_ARCH),OSF1)
OS_SUB		:= $(shell uname -v)
# Until I know the other possibilities, or an easier way to compute them, this is all there's gonna be.
#ifeq ($(OS_SUB),240)
#OS_RELEASE	:= V2.0
#endif
ifeq ($(OS_SUB),148)
OS_RELEASE	:= V3.2C
endif
ifeq ($(OS_SUB),564)
OS_RELEASE	:= V4.0B
endif
ifeq ($(OS_SUB),878)
OS_RELEASE	:= V4.0D
endif
endif
ifneq (,$(findstring OpenVMS,$(OS_ARCH)))
OS_ARCH		:= OpenVMS
OS_RELEASE	:= $(shell uname -v)
CPU_ARCH	:= $(shell uname -Wh)
CPU_ARCH_TAG	:= _$(CPU_ARCH)
PERL		:= perl
endif
ifeq ($(OS_ARCH),QNX)
ifeq ($(OS_TARGET),NTO)
LD		:= qcc -Vgcc_ntox86 -nostdlib
else
OS_RELEASE	:= $(shell uname -v | sed 's/^\([0-9]\)\([0-9]*\)$$/\1.\2/')
LD		:= $(CC)
endif
OS_TEST		:= x86
endif
ifeq ($(OS_ARCH),SCO_SV)
OS_ARCH		:= SCOOS
OS_RELEASE	:= 5.0
endif
ifneq (,$(filter SINIX-N SINIX-Y SINIX-Z ReliantUNIX-M,$(OS_ARCH)))
OS_ARCH		:= SINIX
OS_TEST		:= $(shell uname -p)
endif
ifeq ($(OS_ARCH),UnixWare)
OS_ARCH		:= UNIXWARE
OS_RELEASE	:= $(shell uname -v)
endif
ifeq ($(OS_ARCH),OS_2)
OS_ARCH		:= OS2
OS_RELEASE	:= $(shell uname -v)
endif
ifeq ($(OS_ARCH),Mac OS)
ifeq ($(OS_RELEASE),10.0)
OS_ARCH		:= Rhapsody
endif
endif

#
# Strip off the excessively long version numbers on these platforms,
# but save the version to allow multiple versions of the same base
# platform to be built in the same tree.
#
ifneq (,$(filter FreeBSD HP-UX IRIX Linux NetBSD OpenBSD OSF1 SunOS,$(OS_ARCH)))
OS_RELEASE	:= $(basename $(OS_RELEASE))

# Allow the user to ignore the OS_VERSION, which is usually irrelevant.
ifdef WANT_MOZILLA_CONFIG_OS_VERSION
OS_VERS		:= $(suffix $(OS_RELEASE))
OS_VERSION	:= $(shell echo $(OS_VERS) | sed 's/-.*//')
endif

endif

OS_CONFIG	:= $(OS_ARCH)$(OS_RELEASE)

# determine debug-related options
DEBUG_FLAGS :=

ifndef MOZ_DEBUG
  #global debugging is disabled - check if it was explicitly enabled for this module
  ifneq (, $(findstring $(MODULE), $(MOZ_DEBUG_MODULES)))
    MOZ_DEBUG:=1
endif
else
  #global debugging is enabled - check if it was explicitly disabled for this module
  ifneq (, $(findstring ^$(MODULE), $(MOZ_DEBUG_MODULES)))
    MOZ_DEBUG:=
  endif
endif

ifdef MOZ_DEBUG
  DEBUG_FLAGS += $(MOZ_DEBUG_ENABLE_DEFS)
else
  DEBUG_FLAGS += $(MOZ_DEBUG_DISABLE_DEFS)
endif

ifdef MOZ_DEBUG
  # debugging is enabled - determine if -g should be passed to the compiler, based on
  # the current module, and the value of MOZ_DBGRINFO_MODULES

  pattern:=ALL_MODULES ^ALL_MODULES
  ifdef MODULE
    # our current Makefile specifies a module name - add it to our pattern
    pattern += $(MODULE) ^$(MODULE)
  endif

  # start by finding the first relevant module name (remember that the order of the
  # module names in MOZ_DBGRINFO_MODULES is reversed from the order the user
  # specified to configure - this allows the user to put general names at the beginning
  # of the list, and to override them with explicit module names later in the list)
  first_match:=$(firstword $(filter $(pattern), $(MOZ_DBGRINFO_MODULES)))
  ifeq ($(first_match), $(MODULE))
    # the user specified explicitly that this module should be compiled with -g
    DEBUG_FLAGS += $(MOZ_DEBUG_FLAGS)
  else
    ifeq ($(first_match), ^$(MODULE))
      # the user specified explicitly that this module should not be compiled with -g
      # (nothing to do)
    else
      ifeq ($(first_match), ALL_MODULES)
        # the user didn't mention this module explicitly, but wanted all modules to be
        # compiled with -g
        DEBUG_FLAGS += $(MOZ_DEBUG_FLAGS)
      else
        ifeq ($(first_match), ^ALL_MODULES)
          # the user didn't mention this module explicitly, but wanted all modules to be
          # compiled without -g (nothing to do)
        endif
      endif
    endif
  endif
endif

# append debug flags (these might have been above when processing
# MOZ_DBGRINFO_MODULES)
OS_CFLAGS += $(DEBUG_FLAGS)
OS_CXXFLAGS += $(DEBUG_FLAGS)

#
# Personal makefile customizations go in these optional make include files.
#
MY_CONFIG	:= $(DEPTH)/config/myconfig.mk
MY_RULES	:= $(DEPTH)/config/myrules.mk

#
# Relative pathname from top-of-tree to current source directory
#
ifneq ($(OS_ARCH),OS2)
REVDEPTH	= $(CONFIG_TOOLS)/revdepth
endif

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
else
NS_LIB		:= .
JAVA_LIB	:= .
endif

# Allow NETSCAPE_COMMERCIAL to include XFEPRIVDIR
ifdef NETSCAPE_COMMERCIAL
XFEPRIVDIR		:= $(DEPTH)/../ns/cmd/xfe/
endif

#
# Default command macros; can be overridden in <arch>.mk.
#
CCC		= $(CXX)
CCF		= $(CC) $(CFLAGS)
LINK_EXE	= $(LINK) $(OS_LFLAGS) $(LFLAGS)
LINK_DLL	= $(LINK) $(OS_DLLFLAGS) $(DLLFLAGS)
NFSPWD		= $(CONFIG_TOOLS)/nfspwd
PURIFY		= purify $(PURIFYOPTIONS)
QUANTIFY	= quantify $(QUANTIFYOPTIONS)
MOC		= moc
ifdef CROSS_COMPILE
XPIDL_COMPILE 	= $(DIST)/host/bin/host_xpidl$(BIN_SUFFIX)
XPIDL_LINK	= $(DIST)/host/bin/host_xpt_link$(BIN_SUFFIX)
else
XPIDL_COMPILE 	= $(DIST)/bin/xpidl$(BIN_SUFFIX)
XPIDL_LINK	= $(DIST)/bin/xpt_link$(BIN_SUFFIX)
endif

ifeq ($(OS_ARCH),OS2)
PATH_SEPARATOR	:= \;
else
PATH_SEPARATOR	:= :
ifeq ($(AWT_11),1)
JAVA_PROG	= $(NS_BIN)java
JAVAC_ZIP	= $(NS_LIB)/classes.zip
else
JAVA_PROG	= $(LOCAL_BIN)java
ifdef JDKHOME
JAVAC_ZIP	= $(JAVA_LIB)/classes.zip
else
JAVAC_ZIP	= $(JAVA_LIB)/javac.zip
endif
endif
TAR		= tar
endif # OS2

ifeq ($(OS_ARCH),OpenVMS)
include $(topsrcdir)/config/$(OS_ARCH).mk
endif

XBCFLAGS	=
ifdef MOZ_DEBUG
JAVA_OPTIMIZER	= -g
XBCFLAGS	= -FR$*
endif

INCLUDES	= $(LOCAL_INCLUDES) -I$(PUBLIC) $(OS_INCLUDES)

LIBNT		= $(DIST)/lib/libnt.$(LIB_SUFFIX)
LIBAWT		= $(DIST)/lib/libawt.$(LIB_SUFFIX)
LIBMMEDIA	= $(DIST)/lib/libmmedia.$(LIB_SUFFIX)

NSPRDIR		= nsprpub
LIBNSPR		= $(DIST)/lib/libplds3.$(LIB_SUFFIX) $(DIST)/lib/libnspr3.$(LIB_SUFFIX)
PURELIBNSPR	= $(DIST)/lib/purelibplds3.$(LIB_SUFFIX) $(DIST)/lib/purelibnspr3.$(LIB_SUFFIX)

ifdef DBMALLOC
LIBNSPR		+= $(DIST)/lib/libdbmalloc.$(LIB_SUFFIX)
endif

ifeq ($(OS_ARCH),OS2)
ifneq ($(MOZ_WIDGET_TOOLKIT), os2)
LIBNSJAVA	= $(DIST)/lib/jrt$(MOZ_BITS)$(VERSION_NUMBER).$(LIB_SUFFIX)
LIBMD		= $(DIST)/lib/libjmd.$(LIB_SUFFIX)
LIBJAVA		= $(DIST)/lib/libjrt.$(LIB_SUFFIX)
LIBNSPR		= $(DIST)/lib/pr$(MOZ_BITS)$(VERSION_NUMBER).$(LIB_SUFFIX)
LIBXP		= $(DIST)/lib/libxp.$(LIB_SUFFIX)
endif
else
LIBNSJAVA	= $(DIST)/lib/nsjava32.$(LIB_SUFFIX)
endif

CFLAGS		= $(OS_CFLAGS)
CXXFLAGS	= $(OS_CXXFLAGS)

COMPILE_CFLAGS	= $(DEFINES) $(INCLUDES) $(XCFLAGS) $(PROF_FLAGS) $(DSO_CFLAGS) $(DSO_PIC_CFLAGS) $(CFLAGS) $(OS_COMPILE_CFLAGS)
COMPILE_CXXFLAGS = $(DEFINES) $(INCLUDES) $(XCFLAGS) $(PROF_FLAGS) $(DSO_CFLAGS) $(DSO_PIC_CFLAGS)  $(CXXFLAGS) $(OS_COMPILE_CXXFLAGS)

LDFLAGS		= $(OS_LDFLAGS)

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
# Override defaults

# We need to know where to find the libraries we
# put on the link line for binaries, and should
# we link statically or dynamic?  Assuming dynamic for now.
LIBS_DIR	= -L$(DIST)/bin -L$(DIST)/lib

# Default location of include files
# Note: NSPR doesn't have $(PUBLIC) and 
# will always install its headers to $(DIST)/include
PUBLIC		= $(DIST)/include

DEPENDENCIES	= .md

MOZ_COMPONENT_LIBS=$(MOZ_COMPONENT_NSPR_LIBS) $(MOZ_COMPONENT_XPCOM_LIBS)

ifdef GC_LEAK_DETECTOR
MOZ_COMPONENT_XPCOM_LIBS += -lboehm
XPCOM_LIBS += -lboehm
endif

ifneq (all,$(BUILD_MODULES))
DEFINES += -DXPCOM_STANDALONE -DXPCONNECT_STANDALONE
endif

ifeq ($(OS_ARCH),OS2)
ELF_DYNSTR_GC	= echo
else
ELF_DYNSTR_GC	= :
endif

ifdef USE_ELF_DYNSTR_GC
ifdef MOZ_COMPONENTS_VERSION_SCRIPT_LDFLAGS
ELF_DYNSTR_GC 	= $(DIST)/bin/elf-gc-dynstr
endif
endif

ifneq ($(OS_ARCH),WINNT)

ifdef MOZ_NATIVE_MAKEDEPEND
MKDEPEND_DIR	=
# Adding the '-w' flag shortens the depend.mk files by allowing
# more dependencies on one line. It may even speed up makedepend.
# (Picking 3000 somewhat arbitrarily.)
MKDEPEND	= $(MOZ_NATIVE_MAKEDEPEND) -Y -w 3000
else
MKDEPEND_DIR	= $(DEPTH)/config/mkdepend
MKDEPEND	= $(MKDEPEND_DIR)/mkdepend
endif

MKDEPENDENCIES	= depend.mk

endif

#
# Include any personal overrides the user might think are needed.
#
-include $(MY_CONFIG)

######################################################################
# Now test variables that might have been set or overridden by $(MY_CONFIG).

DEFINES		+= -DOSTYPE=\"$(OS_CONFIG)\"

ifdef MOZ_SECURITY
DEFINES		+= -DMOZ_SECURITY
endif

#
# Platform dependent switching off of JAVA
#

ifdef MOZ_JAVA
DEFINES		+= -DJAVA
ifdef MOZ_OJI
error You can't define both MOZ_JAVA and MOZ_OJI anymore. 
endif
JAVA_OR_OJI	= 1
JAVA_OR_NSJVM	= 1
endif

ifdef NSJVM
JAVA_OR_NSJVM	= 1
endif

ifdef MOZ_OJI
DEFINES		+= -DOJI
JAVA_OR_OJI	= 1
endif

ifdef JAVA_OR_NSJVM	# XXX fix -- su can't depend on java
MOZ_SMARTUPDATE	= 1
endif

ifdef FORTEZZA
DEFINES		+= -DFORTEZZA
endif

# For profiling
ifdef MOZILLA_GPROF
# Don't want profiling on build tools..
ifneq ($(shell echo $(srcdir) | sed 's:\.\./::g'),config)
PROF_FLAGS	= $(OS_GPROF_FLAGS) -DMOZILLA_GPROF
endif
endif

######################################################################

GARBAGE		+= $(DEPENDENCIES) $(MKDEPENDENCIES) $(MKDEPENDENCIES).bak core $(wildcard core.[0-9]*) $(wildcard *.err) $(wildcard *.pure) $(wildcard *_pure_*.o) Templates.DB

ifneq (,$(filter-out WINNT, $(OS_ARCH)))
ifdef CROSS_COMPILE
NSINSTALL	= $(CONFIG_TOOLS)/host_nsinstall
else
NSINSTALL	= $(CONFIG_TOOLS)/nsinstall
endif

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
JAVA_DESTPATH	= $(DIST)/classes

# where the sources for the module you are compiling are
# default is sun-java/classsrc, override for other modules
ifndef JAVA_SOURCEPATH
JAVA_SOURCEPATH	= $(DEPTH)/sun-java/classsrc
endif

ifndef JAVAH_IN_JAVA
ifeq ($(MOZ_OS2_TOOLS),VACPP)
JAVAH_PROG	= flipper $(DIST)/bin/javah
else
JAVAH_PROG	= $(DIST)/bin/javah
endif
else
JAVAH_PROG	= $(JAVA) netscape.tools.jric.Main
endif

ifneq ($(JDKHOME),)
JAVAH_PROG	= $(JDKHOME)/bin/javah
JAVAC_PROG		= $(JDKHOME)/bin/javac $(JAVAC_FLAGS)
JAVAC 			= $(JAVAC_PROG)
endif

ifeq ($(STAND_ALONE_JAVA),1)
STAND_ALONE_JAVA_DLL_SUFFIX	= s
endif

ifeq ($(MOZ_OS2_TOOLS),OLD_IBM_BUILD) # These DLL names are no longer valid for OS/2
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
		   -DJSJDLL=\"$(JSJDLL)\" -DNETDLL=\"$(NETDLL)\" -DNSCDLL=\"$(NSCDLL)\" -DZPWDLL=\"$(ZPWDLL)\" \
		   -DJAR_NAME=\"$(JAR_NAME)\"

ifeq ($(AWT_11),1)
JAVA_DEFINES	+= -DAWT_11
else
JAVA_DEFINES	+= -DAWT_102
endif

