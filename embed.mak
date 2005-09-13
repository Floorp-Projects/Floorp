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
