# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

ifndef INCLUDED_RULES_MK
include $(topsrcdir)/config/rules.mk
endif

# The traditional model of directory traversal with make is as follows:
#   make -C foo
#     Entering foo
#     make -C bar
#       Entering foo/bar
#     make -C baz
#       Entering foo/baz
#   make -C qux
#     Entering qux
#
# Pseudo derecurse transforms the above into:
#   make -C foo
#   make -C foo/bar
#   make -C foo/baz
#   make -C qux

# MOZ_PSEUDO_DERECURSE can have values other than 1.
ifeq (1_.,$(if $(MOZ_PSEUDO_DERECURSE),1)_$(DEPTH))

include root.mk

# Disable build status for mach in top directories without TIERS.
# In practice this disables it when recursing under js/src, which confuses mach.
ifndef TIERS
BUILDSTATUS =
endif

# Main rules (export, compile, libs and tools) call recurse_* rules.
# This wrapping is only really useful for build status.
compile libs export tools::
	$(call BUILDSTATUS,TIER_START $@ $($@_subtiers))
	+$(MAKE) recurse_$@
	$(call BUILDSTATUS,TIER_FINISH $@)

# Carefully avoid $(eval) type of rule generation, which makes pymake slower
# than necessary.
# Get current tier and corresponding subtiers from the data in root.mk.
CURRENT_TIER := $(filter $(foreach tier,compile libs export tools,recurse_$(tier)),$(MAKECMDGOALS))
ifneq (,$(filter-out 0 1,$(words $(CURRENT_TIER))))
$(error $(CURRENT_TIER) not supported on the same make command line)
endif
CURRENT_TIER := $(subst recurse_,,$(CURRENT_TIER))
CURRENT_SUBTIERS := $($(CURRENT_TIER)_subtiers)

# The rules here are doing directory traversal, so we don't want further
# recursion to happen when running make -C subdir $tier. But some make files
# further call make -C something else, and sometimes expect recursion to
# happen in that case (see browser/metro/locales/Makefile.in for example).
# Conveniently, every invocation of make increases MAKELEVEL, so only stop
# recursion from happening at current MAKELEVEL + 1.
ifdef CURRENT_TIER
ifeq (0,$(MAKELEVEL))
export NO_RECURSE_MAKELEVEL=1
else
export NO_RECURSE_MAKELEVEL=$(word $(MAKELEVEL),2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20)
endif
endif

# Get all directories traversed for all subtiers in the current tier, or use
# directly the $(*_dirs) variables available in root.mk when there is no
# TIERS (like for js/src).
CURRENT_DIRS := $(or $($(CURRENT_TIER)_dirs),$(foreach subtier,$(CURRENT_SUBTIERS),$($(CURRENT_TIER)_subtier_$(subtier))))

# Subtier delimiter rules
$(addprefix subtiers/,$(addsuffix _start/$(CURRENT_TIER),$(CURRENT_SUBTIERS))): subtiers/%_start/$(CURRENT_TIER):
	$(call BUILDSTATUS,SUBTIER_START $(CURRENT_TIER) $* $(if $(BUG_915535_FIXED),$($(CURRENT_TIER)_subtier_$*)))

$(addprefix subtiers/,$(addsuffix _finish/$(CURRENT_TIER),$(CURRENT_SUBTIERS))): subtiers/%_finish/$(CURRENT_TIER):
	$(call BUILDSTATUS,SUBTIER_FINISH $(CURRENT_TIER) $*)

# Recursion rule for all directories traversed for all subtiers in the
# current tier.
# root.mk defines subtier_of_* variables, that map a normalized subdir path to
# a subtier name (e.g. subtier_of_memory_jemalloc = base)
$(addsuffix /$(CURRENT_TIER),$(CURRENT_DIRS)): %/$(CURRENT_TIER):
ifdef BUG_915535_FIXED
	$(call BUILDSTATUS,TIERDIR_START $(CURRENT_TIER) $(subtier_of_$(subst /,_,$*)) $*)
endif
	+@$(MAKE) -C $* $(if $(filter $*,$(tier_$(subtier_of_$(subst /,_,$*))_staticdirs)),,$(CURRENT_TIER))
ifdef BUG_915535_FIXED
	$(call BUILDSTATUS,TIERDIR_FINISH $(CURRENT_TIER) $(subtier_of_$(subst /,_,$*)) $*)
endif

# The export tier requires nsinstall, which is built from config. So every
# subdirectory traversal needs to happen after traversing config.
ifeq ($(CURRENT_TIER),export)
$(addsuffix /$(CURRENT_TIER),$(filter-out config,$(CURRENT_DIRS))): config/$(CURRENT_TIER)
endif

else

# Don't recurse if MAKELEVEL is NO_RECURSE_MAKELEVEL as defined above, but
# still recurse for externally managed make files (gyp-generated ones).
ifeq ($(EXTERNALLY_MANAGED_MAKE_FILE)_$(NO_RECURSE_MAKELEVEL),_$(MAKELEVEL))

compile libs export tools::

else
#########################
# Tier traversal handling
#########################

ifdef TIERS

libs export tools::
	$(call BUILDSTATUS,TIER_START $@ $(filter-out $(if $(filter export,$@),,precompile),$(TIERS)))
	$(foreach tier,$(TIERS), $(if $(filter-out libs_precompile tools_precompile,$@_$(tier)), \
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

$(foreach subtier,export libs tools,$(eval $(call CREATE_SUBTIER_TRAVERSAL_RULE,$(subtier))))

tools export:: $(SUBMAKEFILES)
	$(LOOP_OVER_TOOL_DIRS)

endif # ifdef TIERS

endif # ifeq ($(EXTERNALLY_MANAGED_MAKE_FILE)_$(NO_RECURSE_MAKELEVEL),_$(MAKELEVEL))

endif # ifeq (1_.,$(MOZ_PSEUDO_DERECURSE)_$(DEPTH))
