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
# There are N dimensions to a full complement of binaries:
#
#	- target architecture
#	- debug   vs  optimized
#	- normal  vs  "lite"
#	- normal  vs  purified
#   - dynamic vs  statically linked
#
# A directory is needed for each of the first two dimensions (target
# and debug), but executables for all of the others will then be built
# in the same directory from the same .o files.  The files have names
# as follows:
#
#   moz-export          (Dynamically linked with libs in 
#                        ../../dist/<objdir>/bin)
#	mozilla-export      (Statically linked)
#	mozilla-nis-export	(SunOS 4 only)
#	mozilla-export.pure	(SunOS 4 and Solaris only)
#	 ...etc...
#
# The only difference in the executables is which version of config-*.o
# is linked in, and whether or not -lresolv is used.
#
##########################################################################

DEPTH		= ../..
MODULE		= xfe

USE_3PANE	= 1

MICROLINE_LIB	= $(DIST)/lib/libXmL.a

#
# XFE2_DIR:
#
# HPUX_XMU_EXTRA_DIRS: On some HP-UX releases there is no Xmu.  So you
#                      you might need to build your own.  See ftp://ftp.x.org
#
# MOTIF_HACK_DIRS:     Some bug fixes require Motif source code access.
#                      Such hackery cannot be released because of OSF/Motif
#                      licensing restrictions.
#

include $(DEPTH)/config/config.mk

DIRS		= \
		  $(XFE2_DIR) \
		  $(HPUX_XMU_EXTRA_DIRS) \
		  $(MOTIF_HACK_DIRS) \
		  icons \
		  DtWidgets \
		  XfeWidgets \
		  Microline3.0/XmL \
		  $(NULL)

REQUIRES	=\
			XfeWidgets \
			XfeBm \
			Microline \
		  addr \
		  applet \
		  xpcom \
		  caps \
		  img \
		  edtplug \
		  jtools \
		  lay \
		  layer \
		  js \
		  libfont \
		  mariner \
		  msg \
		  plds \
		  nspr20 \
		  parse \
		  plug \
		  hook \
		  pref \
		  rdf \
		  xml \
		  security \
		  softupdt \
		  libreg \
		  style \
		  util \
		  java \
		  ldap \
		  xfeicons \
		  $(NULL)

CSRCS		= \
		  pref_helpers.c \
		  colors.c \
		  colorpicker.c \
		  altmail.c \
		  dragdrop.c \
		  e_kit.c \
		  fonts.c \
		  forms.c \
		  locale.c \
		  menu.c \
		  new_manage.c \
		  outline.c \
		  passwd.c \
		  scroller.c \
		  strids.c \
		  strings.c \
		  visual.c \
		  MozillaWm.c \
		  lite_stubs.c \
		  commands.c \
		  dialogs.c \
		  hot.c \
		  icons.c \
		  images.c \
		  lay.c \
		  mozilla.c \
		  prefs.c \
		  prefdialogs.c \
		  remote-s.c \
		  scroll.c \
		  selection.c \
		  xfe.c \
		  editor.c \
		  editordialogs.c \
		  $(NULL)

ifdef MOZ_MAIL_NEWS
CSRCS		+= \
		  addrbk.c \
		  mailattach.c \
		  mailcompose.c \
		  movemail.c \
		  $(NULL)
endif

ifndef NO_UNIX_ASYNC_DNS
CSRCS		+= xfe-dns.c
endif

XFE_PROGNAME	:= mozilla
DSO_PROGNAME	:= moz

GUESS_CONFIG	:= $(shell $(DEPTH)/config/config.guess | sed 's/i[23456]86/x86/')

#######################################################################

CCLD		= $(CCC)
LDFLAGS		= $(CFLAGS)
NOMD_LDFLAGS	= $(NOMD_CFLAGS)

ifdef MOZILLA_GPROF
CSRCS		+= gmon.c
LDFLAGS		= $(OPTIMIZER)
$(OBJDIR)/gmon.o:	gmon.c gmon.h
	$(CC) -O -c -o $@ $<
endif

ifdef SUB_UI
CSRCS		+= subui.c	
endif

ifndef NO_LAYERS
CSRCS		+= region.c
endif

ifeq ($(OS_ARCH),HP-UX)
#
# HP requires C++ programs to have main() in a C++ file.
#
CPPSRCS		= cplusplusmain.cc
CFLAGS		+= -DCPLUSPLUS_LINKAGE
endif

OBJS		= $(CSRCS:.c=.o) $(CPPSRCS:.cc=.o)

GARBAGE		+= e_kit_resources.h cxxlink-filter

ICONS_LIB	= $(DIST)/lib/lib$(LITE_PREFIX)xfeicons.a

DTWIDGETS_LIB		= $(DIST)/lib/libDtWidgets.a
XFE_WIDGETS_LIB		= $(DIST)/lib/libXfeWidgets.a
XFE_WIDGETS_BM_LIB	= $(DIST)/lib/libXfeBm.a

ifeq ($(OS_ARCH),AIX)
NSPR_LIB	= 
else
NSPR_LIB	= $(DIST)/lib/libplds21.a $(DIST)/lib/libplc21.a $(DIST)/lib/libmsgc21.a $(DIST)/lib/libnspr21.a
endif

ifdef DBMALLOC
NSPR_LIB	+= $(DIST)/lib/libdbmalloc.a
endif

XFE2_DIR	= src
XFE2_LIB	= $(DIST)/lib/lib$(LITE_PREFIX)xfe2.a

ifndef DISABLE_MARINER
CFLAGS		+= -DENABLE_MARINER
endif

#
# We need libnet.a in there twice because libmsg and libnet have circular
# dependencies on functions.
#
BASIC_LIBS	= \
		  $(XFE2_LIB) \
		  $(MICROLINE_LIB) \
		  $(ICONS_LIB) \
		  $(DTWIDGETS_LIB) \
		  $(XFE_WIDGETS_BM_LIB) \
		  $(XFE_WIDGETS_LIB) \
		  $(DIST)/lib/libxlate.a \
		  $(DIST)/lib/libmimetype.a \
		  $(DIST)/lib/libnetcache.a	\
		  $(DIST)/lib/libnetcnvts.a	\
		  $(DIST)/lib/libnetwork.a	\
		  $(DIST)/lib/libnetcnvts.a	\
		  $(DIST)/lib/libcnetinit.a	\
		  $(DIST)/lib/libabouturl.a	\
		  $(DIST)/lib/libdataurl.a	\
		  $(DIST)/lib/libfileurl.a	\
		  $(DIST)/lib/libftpurl.a	\
		  $(DIST)/lib/libgophurl.a	\
		  $(DIST)/lib/libhttpurl.a	\
		  $(DIST)/lib/libjsurl.a	\
		  $(DIST)/lib/libmarimurl.a	\
		  $(DIST)/lib/libremoturl.a	\
		  $(DIST)/lib/libnetwork.a	\
		  $(DIST)/lib/lib$(LITE_PREFIX)rdf.a \
		  $(DIST)/lib/lib$(LITE_PREFIX)xml.a \
		  $(DIST)/lib/lib$(LITE_PREFIX)lay.a \
		  $(DIST)/lib/libpng.a \
		  $(DIST)/lib/libmariner.a \
		  $(DIST)/lib/libimg.a \
		  $(NULL)

ifdef MOZ_LOC_INDEP
BASIC_LIBS	+= $(DIST)/lib/libli.a
endif

ifdef MOZ_JAVA
JAVA_JMC = $(DIST)/lib/libjmc.a
endif

BASIC_LIBS	+= \
		  $(DIST)/lib/libpng.a \
		  $(JAVA_JMC) \
		  $(DIST)/lib/libjpeg.a \
		  $(DIST)/lib/libhook.a \
		  $(DIST)/lib/libparse.a \
		  $(DIST)/lib/lib$(LITE_PREFIX)pref.a \
		  $(DIST)/lib/lib$(LITE_PREFIX)i18n.a \
		  $(DIST)/lib/libpics.a \
		  $(DIST)/lib/libpwcac.a \
		  $(DIST)/lib/libreg.a \
		  $(NULL)

BASIC_LIBS_2	= \
		  $(DIST)/lib/lib$(LITE_PREFIX)xp.a \
		  $(DIST)/lib/libdbm.a \
		  $(NULL)

ifdef MOZ_MAIL_NEWS
BASIC_LIBS	+= \
		  $(DIST)/lib/libmsg.a \
		  $(DIST)/lib/libnet.a \
		  $(NULL)
endif

BASIC_LIBS	+= \
		  $(DIST)/lib/libmisc.a \
		  $(NULL)

ifdef MOZ_MAIL_NEWS
BASIC_LIBS	+= \
		  $(DIST)/lib/libaddr.a \
		  $(NULL)
endif

ifdef MOZ_NEO
BASIC_LIBS	+= \
		  $(DIST)/lib/libneo.a \
		  $(NULL)
endif

ifdef MOZ_MAIL_NEWS
BASIC_LIBS	+= \
		  $(DIST)/lib/libmime.a \
		  $(NULL)
endif

BASIC_LIBS	+= \
		  $(DIST)/lib/lib$(LITE_PREFIX)plug.a \
		  $(DIST)/lib/libutil.a \
		  $(DIST)/lib/libfont.a \
		  $(DIST)/lib/libprgrss.a \
		  $(NULL)

ifndef NO_LAYERS
BASIC_LIBS	+= $(DIST)/lib/liblayer.a
endif

BASIC_LIBS += $(DIST)/lib/libxpcom.a

ifdef MOZ_JAVA

BASIC_LIBS	+= \
                  $(DIST)/lib/libcaps.a \
		  $(DIST)/lib/lib$(LITE_PREFIX)applet.a \
		  $(DIST)/lib/libjrt.a \
		  $(DIST)/lib/libjmd.a \
		  $(NULL)

ifdef EDITOR
BASIC_LIBS	+= $(DIST)/lib/libedtplug.a
endif

BASIC_LIBS	+= \
		  $(DIST)/lib/libnsn.a \
		  $(DIST)/lib/libnsc.a \
		  $(DIST)/lib/libcaps.a \
		  $(DIST)/lib/libxpcom.a \
		  $(DIST)/lib/libjpw.a \
		  $(DIST)/lib/libzpw.a \
		  $(DIST)/lib/libiawt.a \
		  $(DIST)/lib/libmmedia.a \
		  $(DIST)/lib/libsoftupdate.a \
		  $(DIST)/lib/libcon.a \
		  $(DIST)/lib/libjbn.a \
		  $(NULL)

ifndef NO_SECURITY
BASIC_LIBS	+= $(DIST)/lib/libjsl.a
endif

ifdef MOZ_MAIL_NEWS
BASIC_LIBS	+= $(DIST)/lib/libjsl.a
endif

BASIC_LIBS	+= $(DIST)/lib/libjrt.a

else  # MOZ_JAVA

BASIC_LIBS	+= \
		  $(DIST)/lib/libstubsj.a \
		  $(DIST)/lib/libstubnj.a \
		  $(NULL)

endif  # MOZ_JAVA

BASIC_LIBS	+= $(DIST)/lib/libzlib.a

ifndef NO_MOCHA
BASIC_LIBS	+= $(DIST)/lib/libjs.a $(DIST)/lib/libjsj.a $(DIST)/lib/libmocha.a
endif

# Post-Java libs
BASIC_LIBS	+= $(DIST)/lib/libstyle.a

ifndef NO_SECURITY
BASIC_LIBS	+= $(DIST)/lib/libjar.a
else
EXPORT_LIB	= $(DIST)/lib/libhtmldlgs.a $(DIST)/lib/libsecfree.a
endif

ifdef MOZ_LDAP
BASIC_LIBS	+= $(DIST)/lib/libldap.a $(DIST)/lib/liblber.a 
endif

LOCALES		= $(LOCALE_MAP) $(MAIL_IM_HACK) $(NEWS_IM_HACK)

ALL_EXPORT_LIBS	= $(BASIC_LIBS) $(EXPORT_LIB) $(BASIC_LIBS_2) $(NSPR_LIB)

ALL_EXPORT_DSOS	= $(ALL_EXPORT_LIBS:$(DIST)/lib/lib%.a=-l%)
PLUGIN_DSO	= $(DIST)/bin/libnullplugin.so
#
# WEBFONT dso is not in the OFFICAL build yet. It needs to be hooked in
# from the private tree
#
WEBFONT_DSO	=

ifdef BUILD_UNIX_PLUGINS
TEST_PLUGIN	= $(DIST)/bin/libtextplugin.so
endif

ifdef MKSHLIB
ifndef NO_NETSCAPE_SHARED
ifndef NO_BUILD_EXPORT
TARGETS		+= $(OBJDIR)/$(DSO_PROGNAME)-export
endif
endif
endif

ifndef NO_NETSCAPE_STATIC
ifndef NO_BUILD_EXPORT
TARGETS		+= $(OBJDIR)/$(XFE_PROGNAME)-export
endif
endif

X_OBJS		= $(OBJDIR)/$(LITE_PREFIX)config-YYY.o \
		  $(OBJDIR)/license.o \
		  $(OBJDIR)/resources-YYY.o

EXPORT_OBJS	= $(subst YYY,export,$(X_OBJS))
NIS_EXPORT_OBJS	= $(subst YYY,nis-export,$(X_OBJS))

# If the version number changes, all these need to be rebuilt.
# (config-* doesn't need to be in here, since that already has a rule.)
VERSION_OBJS	= $(OBJDIR)/license.o \
		  $(OBJDIR)/resources-export.o \
		  $(OBJDIR)/resources-nis-export.o

#######################################################################

-include $(XFEPRIVDIR)Netscape.mk
include $(DEPTH)/config/rules.mk

NS_RULES_MK	= 1
-include $(XFEPRIVDIR)Netscape.mk

DEFINES		+= -DNEW_DECODERS

ifdef USE_3PANE
DEFINES		+= -DUSE_3PANE
endif

ifdef USE_ABCOM
DEFINES		+= -DUSE_ABCOM
endif

INCLUDES	+= -Isrc -I.


#######################################################################
#
# Set defaults for all platforms.  Each OS_ARCH will override this if
# necessary.
#

#
# Developers won't need libTrueDoc.so.  No point having a dependency
# on this when only official builds (release builds) need this.
#
ifndef BUILD_OFFICIAL
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

DSO_EX_LIBS	= $(EXPORT_LDFLAGS:$(DIST)/lib/lib%.a=-l%)

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
OTHER_LIBS	= $(MOTIFLIB) -lXt -lXmu -lX11 $(OS_LIBS)
US_LDFLAGS	+= $(AIX_NSPR_LINK)
EXPORT_LDFLAGS	+= $(AIX_NSPR_LINK)
FRANCE_LDFLAGS	+= $(AIX_NSPR_LINK)
EXTRA_REL_FILES	+= $(AIX_NSPR)

ifndef NO_WEBFONTS
EXTRA_REL_FILES += $(WEBFONT_DSO)
endif

endif

########################################
# FreeBSD
ifeq ($(OS_ARCH),FreeBSD)
OTHER_LIBS	= /usr/X11R6/lib/libXm.a -L/usr/X11R6/lib -lXt -lXmu -lXext -lX11 -lSM -lICE -lm $(OS_LIBS)
endif

########################################
# BSDI
ifeq ($(OS_ARCH),BSD_OS)
OTHER_LIBS	= $(MOTIF)/libXm.a $(LOC_LIB_DIR)/libXt.a $(X11R6LIBS) $(LOC_LIB_DIR)/libXmu.a \
		  $(LOC_LIB_DIR)/libXext.a $(LOC_LIB_DIR)/libX11.a -lm $(OS_LIBS)

ifeq ($(OS_RELEASE),2.1)
X11R6LIBS	= $(LOC_LIB_DIR)/libSM.a $(LOC_LIB_DIR)/libICE.a
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

OTHER_LIBS	= -L/usr/lib/X11R5 -L/usr/lib/Motif1.2 -lXm -L$(DIST)/lib -lXmu -lXt -lX11 -lXext $(OS_LIBS)

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
OTHER_LIBS	= -lSgm $(MOTIFPATCH_LIB) -lXm -lXmu -lXt -lX11 -lXext -lgen -laudio -lm $(OS_LIBS)

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

# If the motif.mk file exists, it might define extra flags needed for
# specific versions of motif.
#
# Currently there is only 2 flags:
#
# MOZILLA_XM_2_1_PRINT_SHELL_FLAGS: X Print Shell Extension available
# starting with X11R6.3 and needed by motif 2.1.
#
# MOZILLA_XM_2_1_BROKEN_LOCALE_FLAGS: Needed because of currently
# broken locale support in motif 2.1 when linked with gnu libc2.
#
# Please direct questions about motif and linux to ramiro@netscape.com.
#
-include $(DEPTH)/config/motif.mk

XTOOLLIB	= -L/usr/X11R6/lib -lXt -lSM -lICE
XLIB		= -lXext -lX11
XMULIB		= -lXmu -lXpm

ifeq ($(OS_RELEASE),2.0)
ifeq ($(CPU_ARCH),ppc)
MOTIFLIB	= /usr/local/lib/libXm.a
else

MOTIFLIB	=\
			$(MOTIFPATCH_LIB) \
			$(MOZILLA_XM_2_1_BROKEN_LOCALE_FLAGS) \
			-L/usr/X11R6/lib -lXm \
			$(MOZILLA_XM_2_1_PRINT_SHELL_FLAGS)


endif
else
MOTIFLIB	= /usr/X11R6/lib/libXm.a
endif

OTHER_LIBS	= $(MOTIFLIB) $(XTOOLLIB) $(XMULIB) $(EXTENSIONLIB) $(XLIB) -lm -ldl

#
# This assumes there is a libXm.so on the system.  If there isn't, you'll
# get two identical binaries.
#
ifeq ($(OS_RELEASE)$(CPU_ARCH),2.0x86)

DYN_MOTIFLIB		=\
					$(MOTIFPATCH_LIB) \
					$(MOZILLA_XM_2_1_BROKEN_LOCALE_FLAGS) \
					-lXm \
					$(MOZILLA_XM_2_1_PRINT_SHELL_FLAGS)


EXTRA_EXPORT_OBJS	= $(OBJDIR)/$(XFE_PROGNAME)-motif-export
EXTRA_EXPORT_BINARY	= $(EXTRA_EXPORT_OBJS)

PLUGIN_DSO	+= $(DIST)/bin/libnullplugin-dynMotif.so

ifndef NO_WEBFONTS
EXTRA_REL_FILES += $(WEBFONT_DSO)
endif

endif

EXPORT_LDFLAGS	+= -lc

endif

########################################
# NCR SYSV 4.0
ifeq ($(OS_ARCH),NCR)
OTHER_LIBS	= -L/usr/X/lib $(MOTIFLIB) -lXt -lXmu -lXext -lX11 -lgen -lm $(OS_LIBS)
endif

########################################
# NEC SYSV 4.2
ifeq ($(OS_ARCH),NEC)
OTHER_LIBS	= -L/usr/abiccs/lib/X11R5 $(MOTIFLIB) -lXt -lXmu -lXext -lX11 -lresolv -lgen -lm $(OS_LIBS)
endif

########################################
# Dec Machines
ifeq ($(OS_ARCH),OSF1)

#
# We would like to link OSF1 static.
# This is because of motif problems (BadMatch errors on non-default visual)
# on 3.2 and 3.0 (and not on 2.0).
# But libX11 needs to be dynamic, otherwise the locale stuff doesn't work
# and you get warnings and core dump when pasting into Mozilla.
# Also, libXm must be dynamic, otherwise Japanese text widgets hang
# the process.
# 
OTHER_LIBS	= -lXm -lXmu -lXt -lX11 -lXext -ldnet_stub -lm -lots $(OS_LIBS)

EXPORT_LDFLAGS	= $(BASIC_LIBS) $(EXPORT_LIB) $(BASIC_LIBS_2) $(NSPR_LIB) $(OTHER_LIBS)

endif

########################################
# SNI ReliantUNIX (SINIX)
ifeq ($(OS_ARCH),SINIX)
OTHER_LIBS	= -lXm -lXmu -lXt -lX11 -lXext $(OS_LIBS)
endif

########################################
# SCO OpenServer
ifeq ($(OS_ARCH),SCOOS)
OTHER_LIBS	= $(LIB_XMOS) -lXm -lXt -lXmu -lXext -lX11 -lm -lPW $(OS_LIBS)

ifdef NEED_XMOS
LIB_XMOS	= $(OBJDIR)/Xmos.o
endif

endif

########################################
# Sun Machines
ifeq ($(OS_ARCH),SunOS)

ifeq ($(OS_RELEASE),4.1)

ALL_TARGETS	+= $(OBJDIR)/$(XFE_PROGNAME)-nis-export

OTHER_LIBS	= -L$(MOTIF)/lib $(MOTIF)/lib/libXm.a $(MOTIF)/lib/libXmu.a $(MOTIF)/lib/libXt.a \
		  $(MOTIF)/lib/libXext.a $(MOTIF)/lib/libX11.a

ifndef NO_WEBFONTS
EXTRA_REL_FILES += $(WEBFONT_DSO)
endif

ifndef NO_EDITOR
OTHER_LIBS	+= -L$(NS_LIB)

#
# Need this guy because the one in libg++.a is (still?) broken.
#
BASIC_LIBS	+= $(OBJDIR)/regex.o
$(OBJDIR)/regex.o:	/lib/libc.a
	ar x /lib/libc.a regex.o && mv regex.o $@
endif

#
# Need our own popen to fix the SunOS popen problem.
#
BASIC_LIBS	+= $(OBJDIR)/popen.o
CSRCS		+= popen.c

EXPORT_LDFLAGS	+= -lm

# SunOS had 2 executables. Only the non-nis version need this
NIS_OBJS	= $(OBJDIR)/dns-stub.o
NIS_SRCS	= dns-stub.c
NIS_LIB		= $(NIS_OBJS)
DNS_LIB		= -lresolv

# For release only.
# This is only for SunOS as it has both nis and non-nis
# version of the browser packaged together.
EXTRA_EXPORT_OBJS	= $(XFE_PROGNAME)-nis-export

NEED_YP_VERSION	= 1
NEED_NLS	= 1

endif

########################################
ifneq (,$(filter 5 5.5,$(OS_RELEASE)))

USRLIBDIR	:= /usr/openwin/lib
MCS_CMD		= mcs -d

ifeq ($(CPU_ARCH),sparc)
OTHER_LIBS	= $(MOTIFPATCH_LIB) $(MOTIFLIB) -lXt -lXmu -lXext -lX11 $(OS_LIBS) -lgen -lresolv -lm
ifndef NO_WEBFONTS
EXTRA_REL_FILES += $(WEBFONT_DSO)
endif
else
OTHER_LIBS	= $(MOTIFLIB) -lXt -lXmu -lXext -lX11 $(OS_LIBS) -lm
LDFLAGS		= $(NOMD_CFLAGS)
endif

EXPORT_LDFLAGS	= -z defs -L$(MOTIF)/lib -L$(USRLIBDIR) -R$(MOTIF)/lib \
		  -R$(USRLIBDIR) $(BASIC_LIBS) $(EXPORT_LIB) $(BASIC_LIBS_2) \
		  $(OTHER_LIBS) $(NSPR_LIB)

endif
endif

########################################
# SCO Unixware 2.1
ifeq ($(OS_ARCH),UNIXWARE)

ifdef NEED_XMOS
LIB_XMOS	= $(OBJDIR)/Xmos.o
endif

OTHER_LIBS	= $(LIB_XMOS) -L/usr/X/lib -lXm -lXt -lXmu -lXext -lX11 -lm

EXPORT_LDFLAGS	+= -lresolv -lsocket -lc /usr/ucblib/libucb.a

endif

#######################################################################

$(MICROLINE_LIB):
	cd Microline3.0/XmL; $(MAKE)

$(ICONS_LIB):
	cd icons; $(MAKE)

$(DTWIDGETS_LIB):
	cd DtWidgets; $(MAKE)

$(XFE_WIDGETS_LIB):
	cd XfeWidgets/Xfe; $(MAKE)

$(XFE_WIDGETS_BM_LIB):
	cd XfeWidgets/XfeBm; $(MAKE)

$(XFE2_LIB):
	cd src; $(MAKE)

$(TARGETS): $(OBJS)

#######################################################################
# Rules to build license.o files

$(OBJDIR)/license.o: $(OBJDIR)/LICENSE.c Makefile
	$(CC) -c -o $@ $<

ifndef NETSCAPE_MK
$(OBJDIR)/LICENSE.c:
	@echo 'const char fe_LicenseData[] = "This license intentionally left blank.";' > $@
endif

#######################################################################
# Rules to build config*.o files

$(OBJDIR)/$(LITE_PREFIX)config-export.o: config.c versionn.h Makefile $(OBJS) $(EXPORT_DEPLIBS)
	@echo Generating $@ from config.c...;				      \
	X=' ';								      \
	VN=`sed -n$$X 's/^#define VERSION_NUMBER[ 	]*\(.*\)$$/\1/p' versionn.h` ; \
	$(CC) -c $(CFLAGS) -o $@ config.c				      \
		-DCONFIG="$(GUESS_CONFIG)"				      \
		-DDATE="`date +%d-%h-%y`"				      \
		-DVERSION=$${VN}					      \
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

$(OBJDIR)/Netscape-nis-export.ad $(OBJDIR)/Netscape-export.ad: Makefile resources versionn.h strs make-resources $(LOCALE_MAP)
	@./make-resources $@ Netscape Netscape "" export $(LOCALES)

#######################################################################
# The "-export" targets

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

$(OBJDIR)/$(XFE_PROGNAME)-nis-export: $(OBJS) $(NIS_EXPORT_OBJS) $(NIS_OBJS) $(EXPORT_DEPLIBS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(CCLD) -o $@ $(LDFLAGS) $(OBJS) $(NIS_EXPORT_OBJS) $(EXPORT_LDFLAGS) $(NIS_LIB)
	@$(EXTRA_POST_LINK_CMD) $@

#
# Build a dynamic Motif version.
#
ifeq ($(OS_ARCH)$(OS_RELEASE)$(CPU_ARCH),Linux2.0x86)

$(OBJDIR)/$(XFE_PROGNAME)-motif-export: $(OBJS) $(EXPORT_OBJS) $(EXPORT_DEPLIBS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(CCLD) -o $@ $(LDFLAGS) $(OBJS) $(EXPORT_OBJS) $(BASIC_LIBS) $(EXPORT_LIB) $(BASIC_LIBS_2) -L/usr/X11R6/lib $(DYN_MOTIFLIB) -lXt -lSM -lICE $(XMULIB) $(EXTENSIONLIB) $(XLIB) -lm -ldl $(NSPR_LIB) $(DNS_LIB) -lc
	@$(EXTRA_POST_LINK_CMD) $@
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

$(VERSION_OBJS): versionn.h

#
# For some reason the -include's in this file aren't working,
# so until I figure it out, if Netscape.mk doesn't exist, just
# create a dummy file. --briano
#
$(XFEPRIVDIR)Netscape.mk:
ifdef XFEPRIVDIR
	@-mkdir -p $(XFEPRIVDIR)
endif
	@touch $@

everything:	all $(ALL_TARGETS)

symbols:
	@echo "ALL_TARGETS = $(ALL_TARGETS)"
	@echo "RELEASES    = $(RELEASES)"
	@echo "OS_ARCH     = $(OS_ARCH)"
