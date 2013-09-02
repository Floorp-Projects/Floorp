# -*- makefile -*-
# vim:set ts=8 sw=8 sts=8 noet:
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
#

INCLUDED_MOZCONFIG_MK = 1

# We are pulling in the mozconfig exports, so we only need to run once for the
# whole make process tree (not for each sub-make). Export this so the sub-makes
# don't read mozconfig multiple times.
export INCLUDED_MOZCONFIG_MK

MOZCONFIG_LOADER := build/autoconf/mozconfig2client-mk

define CR


endef

# topsrcdir is used by rules.mk (set from the generated Makefile), while
# TOPSRCDIR is used by client.mk
ifneq (,$(topsrcdir))
top := $(topsrcdir)
else
top := $(TOPSRCDIR)
endif
# As $(shell) doesn't preserve newlines, use sed to replace them with an
# unlikely sequence (||), which is then replaced back to newlines by make
# before evaluation.
$(eval $(subst ||,$(CR),$(shell _PYMAKE=$(.PYMAKE) $(top)/$(MOZCONFIG_LOADER) $(top) 2> $(top)/.mozconfig.out | sed 's/$$/||/')))
