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
include $(DEPTH)/cmd/unixfe/config/unixfe.mk
include $(DEPTH)/cmd/unixfe/config/motif.mk

DIRS		= \
		  $(XFE2_DIR) \
		  $(HPUX_XMU_EXTRA_DIRS) \
		  $(MOTIF_HACK_DIRS) \
		  icons \
		  DtWidgets \
		  XfeWidgets \
		  Microline3.0/XmL \
		  $(NULL)

REQUIRES	= \
		  XfeWidgets \
		  XfeBm \
		  XfeToolBar \
		  XfeToolTip \
		  XfeComboBox \
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
		  plds \
		  nspr20 \
		  parse \
		  plugin \
		  plugimpl \
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
		  $(NULL)

ifdef MOZ_LDAP
REQUIRES	+= ldap
endif

ifdef MOZ_MAIL_NEWS
REQUIRES	+= msg
else
ifdef MOZ_MAIL_COMPOSE
REQUIRES	+= mailto
endif
endif

REQUIRES	+= \
		  xfeicons \
		  progress \
		  privacy \
		  $(NULL)

ifdef MOZ_FULLCIRCLE
REQUIRES	+= fullsoft
endif

ifdef OJI
REQUIRES	+= oji ojiimpl
endif

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
		  commands.c \
		  dialogs.c \
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
		  addrbk.c \
		  $(NULL)

ifndef MOZ_MAIL_NEWS
CSRCS		+= lite_stubs.c \
		  $(NULL)
endif

ifdef MOZ_MAIL_NEWS
CSRCS		+= \
		  mailattach.c \
		  mailcompose.c \
		  movemail.c \
		  $(NULL)
else
ifdef MOZ_MAIL_COMPOSE
CSRCS		+= \
		  mailattach.c \
		  mailcompose.c \
		  $(NULL)
endif
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
BASIC_DSOS	= -L$(DIST)/bin

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

DTWIDGETS_LIB				= $(DIST)/lib/libDtWidgets.a
XFE_WIDGETS_LIB				= $(DIST)/lib/libXfeWidgets.a
XFE_WIDGETS_BM_LIB			= $(DIST)/lib/libXfeBm.a
XFE_WIDGETS_COMBO_BOX_LIB	= $(DIST)/lib/libXfeComboBox.a
XFE_WIDGETS_CAPTION_LIB		= $(DIST)/lib/libXfeCaption.a
XFE_WIDGETS_TOOL_BAR_LIB	= $(DIST)/lib/libXfeToolBar.a
XFE_WIDGETS_TOOL_TIP_LIB	= $(DIST)/lib/libXfeToolTip.a

DYNAMIC_SYMBOLS_F	=
ifeq (,$(filter FreeBSD IRIX OpenBSD OSF1 QNX Rhapsody,$(OS_ARCH)))
DYNAMIC_SYMBOLS_F	= -rdynamic
endif
ifeq ($(OS_ARCH),SunOS) 
ifdef NS_USE_NATIVE
DYNAMIC_SYMBOLS_F	= -Bdynamic
endif
endif

ifeq ($(OS_ARCH),AIX)
NSPR_LIB	= 
else
# XXX To a DSO...
ifdef FULL_STATIC_BUILD
NSPR_LIB	= $(DIST)/lib/libplds21.a $(DIST)/lib/libplc21.a $(DIST)/lib/libnspr21.a
else
BASIC_DSOS	+= -lplds21 -lplc21 -lnspr21
endif
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
		  $(XFE_WIDGETS_TOOL_BAR_LIB) \
		  $(XFE_WIDGETS_TOOL_TIP_LIB) \
		  $(XFE_WIDGETS_COMBO_BOX_LIB) \
		  $(XFE_WIDGETS_LIB) \
		  $(DIST)/lib/libxlate.a \
		  $(DIST)/lib/libmimetype.a \
		  $(DIST)/lib/libnetwork.a	\
		  $(DIST)/lib/libnetcache.a	\
		  $(DIST)/lib/libnetcnvts.a	\
		  $(DIST)/lib/libnetutil.a \
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
		  $(DIST)/lib/lib$(LITE_PREFIX)rdf.a \
		  $(DIST)/lib/lib$(LITE_PREFIX)xml.a \
		  $(DIST)/lib/lib$(LITE_PREFIX)lay.a \
		  $(DIST)/lib/libmariner.a \
		  $(DIST)/lib/libimg.a \
		  $(DIST)/lib/libnetwork.a	\
		  $(DIST)/lib/libprivacy.a \
		  $(NULL)

ifdef MOZ_LOC_INDEP
BASIC_LIBS	+= $(DIST)/lib/libli.a
endif

ifdef JAVA_OR_NSJVM
JAVA_JMC	= $(DIST)/lib/libjmc.a	# XXX To be removed...
endif

BASIC_LIBS	+= \
		  $(JAVA_JMC) \
		  $(DIST)/lib/libhook.a \
		  $(DIST)/lib/libparse.a \
		  $(DIST)/lib/lib$(LITE_PREFIX)pref.a \
		  $(DIST)/lib/libpics.a \
		  $(DIST)/lib/libpwcac.a \
		  $(DIST)/lib/libreg.a \
		  $(NULL)

BASIC_LIBS_2	= \
		  $(DIST)/lib/lib$(LITE_PREFIX)xp.a \
		  $(DIST)/lib/libdbm.a \
		  $(DIST)/lib/libcaps.a \
		  $(DIST)/lib/libxpcom.a \
		  $(DIST)/lib/lib$(LITE_PREFIX)rdf.a \
		  $(NULL)

# jwz: link in libmime all the time.
# but it needs to be before libnetutil, and I don't know how to do that,
# so just link against libnetutil twice.

BASIC_LIBS	+= \
		  $(DIST)/lib/libmime.a \
		  $(DIST)/lib/libnetutil.a \
		  $(NULL)

ifdef SMART_MAIL
BASIC_LIBS    += $(DIST)/lib/libpop3url.a
endif

ifdef MOZ_MAIL_NEWS
ifdef MOZ_SECURITY
BASIC_LIBS	+= $(DIST)/lib/libns_mime.a
endif
BASIC_LIBS_2	+= $(DIST)/lib/libldap.a
endif

ifdef SMART_MAIL
ifdef MOZ_SECURITY
BASIC_LIBS	+= \
	$(DIST)/lib/libns_mime.a \
	$(NULL)
endif
endif

# Do we need libns_mime.a here if MOZ_MAIL_COMPOSE and MOZ_SECURITY?
#ifdef MOZ_MAIL_COMPOSE
#ifdef MOZ_SECURITY
#BASIC_LIBS	+= $(DIST)/lib/libns_mime.a
#endif
#endif

BASIC_LIBS	+= \
		  $(DIST)/lib/libmisc.a \
		  $(DIST)/lib/libprgrss.a \
		  $(NULL)

ifdef MOZ_LDAP
BASIC_LIBS	+= \
		$(DIST)/lib/libldap.a \
		$(DIST)/lib/liblber.a \
		$(NULL)
endif

ifdef MOZ_MAIL_NEWS
BASIC_LIBS	+= \
		  $(DIST)/lib/libmozmsg.a \
		  $(NULL)
ifdef MOZ_SECURITY
BASIC_LIBS	+= $(DIST)/lib/libmsg.a \
		$(NULL)
endif
BASIC_LIBS	+= $(DIST)/lib/libaddr.a \
		  $(DIST)/lib/libneo.a \
		  $(DIST)/lib/libaddr.a \
		  $(NULL)
endif

ifdef MOZ_MAIL_COMPOSE
BASIC_LIBS	+= $(DIST)/lib/libmailto.a \
		  $(DIST)/lib/libmime.a \
		  $(DIST)/lib/libsmtpurl.a	\
		$(NULL)
endif

ifdef MOZ_MAIL_NEWS
BASIC_LIBS	+= $(DIST)/lib/libnntpurl.a \
		  $(DIST)/lib/libsmtpurl.a	\
		  $(DIST)/lib/libimap4url.a	\
		  $(DIST)/lib/libpop3url.a	\
		  $(DIST)/lib/libmailbxurl.a	\
		  $(DIST)/lib/libcrtldurl.a	\
		  $(NULL)
endif

ifdef MOZ_MAIL_NEWS
ifdef MOZ_LDAP
BASIC_LIBS	+= $(DIST)/lib/libldapurl.a \
		  $(NULL)
endif
endif

ifdef MOZ_CALENDAR
BASIC_LIBS	+= \
		  $(DIST)/lib/libjulian.a \
		  $(DIST)/lib/libnscnv30.a \
		  $(DIST)/lib/libnsuni30.a \
		  $(DIST)/lib/libnsfmt30.a \
		  $(NULL)
endif

BASIC_LIBS	+= \
		  $(DIST)/lib/lib$(LITE_PREFIX)plug.a \
		  $(DIST)/lib/libutil.a \
		  $(DIST)/lib/libfont.a \
		  $(NULL)

ifndef NO_LAYERS
BASIC_LIBS	+= $(DIST)/lib/liblayer.a
endif

########################################################################
# Java
#

ifdef MOZ_JAVA
#
# Monolithic Java
#

BASIC_LIBS	+= \
		  $(DIST)/lib/lib$(LITE_PREFIX)applet.a \
		  $(DIST)/lib/libjrt.a \
		  $(DIST)/lib/libjmd.a \
		  $(NULL)

ifdef MOZ_EDITOR
BASIC_LIBS	+= $(DIST)/lib/libedtplug.a
endif

BASIC_LIBS	+= \
		  $(DIST)/lib/libnsn.a \
		  $(DIST)/lib/libnsc.a \
		  $(DIST)/lib/libjpw.a \
		  $(DIST)/lib/libzpw.a \
		  $(DIST)/lib/libiawt.a \
		  $(DIST)/lib/libmmedia.a \
		  $(DIST)/lib/libsoftupdate.a \
		  $(DIST)/lib/libprgrss.a \
		  $(DIST)/lib/libcon.a \
		  $(DIST)/lib/libjbn.a \
		  $(NULL)

ifdef MOZ_SMARTUPDATE
BASIC_LIBS	+= $(DIST)/lib/libsoftupdate.a $(DIST)/lib/libprgrss.a
endif

ifdef MOZ_SECURITY
BASIC_LIBS	+= $(DIST)/lib/libjsl.a
endif

ifdef MOZ_MAIL_NEWS
BASIC_LIBS	+= $(DIST)/lib/libjsl.a
endif

BASIC_LIBS	+= $(DIST)/lib/libjrt.a

else # !MOZ_JAVA

#
# OJI
#
ifdef MOZ_OJI

BASIC_LIBS	+= $(DIST)/lib/liboji.a

ifdef NSJVM

ifdef MOZ_EDITOR
BASIC_LIBS	+= $(DIST)/lib/libedtplug.a
endif
else # !NSJVM
BASIC_LIBS	+= \
		  $(DIST)/lib/libstubsj.a \
		  $(DIST)/lib/libstubnj.a \
		  $(NULL)
endif # !NSJVM

BASIC_LIBS	+= $(DIST)/lib/libprgrss.a

else  # !MOZ_OJI
# No Monolithic Java, no OJI, just stubs.

BASIC_LIBS	+= \
		  $(DIST)/lib/libstubsj.a \
		  $(DIST)/lib/libstubnj.a \
		  $(NULL)

endif  # !MOZ_OJI
endif  # !MOZ_JAVA

##
## Graphics libs.  See mozilla/cmd/unixfe/config/unixfe.mk.
## 
BASIC_LIBS += \
			$(XFE_PNG_LIB) \
			$(XFE_ZLIB_LIB) \
			$(XFE_JPEG_LIB) \
			$(NULL)

##
## Graphics libs.  See unixfe.mk.
## 
BASIC_DSOS += \
			$(XFE_PNG_DSO) \
			$(XFE_ZLIB_DSO) \
			$(XFE_JPEG_DSO) \
			$(NULL)

# XXX To a DSO...
ifdef FULL_STATIC_BUILD
BASIC_LIBS	+= $(DIST)/lib/libjs.a $(DIST)/lib/libjsj.a
else
BASIC_DSOS	+= -ljs -ljsj
endif

BASIC_LIBS	+= $(DIST)/lib/libmocha.a \
		  $(DIST)/lib/lib$(LITE_PREFIX)i18n.a

ifdef DOM
# the DOM code calls back into layout.
BASIC_LIBS	+= $(DIST)/lib/liblay.a
endif

ifdef DOM
BASIC_LIBS	+= $(DIST)/lib/libdom.a
endif

# Post-Java libs
BASIC_LIBS	+= $(DIST)/lib/libstyle.a

ifdef MOZ_SECURITY
BASIC_LIBS	+= $(DIST)/lib/libjar.a 
else
EXPORT_LIB	= $(DIST)/lib/libhtmldlgs.a $(DIST)/lib/libsecfree.a 
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

ifdef XFE_PREF_ADVANCED_PASSIVE_FTP
DEFINES		+= -DXFE_PREF_ADVANCED_PASSIVE_FTP
endif

ifdef USE_3PANE
DEFINES		+= -DUSE_3PANE
endif

ifdef USE_ABCOM
DEFINES		+= -DUSE_ABCOM
endif

ifdef MOZILLA_XFE_USE_WINNING_FILE_SELECTION
DEFINES		+= -DUSE_WINNING_FILE_SELECTION
endif

INCLUDES	+= -Isrc -I. -I$(DEPTH)/dist/public/nls -I$(DEPTH)/dist/public/security \
		-I$(DEPTH)/dist/public/ldap \
		-I$(DEPTH)/modules/libimg/public \
		-I$(DEPTH)/l10n/us

ifdef MOZ_MAIL_NEWS
INCLUDES	+= -Isrc -I. -I$(DEPTH)/lib/libmsg
endif


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

#
# The only difference between EXPORT_LDFLAGS and 
# EXPORT_DYN_MOTIF_LDFLAGS is that OTHER_LIBS is
# replaced by OTHER_DYN_MOTIF_LIBS.
#
# OTHER_DYN_MOTIF_LIBS will use something like 
# '-L/foo/bar -lXm' instead of '/foo/bar/libXm.a'
#
EXPORT_LDFLAGS				=\
							$(BASIC_LIBS) \
							$(EXPORT_LIB) \
							$(BASIC_LIBS_2) \
							$(BASIC_DSOS) \
							$(NSPR_LIB) \
							$(OTHER_LIBS)

EXPORT_DYN_MOTIF_LDFLAGS 	=\
							$(BASIC_LIBS) \
							$(EXPORT_LIB) \
							$(BASIC_LIBS_2) \
							$(BASIC_DSOS) \
							$(NSPR_LIB) \
							$(OTHER_DYN_MOTIF_LIBS)

EXPORT_DEPLIBS				=\
							$(BASIC_LIBS) \
							$(EXPORT_LIB) \
							$(BASIC_LIBS_2) \
							$(NSPR_LIB)

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
#LDFLAGS		=  -bGnoproc
#
# ??? need to double check
#
LDFLAGS		= -bE:aix_exports -bGhooksyslibs -bGlibpathexec -bGnoproc -bmaxdata:20000000
OTHER_LIBS	= $(MOTIFLIB) -lXt -lXmu -lX11 $(OS_LIBS)
US_LDFLAGS	+= $(AIX_NSPR_LINK)
EXPORT_LDFLAGS	+= $(AIX_NSPR_LINK)
FRANCE_LDFLAGS	+= $(AIX_NSPR_LINK)
EXTRA_REL_FILES	+= $(AIX_NSPR)

ifndef NO_WEBFONTS
EXTRA_REL_FILES	+= $(WEBFONT_DSO)
endif

endif

########################################
# FreeBSD
ifeq ($(OS_ARCH),FreeBSD)
OTHER_LIBS	= /usr/X11R6/lib/libXm.a -L/usr/X11R6/lib -lXt -lXmu -lXext -lX11 -lSM -lICE -lm $(OS_LIBS)
endif

########################################
# NetBSD
ifeq ($(OS_ARCH),NetBSD)

# Work around linker semantics that will never find our own libutil on 1.3.x
FOOL_GMAKE	= $(EXPORT_LDFLAGS:$(DIST)/lib/lib%.a=-l%)
DSO_EX_LIBS	= $(FOOL_GMAKE:-lutil=$(DIST)/lib/libutil.a)

ifdef LIBRUNPATH
OS_LIBS		+= -R$(LIBRUNPATH)
endif

OTHER_LIBS	= -L/usr/X11R6/lib -lXm -lXt -lXmu -lXext -lX11 -lSM -lICE -lm $(OS_LIBS)

endif

########################################
# OpenBSD
# Copied from NetBSD...
ifeq ($(OS_ARCH),OpenBSD)
ifdef LIBRUNPATH
OS_LIBS		+= -R$(LIBRUNPATH)
endif
OTHER_LIBS	= -L/usr/X11R6/lib -lXm -lXt -lXmu -lXext -lX11 -lSM -lICE -lm $(OS_LIBS)
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

ifeq ($(OS_RELEASE),4.0)
X11R6LIBS	= $(LOC_LIB_DIR)/libSM.a $(LOC_LIB_DIR)/libICE.a
endif

NEED_NLS	= 1

endif

########################################
# HP Machines
ifeq ($(OS_ARCH),HP-UX)

ifeq ($(OS_RELEASE),A.09)
HPUX_XLIBS	= -L/usr/lib/X11R5 -L/usr/contrib/X11R5/lib -L/usr/lib/Motif1.2
endif
ifeq ($(OS_RELEASE),B.10)
INCLUDES	+= -I.
HPUX_XLIBS	= -L/usr/lib/X11R6 -L/usr/contrib/X11R6/lib -L/usr/lib/Motif1.2_R6 -L/usr/lib/Motif1.2
endif
ifeq ($(OS_RELEASE),B.11)
HPUX_XLIBS	= -L/usr/lib/X11R6 -L/usr/contrib/X11R6/lib -L/usr/lib/Motif2.1
endif

HPUX_RESOURCE_HACK	= -Wp,-H16384

OTHER_LIBS	= $(HPUX_XLIBS) -lXm -L$(DIST)/lib -lXmu -lXt -lX11 -lXext $(OS_LIBS)

ifdef MOZ_EDITOR

ifdef NS_USE_GCC
CCLD		= $(CCC)
else
EXPORT_DEPLIBS	+= cxxlink-filter
US_DEPLIBS	+= cxxlink-filter
CCLD		= CC -tl,./cxxlink-filter
endif

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
EXTRA_REL_FILES	+= $(WEBFONT_DSO)
endif

endif

########################################
# Linux
ifeq ($(OS_ARCH),Linux)

# Override the definition of OS_LIBS in config/Linux.mk.
ifeq ($(USE_PTHREADS),1)
OS_LIBS			= -lpthread -lm -ldl -lc
else
OS_LIBS			= -lm -ldl
endif

XFE_MOTIF_LDFLAGS	= \
			$(MOZILLA_XFE_GLIBC_BROKEN_LOCALE_FLAGS) \
			$(MOZILLA_XFE_MOTIF_FLAGS) \
			$(MOZILLA_XFE_MOTIF_PRINT_SHELL_FLAGS)

XFE_X11_LDFLAGS		= \
			$(MOZILLA_XFE_X11_DYNAMIC_PATHS) \
			-lXt \
			-lXmu \
			$(MOZILLA_XFE_MOTIF_XPM_FLAGS) \
			$(MOZILLA_XFE_X11_DYNAMIC_FLAGS) \
			-lXext \
			$(MOZILLA_XFE_X11_SM_FLAGS)

OTHER_LIBS		= $(XFE_MOTIF_LDFLAGS) $(XFE_X11_LDFLAGS) $(OS_LIBS)

# Dynamic motif binary
ifdef MOZILLA_XFE_MOTIF_HAVE_DYNAMIC_LIB

XFE_DYN_MOTIF_LDFLAGS	= \
			$(MOTIFPATCH_LIB) \
			$(MOZILLA_XFE_GLIBC_BROKEN_LOCALE_FLAGS) \
			$(MOZILLA_XFE_MOTIF_DYNAMIC_PATHS) \
			$(MOZILLA_XFE_MOTIF_DYNAMIC_FLAGS) \
			$(MOZILLA_XFE_MOTIF_PRINT_SHELL_FLAGS)

EXTRA_EXPORT_OBJS	= $(OBJDIR)/$(XFE_PROGNAME)-motif-export
TARGETS			+= $(EXTRA_EXPORT_OBJS)

PLUGIN_DSO		+= $(DIST)/bin/libnullplugin-dynMotif.so

OTHER_DYN_MOTIF_LIBS	= $(XFE_DYN_MOTIF_LDFLAGS) $(XFE_X11_LDFLAGS) $(OS_LIBS)

endif

# Web fonts are only available on x86
ifeq ($(OS_RELEASE)$(CPU_ARCH),2.0x86)

ifndef NO_WEBFONTS
EXTRA_REL_FILES	+= $(WEBFONT_DSO)
endif

endif

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

endif

########################################
# Apple Rhapsody
ifeq ($(OS_ARCH),Rhapsody)
OTHER_LIBS	= -L/usr/X11R6/lib -lXm -lXmu -lXt -lXext -lX11 -lSM -lICE $(OS_LIBS)
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
EXTRA_REL_FILES	+= $(WEBFONT_DSO)
endif

ifdef MOZ_EDITOR
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
EXTRA_REL_FILES	+= $(WEBFONT_DSO)
endif
else
OTHER_LIBS	= $(MOTIFLIB) -lXt -lXmu -lXext -lX11 $(OS_LIBS) -lm
LDFLAGS		= $(NOMD_CFLAGS)
endif

EXPORT_LDFLAGS	= -z defs -L$(MOTIF)/lib -L$(USRLIBDIR) -R$(MOTIF)/lib \
		  -R$(USRLIBDIR) $(BASIC_LIBS) $(EXPORT_LIB) $(BASIC_LIBS_2) \
		  $(BASIC_DSOS) $(NSPR_LIB) $(OTHER_LIBS)

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

########################################
# Data General DG/UX
ifeq ($(OS_ARCH),DGUX)

OTHER_LIBS	= $(MOTIFLIB) $(OS_LIBS)

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

$(XFE_WIDGETS_COMBO_BOX_LIB):
	cd XfeWidgets/XfeComboBox; $(MAKE)

$(XFE_WIDGETS_CAPTION_LIB):
	cd XfeWidgets/XfeCaption; $(MAKE)

$(XFE_WIDGETS_TOOL_BAR_LIB):
	cd XfeWidgets/XfeToolBar; $(MAKE)

$(XFE_WIDGETS_TOOL_TIP_LIB):
	cd XfeWidgets/XfeToolTip; $(MAKE)

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

$(OBJDIR)/Netscape-nis-export.ad $(OBJDIR)/Netscape-export.ad: Makefile resources resources-translations versionn.h strs make-resources $(LOCALE_MAP)
	@./make-resources $@ Netscape Netscape versionn.h strs resources resources-translations $(MOZ_USER_DIR) "" export $(LOCALES)

#######################################################################
# The "-export" targets

$(OBJDIR)/$(DSO_PROGNAME)-export: $(OBJS) $(EXPORT_OBJS) $(EXPORT_DEPLIBS)
	@$(MAKE_OBJDIR)
	rm -f $@
ifdef NETSCAPE_OFFICIAL
	rm -rf $(OBJDIR)/objs.txt
	@echo Creating a list of all object files ... objs.txt
	@echo $(OBJS) > $(OBJDIR)/objs.txt
	@echo $(EXPORT_OBJS) >> $(OBJDIR)/objs.txt
	@echo -L$(DIST)/bin >> $(OBJDIR)/objs.txt
	@echo -L$(DIST)/lib >> $(OBJDIR)/objs.txt
	@echo $(DSO_EX_LIBS) >> $(OBJDIR)/objs.txt
	@echo Creating a list of all symbols ... symbols.txt
	perl5 listsymb.pl $(OBJDIR)/objs.txt $(OBJDIR)/symbols.txt
	@echo Generating dispatch table ... dispunix.h dispunix.c cmoffset.h
	perl5 gendisp.pl $(OBJDIR)/symbols.txt $(OBJDIR)/dispunix.h $(OBJDIR)/dispunix.c
	$(CC) -c -o $(OBJDIR)/dispunix.o -I$(DIST)/include -I$(OBJDIR)  $(OBJDIR)/dispunix.c
	$(CCLD) -o $@ $(LDFLAGS) $(OBJDIR)/dispunix.o $(OBJS) $(EXPORT_OBJS) -L$(DIST)/bin -L$(DIST)/lib $(DSO_EX_LIBS) $(DNS_LIB)
else
	$(CCLD) -o $@ $(LDFLAGS) $(OBJS) $(EXPORT_OBJS) -L$(DIST)/bin -L$(DIST)/lib $(DSO_EX_LIBS) $(DNS_LIB)
endif
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
	$(CCLD) -o $@ $(DYNAMIC_SYMBOLS_F) $(LDFLAGS) $(OBJS) $(EXPORT_OBJS) $(EXPORT_LDFLAGS) $(DNS_LIB)
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
ifeq ($(OS_ARCH),Linux)
ifdef MOZILLA_XFE_MOTIF_HAVE_DYNAMIC_LIB
$(OBJDIR)/$(XFE_PROGNAME)-motif-export: $(OBJS) $(EXPORT_OBJS) $(EXPORT_DEPLIBS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(CCLD) -o $@ $(DYNAMIC_SYMBOLS_F) $(LDFLAGS) $(OBJS) $(EXPORT_OBJS) $(EXPORT_DYN_MOTIF_LDFLAGS) $(DNS_LIB)
	@$(EXTRA_POST_LINK_CMD) $@
endif
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

#
# In first. Need to double check
#
# This fixes a 4.5-specific problem caused by including /usr/include/strings.h unnecessarily.
$(OBJDIR)/mozilla.o: mozilla.c
	@$(MAKE_OBJDIR)
	$(CC) -o $@ -c $(CFLAGS) -D_H_STRINGS $<
#
# end 
#
AIX_HACK	:= $(notdir $(AIX_NSPR))

# Install nspr dynamic library for AIX.
$(AIX_HACK): $(AIX_NSPR)
	$(INSTALL) -m 644 $< .
endif

install:: $(AIX_HACK) $(EXTRA_REL_FILES) $(TARGETS)
ifdef TARGETS
	$(INSTALL) $(filter-out tweak_nspr,$(TARGETS)) $(DIST)/bin
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
	@echo "FC_PLATFORM = $(FC_PLATFORM)"
