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

# The list of modules to be built.
# The modules inside exporting sections export header files but do
# not need to be built.

DIRS = \
  nsprpub \
  config \
  include                      \
  jpeg \
!if defined (exporting)
  sun-java                     \
!endif
  modules\libreg \
  string \
  xpcom \
!if defined (MOZ_STATIC_COMPONENTS)
  modules\staticmod
!endif		
  dbm \
  modules\libutil \
  modules\zlib \
  modules\zlib\standalone \
  netwerk \
!if defined(BUILD_PSM)
  security \
!endif
  js \
  modules\libjar \
  modules\libimg \
  modules\libpref \
  modules\oji \
  caps \
  intl \
  rdf \
  expat \
  gfx \
!if defined (exporting)
  modules\plugin \
!endif
  uriloader \
  htmlparser \
  widget \
  dom \
  view \
  content \
  layout \
  docshell \
  webshell \
  embedding \
  profile \
  editor \
  extensions\cookie \
!if defined (exporting)
  extensions\wallet \
!endif		
!if defined(BUILD_PSM)
  extensions\psm-glue \
!endif
!if defined (exporting)
  xpfe \
!else
  xpfe\appshell \
  xpfe\components\shistory \
  xpfe\global \
!endif
  themes\classic \
  embedding\config \
  $(NULL)


include <$(DEPTH)\config\rules.mak>

all:: build_embed

build_embed:
  $(MAKE) -f embed.mak export exporting=1
  $(MAKE) -f embed.mak install
