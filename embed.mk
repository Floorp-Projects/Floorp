#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 
#
#  

DEPTH		= .
topsrcdir	= .
srcdir		= .
VPATH		= .

include $(DEPTH)/config/autoconf.mk

DIRS		= config build

ifdef USE_ELF_DYNSTR_GC
DIRS		+= tools/elf-dynstr-gc
endif

DIRS		+= nsprpub

ifeq ($(exporting),1)
	DIRS += \
		dbm \
		modules/libreg \
		js \
		xpcom \
		js/src/xpconnect \
		js/src/liveconnect \
		modules/zlib \
		widget/timer \
		include \
		modules/libutil \
		netwerk \
		modules/appfilelocprovider \
		security \
		uriloader \
                uriloader/exthandler \
		intl \
		modules/libpref \
		modules/libimg \
		gfx \
		widget \
		modules/oji \
		modules/plugin \
		modules/libjar \
		modules/libimg/png \
		caps \
		expat \
		htmlparser \
		dom \
		view \
		layout \
		db \
		rdf \
		docshell \
		webshell \
		embedding \
		editor \
		sun-java \
		profile \
		xpfe \
		extensions \
		mailnews \
		xpinstall \
		$(NULL)
else
	DIRS += \
		dbm \
		modules/libreg \
		js \
		xpcom \
		js/src/xpconnect \
		js/src/liveconnect \
		modules/libutil \
		modules/zlib \
		modules/zlib/standalone \
		netwerk \
		security \
		uriloader \
		intl \
		modules/libpref \
		modules/libimg \
		gfx \
		modules/oji \
		modules/libjar \
		caps \
		expat \
		htmlparser \
		dom \
		view \
		layout \
		modules/appfilelocprovider \
		rdf \
		docshell \
		webshell \
		widget \
		embedding \
		editor \
		xpfe/appshell \
		xpfe/components/shistory \
		extensions/cookie \
		extensions/psm-glue \
		$(NULL)
endif

ifdef MOZ_STATIC_COMPONENTS
DIRS   += modules/staticmod
endif

GARBAGE += dist
DIST_GARBAGE = config.cache config.log config.status config-defs.h \
   dependencies.beos config/autoconf.mk \
   $(shell cd $(topsrcdir); . ./allmakefiles.sh; echo $${MAKEFILES})

include $(topsrcdir)/config/rules.mk

# rdf chrome are not included, but could be.  I did not include them because you may disable INCLUDE_XUL
# lwbrk unicharutil are not include because they are busted right now.
CONFIG_RELEASE_SMALL_OPTIONS :=  \
	--enable-gfx-toolkit=gtk \
	--enable-optimize        \
	--enable-strip-libs      \
	--enable-elf-dynstr-gc   \
	--disable-mailnews       \
	--disable-debug          \
	--disable-tests          \
	--enable-static-components=necko,necko2,cookie,psmglue,caps,docshell,editor,txmgr,txtsvc,jar50,jsurl,nspng,nsgif,pref,shistory,strres,uriloader \
	$(NULL)

config_release_small:
	$(DEPTH)/configure $(CONFIG_RELEASE_SMALL_OPTIONS) 

build_small:
	$(MAKE) -f embed.mk config_release_small
	$(MAKE) -f embed.mk export exporting=1
	$(MAKE) -f embed.mk

### does not seam to work....
# I was hoping that we could just pull SeaMonkeyCore, but I think I need editor and l10n :-(

NSPR_CO_MODULE = mozilla/nsprpub
NSPR_CO_TAG = NSPRPUB_CLIENT_BRANCH

PSM_CO_MODULE= mozilla/security
PSM_CO_TAG = SECURITY_CLIENT_BRANCH

pull:
	cd ..; \
	cvs co -P SeaMonkeyAll&& \
	cvs co -P -r $(PSM_CO_TAG) $(PSM_CO_MODULE)  && \
	cvs co -P -r $(NSPR_CO_TAG) $(NSPR_CO_MODULE)




