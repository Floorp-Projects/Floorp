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
# Mozilla.org.
# Portions created by the Initial Developer are Copyright (C) 2010
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
#     Jeff Hammel <jhammel@mozilla.com>     (Original author)
# 
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

# finds the location of the browser and puts it in the variable $(browser_path)

ifneq (,$(filter OS2 WINNT,$(OS_ARCH)))
PROGRAM = $(MOZ_APP_NAME)$(BIN_SUFFIX)
else
ifeq ($(MOZ_BUILD_APP),mobile)
PROGRAM = $(MOZ_APP_NAME)$(BIN_SUFFIX)
else
PROGRAM = $(MOZ_APP_NAME)-bin$(BIN_SUFFIX)
endif
endif

TARGET_DIST = $(TARGET_DEPTH)/dist

ifeq ($(MOZ_BUILD_APP),camino)
browser_path = $(TARGET_DIST)/Camino.app/Contents/MacOS/Camino
else
ifeq ($(OS_ARCH),Darwin)
browser_path = $(TARGET_DIST)/$(MOZ_MACBUNDLE_NAME)/Contents/MacOS/$(PROGRAM)
else
browser_path = $(TARGET_DIST)/bin/$(PROGRAM)
endif
endif
