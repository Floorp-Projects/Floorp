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

CFLAGS         +=-D_IMPL_NS_CALENDAR -DNSPR20 -I$(GDEPTH)/include

PROGRAM        = trex

OS_LIBS += $(GUI_LIBS) 

LD_LIBS += \
	raptorbase \
	raptorhtmlpars \
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
	jsj$(MOZ_BITS)$(VERSION_NUMBER) \
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
	$(NATIVE_JULIAN_DLL) \
	nsfmt$(MOZ_BITS)30 \
	nsuni$(MOZ_BITS)30 \
	nscck$(MOZ_BITS)30 \
	nsjpn$(MOZ_BITS)30 \
	nscnv$(MOZ_BITS)30 \
	nssb$(MOZ_BITS)30 \
	calui10 \
	calnetwork10 \
	calparser10 \
	util10 \
	$(NATIVE_RAPTOR_GFX) \
	$(RAPTOR_GFX) \
	$(NATIVE_JULIAN_DLL) \
	xpcom$(MOZ_BITS) \
	$(NATIVE_ZLIB_DLL) \
	$(NATIVE_XP_DLL) \
	$(XP_REG_LIB)

STATIC_LIBS += shell

EXTRA_LIBS += \
	$(NSPR_LIBS)
