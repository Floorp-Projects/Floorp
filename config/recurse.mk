# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

ifndef INCLUDED_RULES_MK
include $(topsrcdir)/config/rules.mk
endif

#########################
# Tier traversal handling
#########################

define CREATE_SUBTIER_TRAVERSAL_RULE
PARALLEL_DIRS_$(1) = $$(addsuffix _$(1),$$(PARALLEL_DIRS))

.PHONY: $(1) $$(PARALLEL_DIRS_$(1))

ifdef PARALLEL_DIRS
$$(PARALLEL_DIRS_$(1)): %_$(1): %/Makefile
	+@$$(call SUBMAKE,$(1),$$*)
endif

$(1):: $$(SUBMAKEFILES)
ifdef PARALLEL_DIRS
	+@$(MAKE) $$(PARALLEL_DIRS_$(1))
endif
	$$(LOOP_OVER_DIRS)

endef

$(foreach subtier,export libs tools,$(eval $(call CREATE_SUBTIER_TRAVERSAL_RULE,$(subtier))))

export:: $(SUBMAKEFILES)
	$(LOOP_OVER_TOOL_DIRS)

tools:: $(SUBMAKEFILES)
	$(foreach dir,$(TOOL_DIRS),$(call SUBMAKE,libs,$(dir)))
