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

CFLAGS         +=-D_IMPL_NS_TREXTEST -DNSPR20 -I$(GDEPTH)/include 

PROGRAM        = zulutest

OS_LIBS += $(GUI_LIBS) 

LD_LIBS += \
	raptorbase \
	raptorhtmlpars \
	raptorhtml \
	png \
	abouturl \
	fileurl \
	ftpurl \
	gophurl \
	httpurl \
	$(XP_PREF_DLL) \
	img$(MOZ_BITS)$(VERSION_NUMBER) \
	jpeg$(MOZ_BITS)$(VERSION_NUMBER) \
	js$(MOZ_BITS)$(VERSION_NUMBER) \
	jsdom \
	mimetype \
	netlib \
	remoturl \
	netcache \
	netcnvts \
	netutil \
	network \
	netlib \
	xpfc10 \
	$(NATIVE_RAPTOR_WIDGET) \
	calui10 \
	calparser10 \
	calcapi10 \
	$(NATIVE_RAPTOR_GFX) \
	$(RAPTOR_GFX) \
	$(NATIVE_ZLIB_DLL) \
	$(NATIVE_XP_DLL) \
	$(NATIVE_LIBNLS_LIBS) \
	xpcom$(MOZ_BITS) \
	util10 \
	calcore10 \
	cal_core_ical10 \
	$(XP_REG_LIB)

ifeq ($(OS_ARCH),Linux)
LD_LIBS += \
	secfree stubnj stubsj util xp pwcac dbm
endif


STATIC_LIBS += shell

EXTRA_LIBS += \
	$(NSPR_LIBS)
