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

CFLAGS         +=-D_IMPL_NS_CAL_UTIL  -DNSPR20
INCLUDES       +=-I../inc -I$(GDEPTH)/include

ifeq ($(OS_ARCH), WINNT)
CFLAGS += /Zm1000
endif

LIBRARY_NAME      = util
LIBRARY_VERSION   = 10

LD_LIBS += \
	$(NATIVE_LIBNLS_LIBS) \
	raptorbase \
	xpcom$(MOZ_BITS) \
	$(XP_REG_LIB)

EXTRA_LIBS += $(NSPR_LIBS)
