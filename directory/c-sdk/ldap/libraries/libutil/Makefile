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

