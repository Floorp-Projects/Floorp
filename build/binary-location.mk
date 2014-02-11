# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# finds the location of the browser and puts it in the variable $(browser_path)

ifneq (,$(filter WINNT,$(OS_ARCH)))
program = $(MOZ_APP_NAME)$(BIN_SUFFIX)
else
program = $(MOZ_APP_NAME)-bin$(BIN_SUFFIX)
endif

TARGET_DIST = $(TARGET_DEPTH)/dist

ifeq ($(MOZ_BUILD_APP),camino)
browser_path = $(TARGET_DIST)/Camino.app/Contents/MacOS/Camino
else
ifeq ($(OS_ARCH),Darwin)
browser_path = $(TARGET_DIST)/$(MOZ_MACBUNDLE_NAME)/Contents/MacOS/$(program)
else
browser_path = $(TARGET_DIST)/bin/$(program)
endif
endif
