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
#
##########################################################################
#
# Makefile for the Mozilla X front end.
#
# There are N dimensions to a full complement of netscape binaries:
#
#	- target architecture
#	- debug vs optimize
#	- normal vs vendor-cobranded version (number, anim, etc)
#	- normal vs purified
#
# A directory is needed for each of the first two dimensions (target
# and debug) but executables for all of the others will then be built
# in the same directory from the same .o files.  The files have names
# as follows:
#
#	netscape-export			2.0
#	netscape-nis-export		2.0  (SunOS 4 only)
#	netscape-sgi-export		2.0S (Irix5 only)
#	netscape-export.pure		2.0  (SunOS 4 and Solaris only)
#	 ...etc...
#
# The only difference in the executables is which version of config-*.o
# is linked in, and whether or not -lresolv is used.
#
# The "release" target will create a release/ subdirectory, and N subdirs
# of that in which the various tar files will go.  The tar files will be
# uniquely named, though the executables in them are all called "netscape".
#
# The exception to this is that the -nis versions don't get their own tar
# files, they are lumped in with the normal SunOS 4 package with the names
# "netscape" (for the NIS version) and "netscape_dns" (for the -lresolv
# version.)
#

DEPTH		= ../..

MODULE		= moz

USE_3PANE	= 1
USE_SPELLCHECK	= 1

REQUIRES	= addr \
		  applet \
		  img \
		  edtplug \
		  jtools \
		  lay \
		  layer \
		  js \
		  libfont \
		  mariner \
		  msg \
		  $(NULL)
ifdef NSPR20
REQUIRES          += plds \
		  nspr20 \
		  $(NULL)
else
REQUIRES          += nspr \
		  $(NULL)
endif
REQUIRES          += parse \
		  plug \
		  hook \
		  pref \
		  rdf \
		  security \
		  softupdt \
		  libreg \
		  style \
		  util \
		  java \
		  ldap \
		  $(NULL)

CSRCS		= \
		  main.c \
		  stubbm.c \
		  stubctxt.c \
		  stubdlgs.c \
		  stubedit.c \
		  stubenc.c \
		  stubform.c \
		  stubgrid.c \
		  stubhelp.c \
		  stubimg.c \
		  stublay.c \
		  stubloc.c \
		  stubmail.c \
		  stubplug.c \
		  stubps.c \
		  stubrdf.c \
		  stubrgn.c \
		  stubsec.c \
		  stubstr.c \
		  stubtime.c \
		  $(NULL)

ifndef MOZ_LITE
XFE_PROGNAME	:= mozilla
DSO_PROGNAME	:= moz
PRODUCT_NAME	:= Communicator
NETHELP_DIR	:= comm
else
XFE_PROGNAME	:= mozilla-lite
DSO_PROGNAME	:= moz-lite
PRODUCT_NAME	:= Navigator
NETHELP_DIR	:= ratbert
CSRCS		+= rat_stubs.c
endif

X_RELEASES	= release-YYY \
		  release-net-YYY \
		  release-sgi-YYY \
		  $(NULL)

EXPORT_RELEASES	= $(subst YYY,export,$(X_RELEASES))
FRANCE_RELEASES	= $(subst YYY,france,$(X_RELEASES))
US_RELEASES	= $(subst YYY,us,$(X_RELEASES))

RELEASES	= $(EXPORT_RELEASES) $(FRANCE_RELEASES) $(US_RELEASES)

GUESS_CONFIG	:= $(shell $(DEPTH)/config/config.guess | sed 's/i[23456]86/x86/')

ifdef BUILD_INTL
README_FILE	= $(DEPTH)/l10n/us/xfe/README.intl
else
README_FILE	= $(OBJDIR)/README
endif

BUILD_NUMBER	= $(DEPTH)/build/build_number
LICENSE_DIR	= $(DEPTH)/l10n/us/xp

LICENSES	= $(LICENSE_DIR)/LICENSE-alpha \
		  $(LICENSE_DIR)/LICENSE-export-beta \
		  $(LICENSE_DIR)/LICENSE-export \
		  $(NULL)

COMMON_REL_DEPS	= versionn.h \
		  make-release \
		  $(OBJDIR)/LICENSE \
		  $(EXTRA_REL_FILES) \
		  $(TEST_PLUGIN) \
		  $(AUTOADMIN_DSO) \
		  $(README_FILE) \
		  $(DEPTH)/tpubs/$(NETHELP_DIR)/nethelp.tar.gz \
		  $(DIST)/bin/zigbert$(BIN_SUFFIX)

ifdef USE_SPELLCHECK
COMMON_REL_DEPS	+= \
		  $(DEPTH)/modules/spellchk/data/spellchk.tar.gz \
		  $(NULL)
endif

MOZILLA_ZIP	= java/applet \
		  java/awt \
		  java/awt/image \
		  java/awt/peer \
		  java/io \
		  java/lang \
		  java/lang/reflect \
		  java/net \
		  java/util \
		  java/text \
		  java/text/resources \
		  netscape/context \
		  netscape/applet \
		  netscape/console \
		  netscape/debug \
		  netscape/plugin \
		  netscape/javascript \
		  netscape/util \
		  netscape/application \
		  netscape/constructor \
		  netscape/security \
		  netscape/softupdate \
		  netscape/net \
		  netscape/misc \
		  netscape/ldap/ber/stream \
		  netscape/ldap/util \
		  netscape/ldap/client/opers \
		  netscape/ldap/client \
		  netscape/ldap \
		  sun/io \
		  sun/audio \
		  sun/awt \
		  sun/awt/image \
		  sun/awt/motif \
		  sun/misc/Cache.class \
		  sun/misc/Ref.class \
		  META-INF \
		  $(NULL)

ALL_CLASS_DEPS	= $(shell find $(XPDIST)/classes -type f \( -name "*.class" -o -name "*.gif" -o -name "*.mf" -o -name "*.sf" -o -name "*.rsa" -o -name "*.properties*" \) -print)

#######################################################################

include $(DEPTH)/config/config.mk

CCLD		= $(CCC)
LDFLAGS		= $(CFLAGS)
NOMD_LDFLAGS	= $(NOMD_CFLAGS)

ifdef MOZILLA_GPROF
CSRCS		+= gmon.c
LDFLAGS		= $(OPTIMIZER)
$(OBJDIR)/gmon.o:	gmon.c gmon.h
	$(CC) -O -c -o $@ $<
endif

ifeq ($(OS_ARCH),HP-UX)

# HP requires C++ programs to have main() in a C++ file.
CPPSRCS		= cplusplusmain.cc
CFLAGS		+= -DCPLUSPLUS_LINKAGE

ifeq ($(OS_RELEASE),B.10)
HPUX_EXTRA_DIRS	= Xmu
endif 

endif

OBJS		= $(CSRCS:.c=.o) $(CPPSRCS:.cc=.o)

GARBAGE		+= $(wildcard java_40*) e_kit_resources.h cxxlink-filter

ifdef FORTEZZA
CI_LIB		= $(DIST)/lib/libci.a
else
CI_LIB		=
endif

SEC_LIBS	= $(DIST)/lib/lib$(LITE_PREFIX)secnav.a \
		  $(DIST)/lib/libssl.a \
		  $(DIST)/lib/libcert.a \
		  $(DIST)/lib/libpkcs12.a \
		  $(DIST)/lib/libpkcs7.a \
		  $(DIST)/lib/libsecmod.a \
		  $(DIST)/lib/libkey.a \
		  $(DIST)/lib/libcrypto.a \
		  $(DIST)/lib/libsecutil.a \
		  $(DIST)/lib/libhash.a \
		  $(DIST)/lib/libhtmldlgs.a \
		  $(NULL)

ifndef MOZ_LITE
SEC_LIBS	+= $(DIST)/lib/libldap.a $(DIST)/lib/liblber.a
endif

SEC_LIBS	+= $(CI_LIB)

US_LIB		= $(SEC_LIBS) $(DIST)/lib/libdomestic.a
EXPORT_LIB	= $(SEC_LIBS) $(DIST)/lib/libexport.a 
FRANCE_LIB	= $(SEC_LIBS) $(DIST)/lib/libfrance.a 

ifeq ($(OS_ARCH),AIX)
NSPR_LIB	= 
else
ifdef NSPR20
NSPR_LIB	= $(DIST)/lib/libplds21.a
NSPR_LIB	+= $(DIST)/lib/libplc21.a
NSPR_LIB	+= $(DIST)/lib/libmsgc21.a
NSPR_LIB	+= $(DIST)/lib/libnspr21.a
else
NSPR_LIB	= $(DIST)/lib/libnspr.a
endif
endif

ifdef DBMALLOC
NSPR_LIB	+= $(DIST)/lib/libdbmalloc.a
endif

ifndef DISABLE_MARINER
CFLAGS		+= -DENABLE_MARINER
endif

#
# We need libnet.a in there twice because libmsg and libnet have circular
# dependencies on functions.
#
BASIC_LIBS	= $(DIST)/lib/lib$(LITE_PREFIX)net.a \
		  $(DIST)/lib/lib$(LITE_PREFIX)lay.a \
		  $(DIST)/lib/libimg.a \
		  $(DIST)/lib/libpng.a \
		  $(DIST)/lib/libli.a \
		  $(DIST)/lib/libjmc.a \
		  $(DIST)/lib/libjpeg.a \
		  $(DIST)/lib/libhook.a \
		  $(DIST)/lib/libparse.a \
		  $(DIST)/lib/lib$(LITE_PREFIX)pref.a \
		  $(DIST)/lib/lib$(LITE_PREFIX)i18n.a \
		  $(DIST)/lib/libpics.a \
		  $(DIST)/lib/libpwcac.a \
		  $(DIST)/lib/libdbm.a

ifndef MOZ_LITE
BASIC_LIBS	+= $(DIST)/lib/libmsg.a \
		  $(DIST)/lib/libnet.a \
		  $(DIST)/lib/libmisc.a \
		  $(DIST)/lib/libaddr.a \
		  $(DIST)/lib/libneo.a \
		  $(DIST)/lib/libmime.a \
		  $(NULL)
else
BASIC_LIBS	+= $(DIST)/lib/lib$(LITE_PREFIX)misc.a
endif

BASIC_LIBS	+= $(DIST)/lib/libxlate.a \
		   $(DIST)/lib/lib$(LITE_PREFIX)plug.a \
		   $(DIST)/lib/libutil.a \
		   $(NULL)

ifndef MOZ_LITE
ifdef USE_SPELLCHECK_STATIC
BASIC_LIBS	+= $(DIST)/lib/libspellchk.a
endif
endif

BASIC_LIBS	+= $(DIST)/lib/libfont.a \
		  $(DIST)/lib/libprgrss.a

ifndef NO_LAYERS
BASIC_LIBS	+= $(DIST)/lib/liblayer.a
endif

ifndef NO_JAVA


BASIC_LIBS	+= $(DIST)/lib/lib$(LITE_PREFIX)applet.a \
		  $(DIST)/lib/libjrt.a \
		  $(DIST)/lib/libjmd.a \
		  $(NULL)

ifndef MOZ_LITE
BASIC_LIBS	+= $(DIST)/lib/libedtplug.a
endif

BASIC_LIBS	+= $(DIST)/lib/libnsn.a \
		  $(DIST)/lib/libjsl.a \
		  $(DIST)/lib/libnsc.a \
		  $(DIST)/lib/libjpw.a \
		  $(DIST)/lib/libzpw.a \
		  $(DIST)/lib/libreg.a \
		  $(DIST)/lib/libmmedia.a \
		  $(DIST)/lib/libsoftupdate.a \
		  $(DIST)/lib/libcon.a \
		  $(DIST)/lib/libjbn.a \
		  $(DIST)/lib/lib$(LITE_PREFIX)xp.a \
		  $(DIST)/lib/librdf.a \
		  $(DIST)/lib/libmariner.a \
		  $(NULL)

BASIC_LIBS	+= $(DIST)/lib/libzlib.a \
		$(NULL)
endif

ifndef NO_MOCHA
BASIC_LIBS	+= $(DIST)/lib/libjs.a $(DIST)/lib/libjsj.a $(DIST)/lib/libmocha.a $(DIST)/lib/libimg.a \
		   $(DIST)/lib/lib$(LITE_PREFIX)lay.a
endif

# Post-Java libs
BASIC_LIBS	+= $(DIST)/lib/libstyle.a $(DIST)/lib/libjar.a

ifndef NO_UNIX_LDAP
BASIC_LIBS	+= $(DIST)/lib/libldap.a $(DIST)/lib/liblber.a 
endif

LOCALES		= $(LOCALE_MAP) $(MAIL_IM_HACK) $(NEWS_IM_HACK)

ALL_US_LIBS	= $(BASIC_LIBS) $(US_LIB) $(BASIC_LIBS_2) $(NSPR_LIB)
ALL_EXPORT_LIBS	= $(BASIC_LIBS) $(EXPORT_LIB) $(BASIC_LIBS_2) $(NSPR_LIB)
ALL_FRANCE_LIBS	= $(BASIC_LIBS) $(FRANCE_LIB) $(BASIC_LIBS_2) $(NSPR_LIB)

ALL_US_DSOS	= $(ALL_US_LIBS:$(DIST)/lib/lib%.a=-l%)
ALL_EXPORT_DSOS	= $(ALL_EXPORT_LIBS:$(DIST)/lib/lib%.a=-l%)
ALL_FRANCE_DSOS	= $(ALL_FRANCE_LIBS:$(DIST)/lib/lib%.a=-l%)

PLUGIN_DSO	= $(DIST)/bin/libnullplugin.so
WEBFONT_DSO	= $(DIST)/bin/libTrueDoc.so

ifndef MOZ_LITE
JSD_DSO		= $(DIST)/bin/libjsd.$(DLL_SUFFIX)
endif

ifdef BUILD_UNIX_PLUGINS
ifdef BUILD_OFFICIAL
EXTRA_REL_FILES	+= $(PLUGIN_DSO)
ifdef MKSHLIB
EXTRA_REL_FILES	+= $(JSD_DSO)
endif
AUTOADMIN_DSO	= $(DIST)/bin/libAutoAdmin.so
endif
TEST_PLUGIN	= $(DIST)/bin/libtextplugin.so
endif

#
# Only build the export binary by default, so the Security folks
# will be happy.  Release builds will build all three binaries.
#
# Defining NO_BUILD_EXPORT will _disable_ export builds, and
# defining BUILD_FRANCE and BUILD_US will enable france and us
# builds, respectively.  These can be set in the environment,
# on the command-line, or in myconfig.mk.
#
ifdef BUILD_OFFICIAL
BUILD_FRANCE	= 1
BUILD_US	= 1
endif

ifdef MKSHLIB
ifndef NO_NETSCAPE_SHARED
ifndef NO_BUILD_EXPORT
TARGETS		+= $(OBJDIR)/$(DSO_PROGNAME)-export
endif
ifdef BUILD_FRANCE
TARGETS		+= $(OBJDIR)/$(DSO_PROGNAME)-france
endif
ifdef BUILD_US
TARGETS		+= $(OBJDIR)/$(DSO_PROGNAME)-us
endif
endif
endif

ifndef NO_NETSCAPE_STATIC
ifndef NO_BUILD_EXPORT
TARGETS		+= $(OBJDIR)/$(XFE_PROGNAME)-export
endif
ifdef BUILD_FRANCE
TARGETS		+= $(OBJDIR)/$(XFE_PROGNAME)-france
endif
ifdef BUILD_US
TARGETS		+= $(OBJDIR)/$(XFE_PROGNAME)-us
endif
endif

#######################################################################

include $(DEPTH)/config/rules.mk

DEFINES		+= -DNEW_DECODERS

#######################################################################
#
# Set defaults for all platforms.  Each OS_ARCH will override this if
# necessary.
#

ifdef BUILD_OFFICIAL
EXTRA_REL_FILES	+= $(DIST)/bin/vreg $(DEPTH)/l10n/us/xp/bookmark.htm
endif

ifndef BUILD_OFFICIAL
#
# Developers won't need libTrueDoc.so.  No point having a dependency
# on this when only official builds (release builds) need this.
#
NO_WEBFONTS	= 1
endif

# Only SunOS4 needs two versions NIS and DNS.
NIS_SRC		=
NIS_OBJS	=
NIS_LIB		=
DNS_LIB		=

# Only SunOS4 needs separate YP versions.
NEED_YP_VERSION	= 0

# Only BSDI, Linux, and SunOS4 need the nls directory.
NEED_NLS	= 0

# Only SunOS5 (Solaris) has MCS.
MCS_CMD		= true

# Only IRIX 5.x uses this.
EXTRA_POST_LINK_CMD	= echo

EXPORT_LDFLAGS	= $(BASIC_LIBS) $(EXPORT_LIB) $(BASIC_LIBS_2) $(OTHER_LIBS) $(NSPR_LIB)
EXPORT_DEPLIBS	= $(BASIC_LIBS) $(EXPORT_LIB) $(BASIC_LIBS_2) $(NSPR_LIB)

FRANCE_LDFLAGS	= $(BASIC_LIBS) $(FRANCE_LIB) $(BASIC_LIBS_2) $(OTHER_LIBS) $(NSPR_LIB)
FRANCE_DEPLIBS	= $(BASIC_LIBS) $(FRANCE_LIB) $(BASIC_LIBS_2) $(NSPR_LIB)

US_LDFLAGS	= $(BASIC_LIBS) $(US_LIB) $(BASIC_LIBS_2) $(OTHER_LIBS) $(NSPR_LIB)
US_DEPLIBS	= $(BASIC_LIBS) $(US_LIB) $(BASIC_LIBS_2) $(NSPR_LIB)

DSO_EX_LIBS	= $(EXPORT_LDFLAGS:$(DIST)/lib/lib%.a=-l%)
DSO_EX_LIBS	= $(FRANCE_LDFLAGS:$(DIST)/lib/lib%.a=-l%)
DSO_US_LIBS	= $(US_LDFLAGS:$(DIST)/lib/lib%.a=-l%)

#######################################################################
#
# Adjust build based on OS_ARCH.
#

########################################
# IBM Machines
ifeq ($(OS_ARCH),AIX)
CCLD		= svxlC -+
#LDFLAGS		= -bGhooksyslibs -bGlibpathexec -bGnoproc
LDFLAGS		=  -bGnoproc
OTHER_LIBS	=
US_LDFLAGS	+= $(AIX_NSPR_LINK)
EXPORT_LDFLAGS	+= $(AIX_NSPR_LINK)
FRANCE_LDFLAGS	+= $(AIX_NSPR_LINK)
# Grab the NSPR shared library and the "Patched especially for Netscape" version of libsvld.a
EXTRA_REL_FILES	+= $(AIX_NSPR) \
		   $(DEPTH)/config/AIX4.1_libsvld.a \
		   $(DEPTH)/config/AIX4.1_libc.a \
		   $(DEPTH)/config/AIX4.2_libc.a \
		   $(DEPTH)/config/AIX4.1_run_netscape

ifndef NO_WEBFONTS
EXTRA_REL_FILES += $(WEBFONT_DSO)
endif

endif

########################################
# FreeBSD
ifeq ($(OS_ARCH),FreeBSD)
OTHER_LIBS	= -lm $(OS_LIBS)
endif

########################################
# BSDI
ifeq ($(OS_ARCH),BSD_OS)
OTHER_LIBS	= -lm $(OS_LIBS)

ifeq ($(OS_RELEASE),2.1)
OTHER_LIBS	+= -lipc
endif

NEED_NLS	= 1

endif

########################################
# HP Machines
ifeq ($(OS_ARCH),HP-UX)

ifeq ($(OS_RELEASE),B.10)
INCLUDES	+= -I.
endif

HPUX_RESOURCE_HACK	= -Wp,-H16384

OTHER_LIBS	= -L$(DIST)/lib $(OS_LIBS)

ifndef NO_EDITOR
EXPORT_DEPLIBS	+= cxxlink-filter
US_DEPLIBS	+= cxxlink-filter
CCLD		= CC -tl,./cxxlink-filter

ifndef NO_WEBFONTS
EXTRA_REL_FILES	+= $(WEBFONT_DSO)
endif

#
# On HP, enable SHLIB_PATH
#
EXTRA_POST_LINK_CMD	= chatr +s enable

endif

endif

########################################
# SGI Machines
ifeq ($(OS_ARCH),IRIX)

#
# Linker will report that '-lSgm' does not resolve any symbols, but it
# should not be removed. SGI dynamically opens the library depending on
# the setting of some resources (e.g. "useEnhancedFSB")
#
OTHER_LIBS	= -lgen -laudio -lm $(OS_LIBS)

#
# On Irix, tag the executable for use by the Indigo Magic Desktop.
# This magic number comes from /usr/lib/filetype/install/netscape.ftr
# shipped by SGI along with their n.nnS version of Mozilla.
#
ifeq ($(OS_RELEASE),5)
EXTRA_POST_LINK_CMD	= /usr/sbin/tag 67150
endif

#
# If we are using gtscc, we must use it as the linker, and we
# can only build statically (no shared libs).
#
CCLD		= CC
ifdef USE_GTSCC
ifndef NO_GTSCC
CCLD		= $(DIST)/bin/gtscc $(GTSCC_LD_OPTIONS) -gtsfile $(DEPTH)/config/$(OBJDIR)/db.gts -gtsrootdir $(DEPTH)
TARGETS		= $(OBJDIR)/$(XFE_PROGNAME)-export
endif
endif

ifndef NO_WEBFONTS
EXTRA_REL_FILES += $(WEBFONT_DSO)
endif

endif

########################################
# Linux
ifeq ($(OS_ARCH),Linux)

OTHER_LIBS	= /usr/lib/libm.a -ldl

US_LDFLAGS	+= -lc
EXPORT_LDFLAGS	+= -lc
FRANCE_LDFLAGS	+= -lc

endif

########################################
# NCR SYSV 4.0
ifeq ($(OS_ARCH),NCR)
OTHER_LIBS	= -lgen -lm $(OS_LIBS)
endif

########################################
# NEC SYSV 4.2
ifeq ($(OS_ARCH),NEC)
OTHER_LIBS	= -lresolv -lgen -lm $(OS_LIBS)
endif

########################################
# Dec Machines
ifeq ($(OS_ARCH),OSF1)

# We would like to link OSF1 static.
# This is because of motif problems (BadMatch errors on non-default visual)
# on 3.2 and 3.0 (and not on 2.0).
# and you get warnings and core dump when pasting into Mozilla.
# Also, libXm must be dynamic, otherwise Japanese text widgets hang
# the process.
# What a delightful state of affairs. -- erik

OTHER_LIBS	= -ldnet_stub -lm -lots $(OS_LIBS)

EXPORT_LDFLAGS	= $(BASIC_LIBS) $(EXPORT_LIB) $(BASIC_LIBS_2) $(NSPR_LIB) $(OTHER_LIBS)
FRANCE_LDFLAGS	= $(BASIC_LIBS) $(FRANCE_LIB) $(BASIC_LIBS_2) $(NSPR_LIB) $(OTHER_LIBS)
US_LDFLAGS	= $(BASIC_LIBS) $(US_LIB) $(BASIC_LIBS_2) $(NSPR_LIB) $(OTHER_LIBS)

ifndef NO_EDITOR
#
# OSF's cxx link command will link in _dynamic_ version of two C++
# libraries. This is bad, they are not shipped on a base system.
# So, we call this special hacked driver script to do the job.
#
#   NOT TRUE FOR DIGITAL UNIX 4.0 OR GREATER
#CCLD		= ./cxxlink-driver -cxx_platform OSF1 -cxx_command cxx

# Hack to add a dependency for executable.
#EXPORT_DEPLIBS	+= cxxlink-driver
#FRANCE_DEPLIBS	+= cxxlink-driver
#US_DEPLIBS	+= cxxlink-driver

# This will also work, but will probably break easily as more C++
# *features* get used (like static constructors - gasp!)
#CCLD		= cc
#OTHER_LIBS	+= /usr/lib/cmplrs/cxx/libcxx.a /usr/lib/cmplrs/cxx/libexc.a
endif

endif

########################################
# SNI ReliantUNIX (SINIX)
ifeq ($(OS_ARCH),ReliantUNIX)
OTHER_LIBS	= $(OS_LIBS)
endif
ifeq ($(OS_ARCH),SINIX-N)
OTHER_LIBS	= $(OS_LIBS)
endif

########################################
# SCO OpenServer
ifeq ($(OS_ARCH),SCOOS)
OTHER_LIBS	= -lm -lPW $(OS_LIBS)

ifdef NEED_XMOS
LIB_XMOS	= $(OBJDIR)/Xmos.o
endif

endif

########################################
# Sun Machines
ifeq ($(OS_ARCH),SunOS)

ifeq ($(OS_RELEASE),4.1)

ALL_TARGETS  += $(OBJDIR)/$(XFE_PROGNAME)-nis-export	\
		$(OBJDIR)/$(XFE_PROGNAME)-nis-net-export \
		$(OBJDIR)/$(XFE_PROGNAME)-nis-france	\
		$(OBJDIR)/$(XFE_PROGNAME)-nis-net-france \
		$(OBJDIR)/$(XFE_PROGNAME)-nis-us	\
		$(OBJDIR)/$(XFE_PROGNAME)-nis-net-us \
		$(NULL)

OTHER_LIBS	= 

ifndef NO_WEBFONTS
EXTRA_REL_FILES += $(WEBFONT_DSO)
endif

ifndef NO_EDITOR
OTHER_LIBS	+= -L/tools/ns/lib

#
# Need this guy because the one in libg++.a is broken.
#
BASIC_LIBS	+= $(OBJDIR)/regex.o
$(OBJDIR)/regex.o:	/lib/libc.a
	ar x /lib/libc.a regex.o && mv regex.o $@
endif
#
# Need our own popen to fix the SunOS popen problem
#
BASIC_LIBS += $(OBJDIR)/popen.o
CSRCS	+= popen.c

US_LDFLAGS	+= -lm
EXPORT_LDFLAGS	+= -lm
FRANCE_LDFLAGS	+= -lm

# SunOS had 2 executables. Only the non-nis version need this
NIS_OBJS	= $(OBJDIR)/dns-stub.o
NIS_SRCS	= dns-stub.c
NIS_LIB		= $(NIS_OBJS)
DNS_LIB		= -lresolv

# For release only.
# This is only for SunOS as it has both nis and non-nis
# version of the browser packaged together.
EXTRA_EXPORT_OBJS	= $(XFE_PROGNAME)-nis-export
EXTRA_FRANCE_OBJS	= $(XFE_PROGNAME)-nis-france
EXTRA_US_OBJS		= $(XFE_PROGNAME)-nis-us
EXTRA_NETEXPORT_OBJS	= $(XFE_PROGNAME)-nis-net-export

NEED_YP_VERSION	= 1
NEED_NLS	= 1

endif

########################################
ifneq (,$(filter 5 5.5,$(OS_RELEASE)))

USRLIBDIR	:= /usr/openwin/lib
MCS_CMD		= mcs -d

ifeq ($(CPU_ARCH),sparc)
OTHER_LIBS	= $(OS_LIBS) -lgen -lresolv -lm
ifndef NO_WEBFONTS
EXTRA_REL_FILES += $(WEBFONT_DSO)
endif
else
OTHER_LIBS	= $(OS_LIBS) -lm
LDFLAGS		= $(NOMD_CFLAGS)
endif

US_LDFLAGS	= -z defs -L$(USRLIBDIR) \
		  -R$(USRLIBDIR) $(BASIC_LIBS) $(US_LIB) $(BASIC_LIBS_2) \
		  $(OTHER_LIBS) $(NSPR_LIB)
EXPORT_LDFLAGS	= -z defs -L$(USRLIBDIR) \
		  -R$(USRLIBDIR) $(BASIC_LIBS) $(EXPORT_LIB) $(BASIC_LIBS_2) \
		  $(OTHER_LIBS) $(NSPR_LIB)
FRANCE_LDFLAGS	= -z defs -L$(USRLIBDIR) \
		  -R$(USRLIBDIR) $(BASIC_LIBS) $(FRANCE_LIB) $(BASIC_LIBS_2) \
		  $(OTHER_LIBS) $(NSPR_LIB)

endif

endif

########################################
# SCO Unixware 2.1
ifeq ($(OS_ARCH),UNIXWARE)

ifdef NEED_XMOS
LIB_XMOS	= $(OBJDIR)/Xmos.o
endif

OTHER_LIBS	= -lm

US_LDFLAGS	+= -lresolv -lsocket -lc /usr/ucblib/libucb.a
EXPORT_LDFLAGS	+= -lresolv -lsocket -lc /usr/ucblib/libucb.a
FRANCE_LDFLAGS	+= -lresolv -lsocket -lc /usr/ucblib/libucb.a

endif

#######################################################################

# Rules to build the special MOZ_LITE *.o files

$(TARGETS): $(OBJS)

#
# I moved this to ns/config/rules.mk  -mcafee
#


#######################################################################
# Rules to build config*.o files

#- export

$(OBJDIR)/$(LITE_PREFIX)config-export.o: config.c versionn.h Makefile $(OBJS) $(EXPORT_DEPLIBS)
	@echo Generating $@ from config.c...;				      \
	X=' ';								      \
	VN=`sed -n$$X 's/^#define VERSION_NUMBER[ 	]*\(.*\)$$/\1/p' versionn.h` ; \
	$(CC) -c $(CFLAGS) -o $@ config.c				      \
		-DCONFIG="$(GUESS_CONFIG)"				      \
		-DDATE="`date +%d-%h-%y`"				      \
		-DVERSION=$${VN}					      \
		-UHAVE_NIS -UFRANCE_VERSION -DEXPORT_VERSION -UUS_VERSION -UVENDOR_ANIM

$(OBJDIR)/config-net-export.o: config.c versionn.h Makefile $(OBJS) $(EXPORT_DEPLIBS)
	@echo Generating $@ from config.c...;				      \
	X=' ';								      \
	VN=`sed -n$$X 's/^#define VERSION_NUMBER[ 	]*\(.*\)$$/\1/p' versionn.h` ; \
	$(CC) -c $(CFLAGS) -o $@ config.c				      \
		-DCONFIG="$(GUESS_CONFIG)"				      \
		-DDATE="`date +%d-%h-%y`"				      \
		-DVERSION=$${VN}N					      \
		-UHAVE_NIS -UFRANCE_VERSION -DEXPORT_VERSION -UUS_VERSION -UVENDOR_ANIM

$(OBJDIR)/$(LITE_PREFIX)config-nis-export.o: config.c versionn.h Makefile $(OBJS) $(NIS_OBJS) $(EXPORT_DEPLIBS)
	@echo Generating $@ from config.c...;				      \
	X=' ';								      \
	VN=`sed -n$$X 's/^#define VERSION_NUMBER[ 	]*\(.*\)$$/\1/p' versionn.h` ; \
	$(CC) -c $(CFLAGS) -o $@ config.c				      \
		-DCONFIG="$(GUESS_CONFIG)"				      \
		-DDATE="`date +%d-%h-%y`"				      \
		-DVERSION=$${VN}					      \
		-DHAVE_NIS -UFRANCE_VERSION -DEXPORT_VERSION -UUS_VERSION -UVENDOR_ANIM

$(OBJDIR)/config-nis-net-export.o: config.c versionn.h Makefile $(OBJS) $(NIS_OBJS) $(EXPORT_DEPLIBS)
	@echo Generating $@ from config.c...;				      \
	X=' ';								      \
	VN=`sed -n$$X 's/^#define VERSION_NUMBER[ 	]*\(.*\)$$/\1/p' versionn.h` ; \
	$(CC) -c $(CFLAGS) -o $@ config.c				      \
		-DCONFIG="$(GUESS_CONFIG)"				      \
		-DDATE="`date +%d-%h-%y`"				      \
		-DVERSION=$${VN}N					      \
		-DHAVE_NIS -UFRANCE_VERSION -DEXPORT_VERSION -UUS_VERSION -UVENDOR_ANIM

$(OBJDIR)/config-sgi-export.o: config.c versionn.h Makefile $(OBJS) $(EXPORT_DEPLIBS)
	@echo Generating $@ from config.c...;				      \
	X=' ';								      \
	VN=`sed -n$$X 's/^#define VERSION_NUMBER[ 	]*\(.*\)$$/\1/p' versionn.h` ; \
	$(CC) -c $(CFLAGS) -o $@ config.c				      \
		-DCONFIG="$(GUESS_CONFIG)"				      \
		-DDATE="`date +%d-%h-%y`"				      \
		-DVERSION=$${VN}S					      \
		-UHAVE_NIS -UFRANCE_VERSION -DEXPORT_VERSION -UUS_VERSION -DVENDOR_ANIM

#- france

$(OBJDIR)/$(LITE_PREFIX)config-france.o: config.c versionn.h Makefile $(OBJS) $(FRANCE_DEPLIBS)
	@echo Generating $@ from config.c...;				      \
	X=' ';								      \
	VN=`sed -n$$X 's/^#define VERSION_NUMBER[ 	]*\(.*\)$$/\1/p' versionn.h` ; \
	$(CC) -c $(CFLAGS) -o $@ config.c				      \
		-DCONFIG="$(GUESS_CONFIG)"				      \
		-DDATE="`date +%d-%h-%y`"				      \
		-DVERSION=$${VN}					      \
		-UHAVE_NIS -DFRANCE_VERSION -UEXPORT_VERSION -UUS_VERSION -UVENDOR_ANIM

$(OBJDIR)/config-net-france.o: config.c versionn.h Makefile $(OBJS) $(FRANCE_DEPLIBS)
	@echo Generating $@ from config.c...;				      \
	X=' ';								      \
	VN=`sed -n$$X 's/^#define VERSION_NUMBER[ 	]*\(.*\)$$/\1/p' versionn.h` ; \
	$(CC) -c $(CFLAGS) -o $@ config.c				      \
		-DCONFIG="$(GUESS_CONFIG)"				      \
		-DDATE="`date +%d-%h-%y`"				      \
		-DVERSION=$${VN}N					      \
		-UHAVE_NIS -DFRANCE_VERSION -UEXPORT_VERSION -UUS_VERSION -UVENDOR_ANIM

$(OBJDIR)/$(LITE_PREFIX)config-nis-france.o: config.c versionn.h Makefile $(OBJS) $(NIS_OBJS) $(FRANCE_DEPLIBS)
	@echo Generating $@ from config.c...;				      \
	X=' ';								      \
	VN=`sed -n$$X 's/^#define VERSION_NUMBER[ 	]*\(.*\)$$/\1/p' versionn.h` ; \
	$(CC) -c $(CFLAGS) -o $@ config.c				      \
		-DCONFIG="$(GUESS_CONFIG)"				      \
		-DDATE="`date +%d-%h-%y`"				      \
		-DVERSION=$${VN}					      \
		-DHAVE_NIS -DFRANCE_VERSION -UEXPORT_VERSION -UUS_VERSION -UVENDOR_ANIM

$(OBJDIR)/config-nis-net-france.o: config.c versionn.h Makefile $(OBJS) $(NIS_OBJS) $(FRANCE_DEPLIBS)
	@echo Generating $@ from config.c...;				      \
	X=' ';								      \
	VN=`sed -n$$X 's/^#define VERSION_NUMBER[ 	]*\(.*\)$$/\1/p' versionn.h` ; \
	$(CC) -c $(CFLAGS) -o $@ config.c				      \
		-DCONFIG="$(GUESS_CONFIG)"				      \
		-DDATE="`date +%d-%h-%y`"				      \
		-DVERSION=$${VN}N					      \
		-DHAVE_NIS -DFRANCE_VERSION -UEXPORT_VERSION -UUS_VERSION -UVENDOR_ANIM

$(OBJDIR)/config-sgi-france.o: config.c versionn.h Makefile $(OBJS) $(FRANCE_DEPLIBS)
	@echo Generating $@ from config.c...;				      \
	X=' ';								      \
	VN=`sed -n$$X 's/^#define VERSION_NUMBER[ 	]*\(.*\)$$/\1/p' versionn.h` ; \
	$(CC) -c $(CFLAGS) -o $@ config.c				      \
		-DCONFIG="$(GUESS_CONFIG)"				      \
		-DDATE="`date +%d-%h-%y`"				      \
		-DVERSION=$${VN}S					      \
		-UHAVE_NIS -DFRANCE_VERSION -UEXPORT_VERSION -UUS_VERSION -DVENDOR_ANIM

#- us

$(OBJDIR)/$(LITE_PREFIX)config-us.o: config.c versionn.h Makefile $(OBJS) $(US_DEPLIBS)
	@echo Generating $@ from config.c...;				      \
	X=' ';								      \
	VN=`sed -n$$X 's/^#define VERSION_NUMBER[ 	]*\(.*\)$$/\1/p' versionn.h` ; \
	$(CC) -c $(CFLAGS) -o $@ config.c				      \
		-DCONFIG="$(GUESS_CONFIG)"				      \
		-DDATE="`date +%d-%h-%y`"				      \
		-DVERSION=$${VN}					      \
		-UHAVE_NIS -UFRANCE_VERSION -UEXPORT_VERSION -DUS_VERSION -UVENDOR_ANIM

$(OBJDIR)/config-net-us.o: config.c versionn.h Makefile $(OBJS) $(US_DEPLIBS)
	@echo Generating $@ from config.c...;				      \
	X=' ';								      \
	VN=`sed -n$$X 's/^#define VERSION_NUMBER[ 	]*\(.*\)$$/\1/p' versionn.h` ; \
	$(CC) -c $(CFLAGS) -o $@ config.c				      \
		-DCONFIG="$(GUESS_CONFIG)"				      \
		-DDATE="`date +%d-%h-%y`"				      \
		-DVERSION=$${VN}N					      \
		-UHAVE_NIS -UFRANCE_VERSION -UEXPORT_VERSION -DUS_VERSION -UVENDOR_ANIM

$(OBJDIR)/$(LITE_PREFIX)config-nis-us.o: config.c versionn.h Makefile $(OBJS) $(NIS_OBJS) $(US_DEPLIBS)
	@echo Generating $@ from config.c...;				      \
	X=' ';								      \
	VN=`sed -n$$X 's/^#define VERSION_NUMBER[ 	]*\(.*\)$$/\1/p' versionn.h` ; \
	$(CC) -c $(CFLAGS) -o $@ config.c				      \
		-DCONFIG="$(GUESS_CONFIG)"				      \
		-DDATE="`date +%d-%h-%y`"				      \
		-DVERSION=$${VN}					      \
		-DHAVE_NIS -UFRANCE_VERSION -UEXPORT_VERSION -DUS_VERSION -UVENDOR_ANIM

$(OBJDIR)/config-nis-net-us.o: config.c versionn.h Makefile $(OBJS) $(NIS_OBJS) $(US_DEPLIBS)
	@echo Generating $@ from config.c...;				      \
	X=' ';								      \
	VN=`sed -n$$X 's/^#define VERSION_NUMBER[ 	]*\(.*\)$$/\1/p' versionn.h` ; \
	$(CC) -c $(CFLAGS) -o $@ config.c				      \
		-DCONFIG="$(GUESS_CONFIG)"				      \
		-DDATE="`date +%d-%h-%y`"				      \
		-DVERSION=$${VN}N					      \
		-DHAVE_NIS -UFRANCE_VERSION -UEXPORT_VERSION -DUS_VERSION -UVENDOR_ANIM

$(OBJDIR)/config-sgi-us.o: config.c versionn.h Makefile $(OBJS) $(US_DEPLIBS)
	@echo Generating $@ from config.c...;				      \
	X=' ';								      \
	VN=`sed -n$$X 's/^#define VERSION_NUMBER[ 	]*\(.*\)$$/\1/p' versionn.h` ; \
	$(CC) -c $(CFLAGS) -o $@ config.c				      \
		-DCONFIG="$(GUESS_CONFIG)"				      \
		-DDATE="`date +%d-%h-%y`"				      \
		-DVERSION=$${VN}S					      \
		-UHAVE_NIS -UFRANCE_VERSION -UEXPORT_VERSION -DUS_VERSION -DVENDOR_ANIM

#######################################################################
# Rules to build license.o files

$(OBJDIR)/license.o: $(OBJDIR)/LICENSE.c Makefile
	$(CC) -c -o $@ $<

$(OBJDIR)/LICENSE.c: Makefile $(LICENSES) make-license
	@./make-license $@ $(LICENSE_DIR) "" export

$(OBJDIR)/LICENSE: $(OBJDIR)/license.o make-license
	@./make-license $@ $(LICENSE_DIR) "" export

#######################################################################
# Rules to build README files ( release specific )

$(OBJDIR)/README: README.tmpl versionn.h
	@rm -f $@
	@echo "Generating $@ from $<...";				\
	CLASS="Netscape";						\
	NAME="Netscape";						\
	PROGNAME="netscape";						\
	PRODUCT="$(PRODUCT_NAME)";					\
	VN=`sed -n 's/^#define VERSION_NUMBER *\(.*\)$$/\1/p' versionn.h`; \
	VERS=`echo $$VN$$vsuffix`;					\
	sed 	"s/@NAME@/$$NAME/g;					\
		 s/@CLASS@/$$CLASS/g;					\
		 s/@PROGNAME@/$$PROGNAME/g;				\
		 s/@PRODUCT@/$$PRODUCT/g;				\
		 s/@VERSION@/$$VERS/g;					\
		 s/@URLVERSION@/$$VERS/g" $< > $@

#######################################################################
# Rules to build resources

.SUFFIXES: .ad

# DO NOT CHANGE THE ORDER OF THE DEPENDENCIES.  Add new ones to the end.
resources-%.o: RESOURCES-%.c Makefile
	$(CC) -c $(HPUX_RESOURCE_HACK) -o $@ $<

RESOURCES-%.c: Netscape-%.ad ad2c Makefile
	@echo 'char *fe_fallbackResources[] = {' > $@; \
	./ad2c $< >> $@; \
	echo '0};' >> $@

# Explicit dependency list to ensure that e_kit_resources.h gets built.
$(OBJDIR)/e_kit.o:	e_kit.c e_kit_resources.h

# Turn ekit app-defaults into a C file
e_kit_resources.h: e_kit.ad
	@echo 'char* fe_ekitDefaultDatabase = ' > $@
	cat $< | tr '[\001-\272]' '[\106-\377]' | \
        od -b | sed 's/^[0-7][0-7]* *\(.*\)/\\\1/; \
		s/ /\\/g;s/\(.*\)/ "\1"/;s/^ *"\\"$$//' >> $@
	@echo ';' >> $@

#- export

$(OBJDIR)/Netscape-nis-export.ad $(OBJDIR)/Netscape-export.ad: Makefile resources versionn.h strs make-resources $(LOCALE_MAP)
	@./make-resources $@ Netscape Netscape "" export $(LOCALES)

$(OBJDIR)/Netscape-nis-net-export.ad $(OBJDIR)/Netscape-net-export.ad: Makefile resources versionn.h strs make-resources $(LOCALE_MAP)
	@./make-resources $@ Netscape Netscape "N" export $(LOCALES)

$(OBJDIR)/Netscape-sgi-export.ad: Makefile resources versionn.h strs make-resources $(LOCALE_MAP)
	@./make-resources $@ Netscape Netscape "S" export $(LOCALES)

#- france

$(OBJDIR)/Netscape-nis-france.ad $(OBJDIR)/Netscape-france.ad: Makefile resources versionn.h strs make-resources $(LOCALE_MAP)
	@./make-resources $@ Netscape Netscape "" france $(LOCALES)

$(OBJDIR)/Netscape-nis-net-france.ad $(OBJDIR)/Netscape-net-france.ad: Makefile resources versionn.h strs make-resources $(LOCALE_MAP)
	@./make-resources $@ Netscape Netscape "N" france $(LOCALES)

$(OBJDIR)/Netscape-sgi-france.ad: Makefile resources versionn.h strs make-resources $(LOCALE_MAP)
	@./make-resources $@ Netscape Netscape "S" france $(LOCALES)

#- us

$(OBJDIR)/Netscape-nis-us.ad $(OBJDIR)/Netscape-us.ad: Makefile resources versionn.h strs make-resources $(LOCALE_MAP)
	@./make-resources $@ Netscape Netscape "" us $(LOCALES)

$(OBJDIR)/Netscape-nis-net-us.ad $(OBJDIR)/Netscape-net-us.ad: Makefile resources versionn.h strs make-resources $(LOCALE_MAP)
	@./make-resources $@ Netscape Netscape "N" us $(LOCALES)

$(OBJDIR)/Netscape-sgi-us.ad: Makefile resources versionn.h strs make-resources $(LOCALE_MAP)
	@./make-resources $@ Netscape Netscape "S" us $(LOCALES)

#######################################################################
# 		The "-export"  DSO targets follow 

$(OBJDIR)/$(DSO_PROGNAME)-export: $(OBJS) $(EXPORT_OBJS) $(EXPORT_DEPLIBS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(CCLD) -o $@ $(LDFLAGS) $(OBJS) $(EXPORT_OBJS) -L$(DIST)/bin -L$(DIST)/lib $(DSO_EX_LIBS) $(DNS_LIB)
	@$(EXTRA_POST_LINK_CMD) $@

$(OBJDIR)/$(DSO_PROGNAME)-export.mcv: $(OBJS) $(EXPORT_OBJS) $(EXPORT_DEPLIBS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(CCLD) -o $@ $(CFLAGS) $(OBJS) $(EXPORT_OBJS) -L$(DIST)/bin -L$(DIST)/lib $(DSO_EX_LIBS) $(DNS_LIB) -lmalloc_cv
	@$(EXTRA_POST_LINK_CMD) $@

$(OBJDIR)/$(DSO_PROGNAME)-export.pure: $(OBJS) $(EXPORT_OBJS) $(EXPORT_DEPLIBS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(PURIFY) $(CCLD) -o $@ $(LDFLAGS) $(OBJS) $(EXPORT_OBJS) -L$(DIST)/bin -L$(DIST)/lib $(DSO_EX_LIBS) $(DNS_LIB)
	@$(EXTRA_POST_LINK_CMD) $@

$(OBJDIR)/$(DSO_PROGNAME)-export.quantify: $(OBJS) $(EXPORT_OBJS) $(EXPORT_DEPLIBS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(QUANTIFY) $(CCLD) -o $@ $(LDFLAGS) $(OBJS) $(EXPORT_OBJS) -L$(DIST)/bin -L$(DIST)/lib $(DSO_EX_LIBS) $(DNS_LIB)
	@$(EXTRA_POST_LINK_CMD) $@

#------------------ the "-us" DSO link targets follow ------------------- #

$(OBJDIR)/$(DSO_PROGNAME)-us: $(OBJS) $(US_OBJS) $(US_DEPLIBS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(CCLD) -o $@ $(LDFLAGS) $(OBJS) $(US_OBJS) -L$(DIST)/bin -L$(DIST)/lib $(DSO_US_LIBS) $(DNS_LIB)
	@$(EXTRA_POST_LINK_CMD) $@

$(OBJDIR)/$(DSO_PROGNAME)-us.pure: $(OBJS) $(US_OBJS) $(US_DEPLIBS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(PURIFY) $(CCLD) -o $@ $(LDFLAGS) $(OBJS) $(US_OBJS) -L$(DIST)/bin -L$(DIST)/lib $(DSO_US_LIBS) $(DNS_LIB) @$(EXTRA_POST_LINK_CMD) $@

#------------------ the "-export" XFE link targets follow ------------------- #

$(OBJDIR)/$(XFE_PROGNAME)-export: $(OBJS) $(EXPORT_OBJS) $(EXPORT_DEPLIBS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(CCLD) -o $@ $(LDFLAGS) $(OBJS) $(EXPORT_OBJS) $(EXPORT_LDFLAGS) $(DNS_LIB)
	@$(EXTRA_POST_LINK_CMD) $@

$(OBJDIR)/$(XFE_PROGNAME)-export.pure: $(OBJS) $(EXPORT_OBJS) $(EXPORT_DEPLIBS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(PURIFY) $(CCLD) -o $@ $(NOMD_LDFLAGS) $(OBJS) $(EXPORT_OBJS) $(EXPORT_LDFLAGS) $(DNS_LIB)
	@$(EXTRA_POST_LINK_CMD) $@

$(OBJDIR)/$(XFE_PROGNAME)-export.quantify: $(OBJS) $(EXPORT_OBJS) $(EXPORT_DEPLIBS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(QUANTIFY) $(CCLD) -o $@ $(NOMD_LDFLAGS) $(OBJS) $(EXPORT_OBJS) $(EXPORT_LDFLAGS) $(DNS_LIB)
	@$(EXTRA_POST_LINK_CMD) $@

$(OBJDIR)/$(XFE_PROGNAME)-export.prof: $(OBJS) $(EXPORT_OBJS) $(EXPORT_DEPLIBS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(CCLD) -p -o $@ $(LDFLAGS) $(OBJS) $(EXPORT_OBJS) $(EXPORT_LDFLAGS) $(DNS_LIB)
	@$(EXTRA_POST_LINK_CMD) $@

#------------------ the "-france" XFE targets follow ---------------- #

ifndef USE_WACKER

$(OBJDIR)/$(XFE_PROGNAME)-france: $(OBJS) $(FRANCE_OBJS) $(FRANCE_DEPLIBS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(CCLD) -o $@ $(LDFLAGS) $(OBJS) $(FRANCE_OBJS) $(FRANCE_LDFLAGS) $(DNS_LIB)
	@$(EXTRA_POST_LINK_CMD) $@

else

$(OBJDIR)/$(XFE_PROGNAME)-france: $(OBJDIR)/$(XFE_PROGNAME)-export $(XPDIST)/xpdist/france.txt $(DIST)/bin/patcher
	@$(MAKE_OBJDIR)
	rm -f $@
	$(DIST)/bin/patcher $(XPDIST)/xpdist/france.txt $(OBJDIR)/$(XFE_PROGNAME)-export $@

endif

#------------------ the "-us" link XFE targets follow ------------------- #

$(OBJDIR)/$(XFE_PROGNAME)-us: $(OBJS) $(US_OBJS) $(US_DEPLIBS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(CCLD) -o $@ $(LDFLAGS) $(OBJS) $(US_OBJS) $(US_LDFLAGS) $(DNS_LIB)
	@$(EXTRA_POST_LINK_CMD) $@

$(OBJDIR)/$(XFE_PROGNAME)-us-ld: $(OBJS) $(US_OBJS) $(US_DEPLIBS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(LD) -o $@ $(OBJS) $(US_OBJS) $(US_LDFLAGS) $(DNS_LIB)
	@$(EXTRA_POST_LINK_CMD) $@

$(OBJDIR)/$(XFE_PROGNAME)-us.pure: $(OBJS) $(US_OBJS) $(US_DEPLIBS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(PURIFY) $(CCLD) -o $@ $(NOMD_LDFLAGS) $(OBJS) $(US_OBJS) $(US_LDFLAGS) $(DNS_LIB)
	@$(EXTRA_POST_LINK_CMD) $@

#------------------ the "-net" XFE link targets follow ------------------- #


$(OBJDIR)/$(XFE_PROGNAME)-net-export: $(OBJS) $(NET_EXPORT_OBJS) $(EXPORT_DEPLIBS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(CCLD) -o $@ $(LDFLAGS) $(OBJS) $(NET_EXPORT_OBJS) $(EXPORT_LDFLAGS) $(DNS_LIB)
	@$(EXTRA_POST_LINK_CMD) $@

$(OBJDIR)/$(XFE_PROGNAME)-net-us: $(OBJS) $(NET_US_OBJS) $(US_DEPLIBS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(CCLD) -o $@ $(LDFLAGS) $(OBJS) $(NET_US_OBJS) $(US_LDFLAGS) $(DNS_LIB)
	@$(EXTRA_POST_LINK_CMD) $@

ifndef USE_WACKER

$(OBJDIR)/$(XFE_PROGNAME)-net-france: $(OBJS) $(NET_FRANCE_OBJS) $(FRANCE_DEPLIBS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(CCLD) -o $@ $(LDFLAGS) $(OBJS) $(NET_FRANCE_OBJS) $(FRANCE_LDFLAGS) $(DNS_LIB)
	@$(EXTRA_POST_LINK_CMD) $@

else

$(OBJDIR)/$(XFE_PROGNAME)-net-france: $(OBJDIR)/$(XFE_PROGNAME)-net-export $(XPDIST)/xpdist/france.txt $(DIST)/bin/patcher
	@$(MAKE_OBJDIR)
	rm -f $@
	$(DIST)/bin/patcher $(XPDIST)/xpdist/france.txt $(OBJDIR)/$(XFE_PROGNAME)-net-export $@

endif

#------------------ the "-nis" XFE link targets follow ------------------- #

$(OBJDIR)/$(XFE_PROGNAME)-nis-export: $(OBJS) $(NIS_EXPORT_OBJS) $(NIS_OBJS) $(EXPORT_DEPLIBS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(CCLD) -o $@ $(LDFLAGS) $(OBJS) $(NIS_EXPORT_OBJS) $(EXPORT_LDFLAGS) $(NIS_LIB)
	@$(EXTRA_POST_LINK_CMD) $@

$(OBJDIR)/$(XFE_PROGNAME)-nis-france: $(OBJS) $(NIS_FRANCE_OBJS) $(NIS_OBJS) $(FRANCE_DEPLIBS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(CCLD) -o $@ $(LDFLAGS) $(OBJS) $(NIS_FRANCE_OBJS) $(EXPORT_LDFLAGS) $(NIS_LIB)
	@$(EXTRA_POST_LINK_CMD) $@

$(OBJDIR)/$(XFE_PROGNAME)-nis-us: $(OBJS) $(NIS_US_OBJS) $(NIS_OBJS) $(US_DEPLIBS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(CCLD) -o $@ $(LDFLAGS) $(OBJS) $(NIS_US_OBJS) $(US_LDFLAGS) $(NIS_LIB)
	@$(EXTRA_POST_LINK_CMD) $@

$(OBJDIR)/$(XFE_PROGNAME)-nis-net-export: $(OBJS) $(NIS_EXPORT_OBJS) $(NIS_OBJS) $(EXPORT_DEPLIBS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(CCLD) -o $@ $(LDFLAGS) $(OBJS) $(NIS_EXPORT_OBJS) $(EXPORT_LDFLAGS) $(NIS_LIB)
	@$(EXTRA_POST_LINK_CMD) $@

#------------------ the "-nis-net" XFE link targets follow ---------------- #

$(OBJDIR)/$(XFE_PROGNAME)-nis-net-us: $(OBJS) $(NIS_US_OBJS) $(NIS_OBJS) $(US_DEPLIBS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(CCLD) -o $@ $(LDFLAGS) $(OBJS) $(NIS_US_OBJS) $(US_LDFLAGS) $(NIS_LIB)
	@$(EXTRA_POST_LINK_CMD) $@

#-------------- SGI versions of the navigator (no nis, no net) ------------#

$(OBJDIR)/$(XFE_PROGNAME)-sgi-export: $(OBJS) $(SGI_EXPORT_OBJS) $(EXPORT_DEPLIBS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(CCLD) -o $@ $(LDFLAGS) $(OBJS) $(SGI_EXPORT_OBJS) $(EXPORT_LDFLAGS) $(DNS_LIB)
	@$(EXTRA_POST_LINK_CMD) $@

$(OBJDIR)/$(XFE_PROGNAME)-sgi-us: $(OBJS) $(SGI_US_OBJS) $(US_DEPLIBS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(CCLD) -o $@ $(LDFLAGS) $(OBJS) $(SGI_US_OBJS) $(US_LDFLAGS) $(DNS_LIB)
	@$(EXTRA_POST_LINK_CMD) $@

ifndef USE_WACKER

$(OBJDIR)/$(XFE_PROGNAME)-sgi-france: $(OBJS) $(SGI_FRANCE_OBJS) $(FRANCE_DEPLIBS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(CCLD) -o $@ $(LDFLAGS) $(OBJS) $(SGI_FRANCE_OBJS) $(FRANCE_LDFLAGS) $(DNS_LIB)
	@$(EXTRA_POST_LINK_CMD) $@

else

$(OBJDIR)/$(XFE_PROGNAME)-sgi-france: $(OBJDIR)/$(XFE_PROGNAME)-sgi-export $(XPDIST)/xpdist/france.txt $(DIST)/bin/patcher
	@$(MAKE_OBJDIR)
	rm -f $@
	$(DIST)/bin/patcher $(XPDIST)/xpdist/france.txt $(OBJDIR)/$(XFE_PROGNAME)-sgi-export $@

endif

#---------------- Misc link targets follow ----------------------#

# Make a dynamic export executable using malloc_cv by default.
mcv:	$(OBJDIR)/$(DSO_PROGNAME)-export $(OBJDIR)/$(DSO_PROGNAME)-export.mcv

# Make a purified, static export executable by default.
pure:	$(OBJDIR)/$(XFE_PROGNAME)-export $(OBJDIR)/$(XFE_PROGNAME)-export.pure

# Trying out quantify.
quantify:	$(OBJDIR)/$(XFE_PROGNAME)-export $(OBJDIR)/$(XFE_PROGNAME)-export.quantify

# Use gtscc to find dead code globals.
unreferenced: $(OBJS) $(NET_EXPORT_OBJS) $(BASIC_LIBS) $(EXPORT_LIB) $(BASIC_LIBS_2) $(NSPR_LIB) $(DIST)/bin/gtscc 
	@$(MAKE_OBJDIR)
	rm -f $@
	$(DIST)/bin/gtscc -gtsrootdir $(DEPTH) -gtsdump -gtsnorecompile -gtsnolink $(DEPTH) $(OBJS) $(NET_EXPORT_OBJS) $(BASIC_LIBS) $(EXPORT_LIB) $(BASIC_LIBS_2) $(NSPR_LIB) | awk -n '/ 0 0 / { print $$5 " " $$1 }' | sort > $@

# Some dependencies that -MDupdate isn't getting.
$(OBJDIR)/strids.o:	xfe_err.h
$(OBJDIR)/strings.o:	xfe_err.h

ifeq ($(OS_ARCH),AIX)
AIX_HACK	:= $(notdir $(AIX_NSPR))

# Install nspr dynamic library for AIX.
$(AIX_HACK): $(AIX_NSPR)
	$(INSTALL) -m 644 $< .
endif

install:: $(AIX_HACK) $(EXTRA_REL_FILES) $(TARGETS)
ifdef TARGETS
	$(INSTALL) $(TARGETS) $(DIST)/bin
endif

#######################################################################
# Release target

ifdef AUTOADMIN_DSO
$(AUTOADMIN_DSO):
	@cd $(DEPTH)/modules/libpref/admin; $(MAKE) POLARIS=1 polaris
endif

$(PLUGIN_DSO):
	@cd plugins/nullplugin; $(MAKE)

ifdef TEST_PLUGIN
$(TEST_PLUGIN):
	@cd plugins/textplugin; $(MAKE)
endif

$(DIST)/bin/zigbert$(BIN_SUFFIX):
	@cd $(DEPTH)/security/cmd/lib; $(MAKE)
	@cd $(DEPTH)/security/cmd/jar; $(MAKE)

$(DEPTH)/tpubs/$(NETHELP_DIR)/nethelp.tar.gz:
	@cd $(DEPTH)/tpubs/$(NETHELP_DIR); $(MAKE) release

ifdef USE_SPELLCHECK
$(DEPTH)/modules/spellchk/data/spellchk.tar.gz:
	@cd $(DEPTH)/modules/spellchk/data; $(MAKE) release
endif

release:: $(RELEASES)

release-us: $(XFE_PROGNAME)-us $(EXTRA_US_OBJS) $(COMMON_REL_DEPS)
	./make-release $(OBJDIR) $(XFE_PROGNAME) Netscape "" us $(GUESS_CONFIG) $(NEED_YP_VERSION) "$(MCS_CMD)" $(NEED_NLS) "$(EXTRA_US_BINARY) $(EXTRA_REL_FILES)" $(README_FILE)

release-export: $(XFE_PROGNAME)-export $(EXTRA_EXPORT_OBJS) $(COMMON_REL_DEPS)
	./make-release $(OBJDIR) $(XFE_PROGNAME) Netscape "" export $(GUESS_CONFIG) $(NEED_YP_VERSION) "$(MCS_CMD)" $(NEED_NLS) "$(EXTRA_EXPORT_BINARY) $(EXTRA_REL_FILES)" $(README_FILE)

release-france: $(XFE_PROGNAME)-france $(EXTRA_FRANCE_OBJS) $(COMMON_REL_DEPS)
	./make-release $(OBJDIR) $(XFE_PROGNAME) Netscape "" france $(GUESS_CONFIG) $(NEED_YP_VERSION) "$(MCS_CMD)" $(NEED_NLS) "$(EXTRA_FRANCE_BINARY) $(EXTRA_REL_FILES)" $(README_FILE)

#---------------- release-net targets follow --------------------#

release-net-us: $(XFE_PROGNAME)-net-us $(COMMON_REL_DEPS)
	./make-release $(OBJDIR) $(XFE_PROGNAME) Netscape "N" net-us $(GUESS_CONFIG) $(NEED_YP_VERSION) "$(MCS_CMD)" $(NEED_NLS) "$(EXTRA_REL_FILES)" $(README_FILE)

release-net-export: $(XFE_PROGNAME)-net-export $(EXTRA_NETEXPORT_OBJS) $(COMMON_REL_DEPS)
	./make-release $(OBJDIR) $(XFE_PROGNAME) Netscape "N" net-export $(GUESS_CONFIG) $(NEED_YP_VERSION) "$(MCS_CMD)" $(NEED_NLS) "$(EXTRA_REL_FILES)" $(README_FILE)

release-net-france: $(XFE_PROGNAME)-net-france $(EXTRA_NETEXPORT_OBJS) $(COMMON_REL_DEPS)
	./make-release $(OBJDIR) $(XFE_PROGNAME) Netscape "N" net-france $(GUESS_CONFIG) $(NEED_YP_VERSION) "$(MCS_CMD)" $(NEED_NLS) "$(EXTRA_REL_FILES)" $(README_FILE)

#---------------- release-sgi targets follow --------------------#

release-sgi-us: $(XFE_PROGNAME)-sgi-us $(COMMON_REL_DEPS)
	./make-release $(OBJDIR) $(XFE_PROGNAME) Netscape "S" sgi-us $(GUESS_CONFIG) $(NEED_YP_VERSION) "$(MCS_CMD)" $(NEED_NLS) $(EXTRA_REL_FILES) $(README_FILE)

release-sgi-export: $(XFE_PROGNAME)-sgi-export $(COMMON_REL_DEPS)
	./make-release $(OBJDIR) $(XFE_PROGNAME) Netscape "S" sgi-export $(GUESS_CONFIG) $(NEED_YP_VERSION) "$(MCS_CMD)" $(NEED_NLS) "$(EXTRA_REL_FILES)" $(README_FILE)

release-sgi-france: $(XFE_PROGNAME)-sgi-france $(COMMON_REL_DEPS)
	./make-release $(OBJDIR) $(XFE_PROGNAME) Netscape "S" sgi-france $(GUESS_CONFIG) $(NEED_YP_VERSION) "$(MCS_CMD)" $(NEED_NLS) "$(EXTRA_REL_FILES)" $(README_FILE)

#######################################################################
# Java Jar File
#
# The definative shipping JAR files are now made in the win32 build
# this is just for developers

zips: $(JAR_NAME)

$(XPDIST)/classes/$(JAR_NAME): $(BUILD_NUMBER) Makefile $(ALL_CLASS_DEPS)
	@rm -rf $@
	@echo "Building `basename $@`"
	cd $(XPDIST)/classes;					\
	if test ! -d META-INF; then mkdir META-INF; fi;		\
	for d in $(MOZILLA_ZIP); do				\
		files=`find $$d \( -name '*.class' -o -name '*.gif' -o -name '*.props' -o -name '*.mf' -o -name '*.sf' -o -name '*.rsa' -o -name '*.properties*' \) -print`;			\
		if test -n $${files:-''}; then $(ZIP) $@ $$files ; fi;	\
	done;							\

$(JAR_NAME): $(XPDIST)/classes/$(JAR_NAME)

#
# symbols.gts for Irix, used by gtscc driver. The file needs to
# be stored in ns/config/symbols.gts. There should be a version
# checked in there. Use this rule only if you get link errors for
# C++ members functions which you reference globally.
# This rule is designed to work on IRIX or Solaris. If it works
# elsewhere fine, but...  djw Feb/14/97
#
$(OBJDIR)/symbols.gts: $(OBJS) $(US_OBJS) $(US_DEPLIBS)
	@$(MAKE_OBJDIR)
	rm -f $@
	nm -p $(OBJS) $(US_OBJS) $(US_DEPLIBS) > $(OBJDIR)/gts_a
	sed -n 's%.* T \(.*\)%\1%p' $(OBJDIR)/gts_a | sort -u > $(OBJDIR)/gts_d
	sed -n 's%.* U \(.*\)%\1%p' $(OBJDIR)/gts_a | sort -u > $(OBJDIR)/gts_u
	comm -23 $(OBJDIR)/gts_d $(OBJDIR)/gts_u > $@
	rm -f $(OBJDIR)/gts_d $(OBJDIR)/gts_u $(OBJDIR)/gts_a

symbols.gts: $(OBJDIR)/symbols.gts

export:: $(JAR_NAME)

$(VERSION_OBJS): versionn.h

everything:	all $(ALL_TARGETS)

symbols:
	@echo "ALL_TARGETS = $(ALL_TARGETS)"
	@echo "RELEASES    = $(RELEASES)"
	@echo "OS_ARCH     = $(OS_ARCH)"
