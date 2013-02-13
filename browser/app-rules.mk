PURGECACHES_DIRS = $(DIST)/bin/browser
ifdef MOZ_METRO
PURGECACHES_DIRS += $(DIST)/bin/metro
endif
