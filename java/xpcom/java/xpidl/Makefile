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

DEPTH		= ../../../..
topsrcdir	= ../../../..
srcdir		= .
VPATH		= .
XPIDLDIR = $(DEPTH)/xpcom/typelib/xpidl

include $(DEPTH)/config/autoconf.mk

PROGRAM		= xpidl$(BIN_SUFFIX)
INTERNAL_TOOLS	= 1

CSRCS		= \
		$(XPIDLDIR)/xpidl.c \
		xpidl_idl.c \
		$(XPIDLDIR)/xpidl_util.c \
		$(XPIDLDIR)/xpidl_header.c \
		$(XPIDLDIR)/xpidl_typelib.c \
		$(XPIDLDIR)/xpidl_doc.c \
		xpidl_java.c \
		$(NULL)


CFLAGS		+= $(LIBIDL_CFLAGS) -I$(XPIDLDIR)

ifdef CROSS_COMPILE
HOST_PROGRAM	= host_xpidl
HOST_CSRCS	= $(CSRCS)
endif

include $(topsrcdir)/config/rules.mk



# Compile directly against the static lib, so we can use xpidl during the build
# without the shared library path being set.
LIBS		= $(DIST)/lib/libxpt.$(LIB_SUFFIX) $(LIBIDL_LIBS)

# Tell the $(PROGRAM) target that we need to be recompiled when libxpt changes.
EXTRA_DEPS	= $(wildcard $(DIST)/lib/libxpt.*)

ifdef CROSS_COMPILE
HOST_CFLAGS	+= $(HOST_LIBIDL_CFLAGS)
HOST_LIBS	= $(DIST)/host/lib/libhostxpt.$(LIB_SUFFIX) $(HOST_LIBIDL_LIBS)
HOST_EXTRA_DEPS	= $(wildcard $(DIST)/host/lib/libhostxpt.*)

ifdef HOST_NSPR_MDCPUCFG
HOST_CFLAGS	+= -DMDCPUCFG=$(HOST_NSPR_MDCPUCFG)
endif
endif

export:: install

