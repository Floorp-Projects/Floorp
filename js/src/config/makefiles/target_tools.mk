# -*- makefile -*-
# vim:set ts=8 sw=8 sts=8 noet:
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

PARALLEL_DIRS_tools = $(addsuffix _tools,$(PARALLEL_DIRS))

.PHONY: tools $(PARALLEL_DIRS_tools)

###############
## TIER targets
###############
$(addprefix tools_tier_,$(TIERS)): tools_tier_%:
	@$(ECHO) "$@"
	$(foreach dir,$(tier_$*_dirs),$(call TIER_DIR_SUBMAKE,tools,$(dir)))

#################
## Common targets
#################
ifdef PARALLEL_DIRS
tools:: $(PARALLEL_DIRS_tools)

$(PARALLEL_DIRS_tools): %_tools: %/Makefile
	+@$(call SUBMAKE,tools,$*)
endif

tools:: $(SUBMAKEFILES) $(MAKE_DIRS)
	$(LOOP_OVER_DIRS)
ifneq (,$(strip $(TOOL_DIRS)))
	$(foreach dir,$(TOOL_DIRS),$(call SUBMAKE,libs,$(dir)))
endif

# EOF
