#
# ***** BEGIN LICENSE BLOCK *****
# Version: NPL 1.1/GPL 2.0/LGPL 2.1
# 
# The contents of this file are subject to the Netscape Public License
# Version 1.1 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/NPL/
# 
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
# 
# The Original Code is Mozilla Communicator client code.
# 
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998-1999
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the NPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the NPL, the GPL or the LGPL.
# 
# ***** END LICENSE BLOCK ***** 

#
# GNU Makefile for libutil
#

LDAP_SRC = ../..
MCOM_ROOT = ../../../..

NOSTDCLEAN=true # don't let nsconfig.mk define target clean
NOSTDSTRIP=true # don't let nsconfig.mk define target strip
NSPR20=true	# probably should be defined somewhere else (not sure where)

OBJDEST = $(OBJDIR)/lib/libutil
LIBDIR = $(LDAP_LIBDIR)

include $(MCOM_ROOT)/c-sdk/nsdefs.mk
include $(MCOM_ROOT)/c-sdk/nsconfig.mk
include $(LDAP_SRC)/nsldap.mk

#
# crypt.c and crypt_util.c now come in the liblcache dll
# ntdebug.c currently not used
#
#if ServerBuild  XXXmcs? how to switch on this?
#LIBUTIL_OBJS=	getopt.o ntevent.o ntgetpassword.o \
# 	ntreg.o ntstubs.o
#ifeq ($(ARCH), WINNT)
#LIBUTIL_OBJS += crypt.o crypt_util.o
#endif
#
#else
# SDK builds only need getopt!
LIBUTIL_OBJS=	getopt.o
#endif

OBJS = $(addprefix $(OBJDEST)/, $(LIBUTIL_OBJS)) 

LIBUTIL=	$(addprefix $(LIBDIR)/, libutil.$(LIB_SUFFIX))

INCLUDES += -I$(LDAP_SRC)/servers/slapd -I$(OBJDIR)/include

SLAPDMESSAGES_H=$(MCOM_ROOT)/c-sdk/ldap/include/ntslapdmessages.h

ifeq ($(LDAP_NO_LIBLCACHE),1)
CFLAGS+=-DNO_LIBLCACHE
endif

clientSDK: $(OBJDEST) $(LIBDIR) $(OBJS) $(LIBUTIL)

all:	$(OBJDEST) $(SLAPDMESSAGES_H) $(LIBDIR) $(OBJS) $(LIBUTIL)

$(LIBDIR):
		$(MKDIR) $(LIBDIR)

$(LIBUTIL): $(OBJS)
	$(LINK_LIB)

$(SLAPDMESSAGES_H):
	@echo target: $@
	cd $(MCOM_ROOT)/c-sdk/ldap/servers/slapd/ntmsgdll; $(MAKE) $(MFLAGS)

veryclean: clean

cleanSDK: clean

clean:
	$(RM) $(OBJS)
	$(RM) $(LIBUTIL)

$(OBJDEST):
	$(MKDIR) $(OBJDEST)

