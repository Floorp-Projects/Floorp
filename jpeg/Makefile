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

DEPTH		= ..

MODULE		= jpeg
LIBRARY_NAME	= jpeg

REQUIRES	= java nspr

EXPORTS		= jconfig.h jerror.h jinclude.h jmorecfg.h jpeglib.h jpegint.h jwinfig.h jos2fig.h

CSRCS		= jdapimin.c \
		  jdapistd.c \
		  jdatasrc.c \
		  jdatadst.c \
		  jdmaster.c \
		  jdinput.c  \
		  jdmarker.c \
		  jdhuff.c \
		  jdphuff.c \
		  jdmainct.c \
		  jdcoefct.c \
		  jdpostct.c \
		  jddctmgr.c \
		  jidctfst.c \
		  jidctflt.c \
		  jidctint.c \
		  jidctred.c \
		  jdsample.c \
		  jdcolor.c \
		  jquant1.c \
		  jquant2.c \
		  jdmerge.c \
		  jcomapi.c \
		  jutils.c \
		  jerror.c \
		  jmemmgr.c \
		  jmemnobs.c \
		  jfdctflt.c \
		  jfdctfst.c \
		  jfdctint.c \
		  $(NULL)

ifeq ($(subst /,_,$(shell uname -s)),OS2)
CSRCS		+= jcapimin.c \
		   jcparam.c \
		   jcapistd.c \
		   jcmarker.c \
		   jcinit.c \
		   jcmainct.c \
		   jchuff.c \
		   jcsample.c \
		   jcmaster.c \
		   jccoefct.c \
		   jccolor.c \
		   jcphuff.c \
		   jcdctmgr.c \
		   jcprepct.c \
		   $(NULL)

OS2_IMPLIB	= 1
LIBRARY		= $(LIBRARY_NAME)$(MOZ_BITS)$(VERSION_NUMBER).$(LIB_SUFFIX)
DEF_FILE	= $(LIBRARY_NAME).def
TARGETS		= $(LIBRARY)
endif

include $(DEPTH)/config/rules.mk

#
# Trying to compile jidctred.c with -O2 causes cc1 (?) to dump core,
# leaving gcc hanging.  Using -O works fine.
#
# Linux 2.0.30 (RedHat 5 Alpha)
# 64MB RAM
# egcs-2.90.23 980102 (egcs-1.0.1 release)
#
ifeq ($(OS_ARCH)$(CPU_ARCH),Linuxalpha)
ifdef BUILD_OPT
$(OBJDIR)/jidctred.o: jidctred.c
	@$(MAKE_OBJDIR)
	$(CC) -o $@ -c $(XP_DEFINE) -O $(OS_CFLAGS) $(MDUPDATE_FLAGS) $(DEFINES) $(INCLUDES) $(XCFLAGS) $(PROF_FLAGS) $<
endif
endif
