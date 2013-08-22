# -*- makefile -*-
# vim:set ts=8 sw=8 sts=8 noet:
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This file contains logic for tier traversal.

define CREATE_SUBTIER_RULE
$(2)_tier_$(1):
	$$(call BUILDSTATUS,SUBTIER_START  $(1) $(2) $$(tier_$(1)_dirs))
	$$(foreach dir,$$(tier_$(1)_dirs),$$(call TIER_DIR_SUBMAKE,$(1),$(2),$$(dir),$(2)))
	$$(call BUILDSTATUS,SUBTIER_FINISH $(1) $(2))

endef

# This function is called and evaluated to produce the rule to build the
# specified tier.
#
# Tiers are traditionally composed of directories that are invoked either
# once (so-called "static" directories) or 3 times with the export, libs, and
# tools sub-tiers.
#
# If the TIER_$(tier)_CUSTOM variable is defined, then these traditional
# tier rules are ignored and each directory in the tier is executed via a
# sub-make invocation (make -C).
define CREATE_TIER_RULE
tier_$(1)::
ifdef TIER_$(1)_CUSTOM
	$$(foreach dir,$$($$@_dirs),$$(call SUBMAKE,,$$(dir)))
else
	$(call BUILDSTATUS,TIER_START $(1) $(if $(tier_$(1)_staticdirs),static )$(if $(tier_$(1)_dirs),export libs tools))
ifneq (,$(tier_$(1)_staticdirs))
	$(call BUILDSTATUS,SUBTIER_START  $(1) static $$($$@_staticdirs))
	$$(foreach dir,$$($$@_staticdirs),$$(call TIER_DIR_SUBMAKE,$(1),static,$$(dir),,1))
	$(call BUILDSTATUS,SUBTIER_FINISH $(1) static)
endif
ifneq (,$(tier_$(1)_dirs))
	$$(MAKE) export_$$@
	$$(MAKE) libs_$$@
	$$(MAKE) tools_$$@
endif
	$(call BUILDSTATUS,TIER_FINISH $(1))
endif

$(foreach subtier,export libs tools,$(call CREATE_SUBTIER_RULE,$(1),$(subtier)))

endef
