#
# GNU Makefile for libssldap
#

LDAP_SRC = ../..
MCOM_ROOT = ../../../..

NOSTDCLEAN=true # don't let nsconfig.mk define target clean
NOSTDSTRIP=true # don't let nsconfig.mk define target strip
NSPR20=true	# probably should be defined somewhere else (not sure where)

OBJDEST = $(OBJDIR)/lib/libssldap
LIBDIR = $(LDAP_LIBDIR)

include $(MCOM_ROOT)/c-sdk/nsdefs.mk
include $(MCOM_ROOT)/c-sdk/nsconfig.mk
include $(LDAP_SRC)/nsldap.mk

CFLAGS +=	$(SLCFLAGS)

LIBSSLDAP_OBJS=	clientinit.o ldapsinit.o errormap.o

OBJS = $(addprefix $(OBJDEST)/, $(LIBSSLDAP_OBJS)) 

LIBSSLDAP=	$(addprefix $(LIBDIR)/, $(LIBSSLDAP_LIB).$(LIB_SUFFIX))

clientSDK: all

all:	$(OBJDEST) $(LIBDIR) $(LIBSSLDAP)

$(LIBDIR):
	$(MKDIR) $(LIBDIR)

$(LIBSSLDAP): $(OBJS)
	$(LINK_LIB)

veryclean: clean

clean:
	$(RM) $(OBJS)
	$(RM) $(LIBSSLDAP)

$(OBJDEST):
	$(MKDIR) $(OBJDEST)

