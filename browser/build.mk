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
# The Original Code is the Mozilla build system.
#
# The Initial Developer of the Original Code is
# the Mozilla Foundation <http://www.mozilla.org/>.
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Benjamin Smedberg <benjamin@smedbergs.us> (Initial Code)
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

ifndef LIBXUL_SDK
include $(topsrcdir)/toolkit/toolkit-tiers.mk
endif

TIERS += app

ifdef MOZ_EXTENSIONS
tier_app_dirs += extensions
endif

tier_app_dirs += $(MOZ_BRANDING_DIRECTORY)

ifdef MOZ_SERVICES_SYNC
tier_app_dirs += services
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
