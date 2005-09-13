#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

DEPTH		= .
topsrcdir	= .
srcdir		= .
VPATH		= .

include $(DEPTH)/config/autoconf.mk

include $(topsrcdir)/build/unix/modules.mk

EMBEDDING_EXPORTS = \
	dbm \
	modules/libreg \
	js \
	xpcom \
	js/src/xpconnect \
	js/src/liveconnect \
	modules/zlib \
	include \
	modules/libutil \
	netwerk \
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
	content \
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

EMBEDDING_INSTALLS = \
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
	content \
	layout \
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
	embedding/config \
	$(NULL)

DIRS             = $(NULL)
DIRS		+= config build

ifdef MOZ_L10N
DIRS		+= l10n
endif

ifdef USE_ELF_DYNSTR_GC
DIRS		+= tools/elf-dynstr-gc
endif

DIRS		+= $(NSPRPUB_DIR)

ifeq ($(exporting),1)
DIRS += \
	$(EMBEDDING_EXPORTS) \
	$(NULL)
else
DIRS += \
	$(EMBEDDING_INSTALLS) \
	$(NULL)
endif

ifdef MOZ_STATIC_COMPONENTS
DIRS   += modules/staticmod
endif

STATIC_MAKEFILES := $(NSPRPUB_DIR)

GARBAGE += dist
DIST_GARBAGE = config.cache config.log config.status config-defs.h \
   dependencies.beos config/autoconf.mk \
   $(shell cd $(topsrcdir); . ./allmakefiles.sh; echo $${MAKEFILES})

include $(topsrcdir)/config/rules.mk

# rdf chrome are not included, but could be.  I did not include them because you may disable INCLUDE_XUL
# lwbrk unicharutil are not include because they are busted right now.
CONFIG_RELEASE_EMBED_OPTIONS :=  \
	--enable-gfx-toolkit=gtk \
	--enable-optimize        \
	--enable-strip-libs      \
	--enable-elf-dynstr-gc   \
	--disable-mailnews       \
	--disable-debug          \
	--disable-tests          \
	--enable-static-components=necko,necko2,cookie,psmglue,caps,docshell,editor,txmgr,txtsvc,jar50,jsurl,nspng,nsgif,pref,shistory,strres,uriloader \
	$(NULL)

config_release_embed:
	$(DEPTH)/configure $(CONFIG_RELEASE_EMBED_OPTIONS) 

build_embed:
	$(MAKE) -f embed.mk export exporting=1
	$(MAKE) -f embed.mk

### does not seem to work....
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




