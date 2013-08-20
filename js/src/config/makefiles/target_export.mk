# -*- makefile -*-
# vim:set ts=8 sw=8 sts=8 noet:
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

PARALLEL_DIRS_export = $(addsuffix _export,$(PARALLEL_DIRS))

.PHONY: export $(PARALLEL_DIRS_export)

###############
## TIER targets
###############
$(addprefix export_tier_,$(TIERS)): export_tier_%:
	@$(ECHO) "$@"
	$(foreach dir,$(tier_$*_dirs),$(call TIER_DIR_SUBMAKE,export,$(dir)))

#################
## Common targets
#################
ifdef PARALLEL_DIRS
export:: $(PARALLEL_DIRS_export)

$(PARALLEL_DIRS_export): %_export: %/Makefile
	+@$(call SUBMAKE,export,$*)
endif

export:: $(SUBMAKEFILES) $(MAKE_DIRS)
	$(LOOP_OVER_DIRS)
	$(LOOP_OVER_TOOL_DIRS)

