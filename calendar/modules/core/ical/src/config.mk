#
#           CONFIDENTIAL AND PROPRIETARY SOURCE CODE OF
#              NETSCAPE COMMUNICATIONS CORPORATION
# Copyright © 1996, 1997 Netscape Communications Corporation.  All Rights
# Reserved.  Use of this Source Code is subject to the terms of the
# applicable license agreement from Netscape Communications Corporation.
# The copyright notice(s) in this Source Code does not indicate actual or
# intended publication of this Source Code.
#

#
#  Override TARGETS variable so that only static libraries
#  are specifed as dependencies within rules.mk.
#

CFLAGS         +=-D_IMPL_NS_CAL_CORE_ICAL -DNSPR20
INCLUDES       += -I../inc -I$(GDEPTH)/include -I$(GDEPTH)/htmlparser/src

ifeq ($(OS_ARCH), WINNT)
CFLAGS += /Zm1000
endif

LD_LIBS += \
	raptorbase \
	raptorhtmlpars \
	xpcom$(MOZ_BITS) \
	$(NATIVE_RAPTOR_WIDGET) \
	calcapi10 \
	util10  \
	xpcom$(MOZ_BITS) \
    $(NATIVE_LIBNLS_LIBS) \
	$(XP_REG_LIB)


EXTRA_LIBS += $(NSPR_LIBS)
