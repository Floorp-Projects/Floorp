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
	$(NATIVE_RAPTOR_GFX) \
	$(NATIVE_RAPTOR_WIDGET) \
	$(NATIVE_JULIAN_DLL) \
	xpcom$(MOZ_BITS) \
    $(NATIVE_LIBNLS_LIBS) \
    xpfc10 \
    calcore10 \
	$(XP_REG_LIB)

AR_LIBS += \
              canvas \
              core \
              context \
			  component \
              controller \
			  command \
              toolkit \
              $(NULL)

OS_LIBS += $(GUI_LIBS) $(MATH_LIB)

EXTRA_LIBS += $(NSPR_LIBS)

