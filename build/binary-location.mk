# finds the location of the browser and puts it in the variable $(browser_path)

ifneq (,$(filter OS2 WINCE WINNT,$(OS_ARCH)))
PROGRAM = $(MOZ_APP_NAME)$(BIN_SUFFIX)
else
PROGRAM = $(MOZ_APP_NAME)-bin$(BIN_SUFFIX)
endif

TARGET_DIST = $(TARGET_DEPTH)/dist

ifeq ($(MOZ_BUILD_APP),camino)
browser_path = $(TARGET_DIST)/Camino.app/Contents/MacOS/Camino
else
ifeq ($(OS_ARCH),Darwin)
ifdef MOZ_DEBUG
browser_path = $(TARGET_DIST)/$(MOZ_APP_DISPLAYNAME)Debug.app/Contents/MacOS/$(PROGRAM)
else
browser_path = $(TARGET_DIST)/$(MOZ_APP_DISPLAYNAME).app/Contents/MacOS/$(PROGRAM)
endif
else
browser_path = $(TARGET_DIST)/bin/$(PROGRAM)
endif
endif
