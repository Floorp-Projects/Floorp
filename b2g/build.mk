# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

ifndef LIBXUL_SDK
include $(topsrcdir)/toolkit/toolkit-tiers.mk
else
ifdef ENABLE_TESTS
tier_testharness_dirs += \
  testing/mochitest \
  $(NULL)
endif
endif

TIERS += app

ifdef MOZ_EXTENSIONS
tier_app_dirs += extensions
endif

tier_app_dirs += \
  $(MOZ_BRANDING_DIRECTORY) \
  b2g \
  $(NULL)


installer: 
	@$(MAKE) -C b2g/installer installer

package:
	@$(MAKE) -C b2g/installer

install::
	@echo "B2G can't be installed directly."
	@exit 1

upload::
	@$(MAKE) -C b2g/installer upload

ifdef ENABLE_TESTS
# Implemented in testing/testsuite-targets.mk

mochitest-browser-chrome:
	$(RUN_MOCHITEST) --browser-chrome
	$(CHECK_TEST_ERROR)

mochitest:: mochitest-browser-chrome

.PHONY: mochitest-browser-chrome
endif

