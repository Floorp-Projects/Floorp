# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

A11Y_LOG = 0
ifdef MOZ_DEBUG
  A11Y_LOG = 1
endif
ifeq (,$(filter aurora beta release esr,$(MOZ_UPDATE_CHANNEL)))
  A11Y_LOG = 1
endif
