# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

ifneq (extensions,$(MOZ_BUILD_APP))
$(error This file shouldn't be included without --enable-application=extensions)
endif

ifndef MOZ_EXTENSIONS
$(error You forgot to set --enable-extensions)
endif

TIERS += app
tier_app_dirs += extensions

installer:
	@echo Check each extension for an installer.
	@exit 1
