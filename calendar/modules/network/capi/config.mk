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

CFLAGS         +=-D_IMPL_NS_CALENDAR

LD_LIBS += \
	raptorbase \
	xpcom$(MOZ_BITS) \
	$(NATIVE_LIBNLS_LIBS) \
	util10	\
	$(XP_REG_LIB)

AR_LIBS +=  \
              capicore \
              capiinterface \
              $(NULL)

EXTRA_LIBS += $(NSPR_LIBS)

