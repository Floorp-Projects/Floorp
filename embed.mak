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
		security \
		uriloader \
                uriloader\exthandler \
		intl \
		modules\libpref \
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
		js \
		js\src\xpconnect \
		js\src\liveconnect \
		modules\libutil \
		modules\zlib \
		modules\zlib\standalone \
		netwerk \
		security \
		uriloader \
		intl \
		modules\libpref \
		modules\libimg \
		gfx \
		modules\oji \
		modules\libjar \
		caps \
		expat \
		htmlparser \
		dom \
		view \
		layout \
		xpfe\appfilelocprovider \
		rdf \
		docshell \
		webshell \
		widget \
		embedding \
		editor \
		xpfe\appshell \
		xpfe\components\shistory \
		extensions\cookie \
		extensions\psm-glue \
		$(NULL)
!endif


include <$(DEPTH)\config\rules.mak>

build_small:
	$(MAKE) -f embed.mak export exporting=1
	$(MAKE) -f embed.mak
