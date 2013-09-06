# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

ifndef INCLUDED_RULES_MK
include $(topsrcdir)/config/rules.mk
endif

#########################
# Tier traversal handling
#########################

ifdef TIERS

compile libs export tools::
	$(call BUILDSTATUS,TIER_START $@ $(filter-out $(if $(filter export,$@),,precompile),$(TIERS)))
	$(foreach tier,$(TIERS), $(if $(filter-out compile_precompile libs_precompile tools_precompile,$@_$(tier)), \
		$(call BUILDSTATUS,SUBTIER_START $@ $(tier) $(if $(filter libs,$@),$(tier_$(tier)_staticdirs)) $(tier_$(tier)_dirs)) \
		$(if $(filter libs,$@),$(foreach dir, $(tier_$(tier)_staticdirs), $(call TIER_DIR_SUBMAKE,$@,$(tier),$(dir),,1))) \
		$(foreach dir, $(tier_$(tier)_dirs), $(call TIER_DIR_SUBMAKE,$@,$(tier),$(dir),$@)) \
		$(call BUILDSTATUS,SUBTIER_FINISH $@ $(tier))))
	$(call BUILDSTATUS,TIER_FINISH $@)

else

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

$(foreach subtier,export compile libs tools,$(eval $(call CREATE_SUBTIER_TRAVERSAL_RULE,$(subtier))))

compile export:: $(SUBMAKEFILES)
	$(LOOP_OVER_TOOL_DIRS)

tools:: $(SUBMAKEFILES)
	$(foreach dir,$(TOOL_DIRS),$(call SUBMAKE,libs,$(dir)))

endif
