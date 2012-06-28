# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

ifndef LIBXUL_SDK
include $(topsrcdir)/toolkit/toolkit-tiers.mk
endif

TIERS += app

ifdef MOZ_EXTENSIONS
tier_app_dirs += extensions
endif

tier_app_dirs += $(MOZ_BRANDING_DIRECTORY)

tier_app_dirs += services

ifdef MOZ_WEBAPP_RUNTIME
tier_app_dirs += webapprt
endif

tier_app_dirs += browser
# Never add other tier_app_dirs after browser. They won't get packaged
# properly on mac.

################################################
# Parallel build on Windows with GNU make check

default::
ifeq (,$(findstring pymake,$(MAKE)))
ifeq ($(HOST_OS_ARCH),WINNT)
ifneq (1,$(NUMBER_OF_PROCESSORS))
	@echo $(if $(findstring -j,$(value MAKEFLAGS)), \
$(error You are using GNU make to build Firefox with -jN on Windows. \
This will randomly deadlock. To compile a parallel build on Windows \
run "python -OO build/pymake/make.py -f client.mk build". \
See https://developer.mozilla.org/en/pymake for more details.))
endif
endif
endif

installer:
	@$(MAKE) -C browser/installer installer

package:
	@$(MAKE) -C browser/installer

package-compare:
	@$(MAKE) -C browser/installer package-compare

stage-package:
	@$(MAKE) -C browser/installer stage-package

install::
	@$(MAKE) -C browser/installer install

clean::
	@$(MAKE) -C browser/installer clean

distclean::
	@$(MAKE) -C browser/installer distclean

source-package::
	@$(MAKE) -C browser/installer source-package

upload::
	@$(MAKE) -C browser/installer upload

source-upload::
	@$(MAKE) -C browser/installer source-upload

hg-bundle::
	@$(MAKE) -C browser/installer hg-bundle

l10n-check::
	@$(MAKE) -C browser/locales l10n-check

ifdef ENABLE_TESTS
# Implemented in testing/testsuite-targets.mk

mochitest-browser-chrome:
	$(RUN_MOCHITEST) --browser-chrome
	$(CHECK_TEST_ERROR)

mochitest:: mochitest-browser-chrome

.PHONY: mochitest-browser-chrome
endif
