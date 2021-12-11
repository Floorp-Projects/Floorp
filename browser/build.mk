# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

package:
	@$(MAKE) -C browser/installer

package-compare:
	@$(MAKE) -C browser/installer package-compare

stage-package:
	@$(MAKE) -C browser/installer stage-package

install::
	@$(MAKE) -C browser/installer install

source-package::
	@$(MAKE) -C browser/installer source-package

upload::
	@$(MAKE) -C browser/installer upload

source-upload::
	@$(MAKE) -C browser/installer source-upload

hg-bundle::
	@$(MAKE) -C browser/installer hg-bundle

wget-en-US:
	@$(MAKE) -C browser/locales $@

merge-% installers-% langpack-% chrome-%:
	$(MAKE) -C browser/locales $@

ifdef ENABLE_TESTS
# Implemented in testing/testsuite-targets.mk

mochitest-browser-chrome:
	$(RUN_MOCHITEST) --flavor=browser
	$(CHECK_TEST_ERROR)

mochitest:: mochitest-browser-chrome

.PHONY: mochitest-browser-chrome

endif
