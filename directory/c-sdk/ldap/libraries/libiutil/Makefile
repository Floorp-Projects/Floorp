
#
# GNU Makefile for libiutil
#

LDAP_SRC = ../..
MCOM_ROOT = ../../../..

NOSTDCLEAN=true # don't let nsconfig.mk define target clean
NOSTDSTRIP=true # don't let nsconfig.mk define target strip
NSPR20=true	# probably should be defined somewhere else (not sure where)

OBJDEST = $(OBJDIR)/lib/libiutil
LIBDIR = $(LDAP_LIBDIR)

include $(MCOM_ROOT)/netsite/nsdefs.mk
include $(MCOM_ROOT)/netsite/nsconfig.mk
include $(LDAP_SRC)/nsldap.mk

CFLAGS +=	$(SLCFLAGS)

LIBIUTIL_OBJS=	iutil-lock.o

OBJS = $(addprefix $(OBJDEST)/, $(LIBIUTIL_OBJS)) 

LIBIUTIL=	$(addprefix $(LIBDIR)/, libiutil.$(LIB_SUFFIX))

clientSDK: all

all:	$(OBJDEST) $(LIBDIR) $(LIBIUTIL)

$(LIBDIR):
	$(MKDIR) $(LIBDIR)

$(LIBIUTIL): $(OBJS)
	$(LINK_LIB) 

veryclean: clean

clean:
	$(RM) $(OBJS)
	$(RM) $(LIBIUTIL)

$(OBJDEST):
	$(MKDIR) $(OBJDEST)

