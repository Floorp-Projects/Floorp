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

NECKO=1
DEPTH=.

DIRS = \
!if defined (exporting)
		config \
		nsprpub \
!if defined (MOZ_STATIC_COMPONENTS)
		 modules\staticmod
!endif
		dbm \
		modules\libreg \
		xpcom \
		js \
		js\src\xpconnect \
		js\src\liveconnect \
		modules\zlib \
		widget\timer \
		include \
		modules\libutil \
		netwerk \
		modules\appfilelocprovider \
!if defined(BUILD_PSM)
                security \
!endif
		uriloader \
                uriloader\exthandler \
		intl \
		modules\libpref \
		jpeg \
		modules\libimg \
		gfx \
		widget \
		modules\oji \
		modules\plugin \
		modules\libjar \
		modules\libimg\png \
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
!if defined(BUILD_PSM)
                extensions\psm-glue \
!endif
		mailnews \
		xpinstall \
		$(NULL)
!else
		config \
		nsprpub \
!if defined (MOZ_STATIC_COMPONENTS)
		modules\staticmod
!endif		
		dbm \
		modules\libreg \
		xpcom \
		modules\libutil \
		jpeg \
		modules\libimg \
		widget\timer \
		gfx \
		widget \
		js \
		js\src\xpconnect \
		js\src\liveconnect \
		modules\zlib \
		modules\zlib\standalone \
		netwerk \
!if defined(BUILD_PSM)
                security \
!endif
		uriloader \
		intl \
		modules\libpref \
		modules\oji \
		modules\libjar \
		caps \
		expat \
		htmlparser \
		dom \
		view \
		layout \
		rdf \
		docshell \
		modules\appfilelocprovider \
		webshell \
		embedding \
		editor \
		xpfe\appshell \
		xpfe\components\shistory \
		extensions\cookie \
!if defined(BUILD_PSM)
                extensions\psm-glue \
!endif
		$(NULL)
!endif


include <$(DEPTH)\config\rules.mak>

all:: build_small

build_small:
	$(MAKE) -f embed.mak export exporting=1
	$(MAKE) -f embed.mak install
