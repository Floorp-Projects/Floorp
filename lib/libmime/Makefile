#!gmake
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

DEPTH		= ../..

MODULE		= mime
LIBRARY_NAME	= mime

CSRCS		= mimecont.c \
		  mimedrft.c \
		  mimeebod.c \
		  mimeenc.c \
		  mimeeobj.c \
		  mimehdrs.c \
		  mimei.c \
		  mimeiimg.c \
		  mimeleaf.c \
		  mimemalt.c \
		  mimemapl.c \
		  mimemdig.c \
		  mimemmix.c \
		  mimemoz.c \
		  mimempar.c \
		  mimemrel.c \
		  mimemsg.c \
		  mimemsig.c \
		  mimemult.c \
		  mimeobj.c \
		  mimepbuf.c \
		  mimesun.c \
		  mimetenr.c \
		  mimetext.c \
		  mimethtm.c \
		  mimetpla.c \
		  mimetric.c \
		  mimeunty.c \
		  mimevcrd.c \
		  mimecryp.c \
		  $(NULL)

ifndef NO_SECURITY
CSRCS		+= mimempkc.c \
		   mimepkcs.c \
		  $(NULL)
endif

REQUIRES	= nspr dbm img util layer security pref js

include $(DEPTH)/config/rules.mk

FILT_SRCS	= mimefilt.c mimestub.c
FILT_OBJS	= $(addprefix $(OBJDIR)/,$(FILT_SRCS:.c=.o))

ifndef NO_SECURITY
INCLUDES	+= $(DEPTH)/../ns_include/libmime
endif

#
# Building the "mimefilt" executable, which reads a message from stdin and
# writes HTML to stdout
#
LIBMSG_DIR	= ../libmsg
LIBXP_DIR	= ../xp
LIBXP_BIN_DIR	= $(LIBXP_DIR)/$(OBJDIR)
LIBMSG_BIN_DIR	= $(LIBMSG_DIR)/$(OBJDIR)

EXT_OBJECTS	= $(LIBMSG_BIN_DIR)/addr.o \
		  $(LIBXP_BIN_DIR)/xp_file.o \
		  $(LIBXP_BIN_DIR)/xp_str.o \
		  $(LIBXP_BIN_DIR)/xp_intl.o \
		  $(LIBXP_BIN_DIR)/xp_error.o \
		  $(LIBXP_BIN_DIR)/xp_trace.o \
		  $(LIBXP_BIN_DIR)/xp_reg.o \
		  $(LIBXP_BIN_DIR)/xp_hash.o \
		  $(LIBXP_BIN_DIR)/xp_list.o \
		  $(NULL)

FILT_STUBS	= $(EXT_OBJECTS) \
		  $(LIBXP_BIN_DIR)/allxpstr.o \
		  $(DIST)/lib/libpkcs7.$(LIB_SUFFIX) \
		  $(DIST)/lib/libsecmod.$(LIB_SUFFIX) \
		  $(DIST)/lib/libcert.$(LIB_SUFFIX) \
		  $(DIST)/lib/libkey.$(LIB_SUFFIX) \
		  $(DIST)/lib/libsecutil.$(LIB_SUFFIX) \
		  $(DIST)/lib/libhash.$(LIB_SUFFIX) \
		  $(DIST)/lib/libssl.$(LIB_SUFFIX) \
		  $(DIST)/lib/libcrypto.$(LIB_SUFFIX) \
		  $(DIST)/lib/libdbm.$(LIB_SUFFIX) \
		  $(DIST)/lib/libaddr.$(LIB_SUFFIX) \
		  $(DIST)/lib/libnspr.$(LIB_SUFFIX) \
		  $(NULL)

$(EXT_OBJECTS):
	@cd $(shell dirname $@)/../ ; \
	$(MAKE_OBJDIR) ; \
	$(MAKE) $(subst $(LIBXP_BIN_DIR)/,,$(subst $(LIBMSG_BIN_DIR)/,,$@))

$(OBJDIR)/mimefilt: $(FILT_OBJS) $(LIBRARY) $(FILT_STUBS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(CC) -o $@ $(CFLAGS) $(FILT_OBJS) $(LIBRARY) $(FILT_STUBS) $(LDFLAGS)

$(OBJDIR)/mimefilt.pure: $(LIBRARY) $(FILT_OBJS) $(FILT_STUBS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(PURIFY) $(CC) -o $@ $(NOMD_CFLAGS) $(FILT_OBJS) $(LIBRARY) $(FILT_STUBS) $(LDFLAGS)

TAGS:: $(LIB_SRCS) $(FILT_SRCS) $(HEADERS)
	$(ETAGS) $(LIB_SRCS) $(FILT_SRCS) $(HEADERS)
