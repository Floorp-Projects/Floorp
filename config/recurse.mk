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

# Main rules (export, compile, binaries, libs and tools) call recurse_* rules.
# This wrapping is only really useful for build status.
compile binaries libs export tools::
	$(call BUILDSTATUS,TIER_START $@)
	+$(MAKE) recurse_$@
	$(call BUILDSTATUS,TIER_FINISH $@)

# Carefully avoid $(eval) type of rule generation, which makes pymake slower
# than necessary.
# Get current tier and corresponding subtiers from the data in root.mk.
CURRENT_TIER := $(filter $(foreach tier,compile binaries libs export tools,recurse_$(tier) $(tier)-deps),$(MAKECMDGOALS))
ifneq (,$(filter-out 0 1,$(words $(CURRENT_TIER))))
$(error $(CURRENT_TIER) not supported on the same make command line)
endif
CURRENT_TIER := $(subst recurse_,,$(CURRENT_TIER:-deps=))
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
TIER_DIRS = $(or $($(1)_dirs),$(foreach subtier,$($(1)_subtiers),$($(1)_subtier_$(subtier))))
CURRENT_DIRS := $(call TIER_DIRS,$(CURRENT_TIER))

ifneq (,$(filter binaries libs,$(CURRENT_TIER)))
WANT_STAMPS = 1
STAMP_TOUCH = $(TOUCH) $(@D)/binaries
endif

# Subtier delimiter rules
$(addprefix subtiers/,$(addsuffix _start/$(CURRENT_TIER),$(CURRENT_SUBTIERS))): subtiers/%_start/$(CURRENT_TIER): $(if $(WANT_STAMPS),$(call mkdir_deps,subtiers/%_start))
	@$(STAMP_TOUCH)

$(addprefix subtiers/,$(addsuffix _finish/$(CURRENT_TIER),$(CURRENT_SUBTIERS))): subtiers/%_finish/$(CURRENT_TIER): $(if $(WANT_STAMPS),$(call mkdir_deps,subtiers/%_finish))
	@$(STAMP_TOUCH)

$(addprefix subtiers/,$(addsuffix /$(CURRENT_TIER),$(CURRENT_SUBTIERS))): %/$(CURRENT_TIER): $(if $(WANT_STAMPS),$(call mkdir_deps,%))
	@$(STAMP_TOUCH)

GARBAGE_DIRS += subtiers

# Recursion rule for all directories traversed for all subtiers in the
# current tier.
# root.mk defines subtier_of_* variables, that map a normalized subdir path to
# a subtier name (e.g. subtier_of_memory_jemalloc = base)
$(addsuffix /$(CURRENT_TIER),$(CURRENT_DIRS)): %/$(CURRENT_TIER):
	+@$(MAKE) -C $* $(if $(filter $*,$(tier_$(subtier_of_$(subst /,_,$*))_staticdirs)),,$(CURRENT_TIER))
# Ensure existing stamps are up-to-date, but don't create one if submake didn't create one.
	$(if $(wildcard $@),@$(STAMP_TOUCH))

# Dummy rules for possibly inexisting dependencies for the above tier targets
$(addsuffix /Makefile,$(CURRENT_DIRS)) $(addsuffix /backend.mk,$(CURRENT_DIRS)):

# The export tier requires nsinstall, which is built from config. So every
# subdirectory traversal needs to happen after traversing config.
ifeq ($(CURRENT_TIER),export)
$(addsuffix /$(CURRENT_TIER),$(filter-out config,$(CURRENT_DIRS))): config/$(CURRENT_TIER)
endif

ifdef COMPILE_ENVIRONMENT
ifneq (,$(filter libs binaries,$(CURRENT_TIER)))
# When doing a "libs" build, target_libs.mk ensures the interesting dependency data
# is available in the "binaries" stamp. Once recursion is done, aggregate all that
# dependency info so that stamps depend on relevant files and relevant other stamps.
# When doing a "binaries" build, the aggregate dependency file and those stamps are
# used and allow to skip recursing directories where changes are not going to require
# rebuild. A few directories, however, are still traversed all the time, mostly, the
# gyp managed ones and js/src.
# A few things that are not traversed by a "binaries" build, but should, in an ideal
# world, are nspr, nss, icu and ffi.
recurse_$(CURRENT_TIER):
	@$(MAKE) binaries-deps

# Creating binaries-deps.mk directly would make us build it twice: once when beginning
# the build because of the include, and once at the end because of the stamps.
binaries-deps: $(addsuffix /binaries,$(CURRENT_DIRS))
	@$(call py_action,link_deps,-o $@.mk --group-by-depfile --topsrcdir $(topsrcdir) --topobjdir $(DEPTH) --dist $(DIST) --guard $(addprefix ',$(addsuffix ',$^)))
	@$(TOUCH) $@

ifeq (recurse_binaries,$(MAKECMDGOALS))
$(call include_deps,binaries-deps.mk)
endif

endif

DIST_GARBAGE += binaries-deps.mk binaries-deps

endif

else

# Don't recurse if MAKELEVEL is NO_RECURSE_MAKELEVEL as defined above
ifeq ($(NO_RECURSE_MAKELEVEL),$(MAKELEVEL))

compile binaries libs export tools::

else
#########################
# Tier traversal handling
#########################

ifdef TIERS

libs export tools::
	$(call BUILDSTATUS,TIER_START $@)
	$(foreach tier,$(TIERS), $(if $(filter-out libs_precompile tools_precompile,$@_$(tier)), \
		$(if $(filter libs,$@),$(foreach dir, $(tier_$(tier)_staticdirs), $(call TIER_DIR_SUBMAKE,$@,$(tier),$(dir),,1))) \
		$(foreach dir, $(tier_$(tier)_dirs), $(call TIER_DIR_SUBMAKE,$@,$(tier),$(dir),$@))))
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

$(foreach subtier,export compile binaries libs tools,$(eval $(call CREATE_SUBTIER_TRAVERSAL_RULE,$(subtier))))

tools export:: $(SUBMAKEFILES)
	$(LOOP_OVER_TOOL_DIRS)

endif # ifdef TIERS

endif # ifeq ($(NO_RECURSE_MAKELEVEL),$(MAKELEVEL))

endif # ifeq (1_.,$(MOZ_PSEUDO_DERECURSE)_$(DEPTH))

ifdef MOZ_PSEUDO_DERECURSE

ifdef COMPILE_ENVIRONMENT

# Aggregate all dependency files relevant to a binaries build except in
# the mozilla top-level directory.
ifneq (.,$(DEPTH))
ALL_DEP_FILES := \
  $(BINARIES_PP) \
  $(addsuffix .pp,$(addprefix $(MDDEPDIR)/,$(sort \
    $(TARGETS) \
    $(filter-out $(SOBJS) $(ASOBJS) $(EXCLUDED_OBJS),$(OBJ_TARGETS)) \
  ))) \
  $(NULL)
endif

binaries libs:: $(TARGETS) $(BINARIES_PP)
ifneq (.,$(DEPTH))
	@$(if $^,$(call py_action,link_deps,-o binaries --group-all --topsrcdir $(topsrcdir) --topobjdir $(DEPTH) --dist $(DIST) $(ALL_DEP_FILES)))
endif

endif

endif # ifdef MOZ_PSEUDO_DERECURSE

recurse:
	@$(RECURSED_COMMAND)
	$(LOOP_OVER_PARALLEL_DIRS)
	$(LOOP_OVER_DIRS)
	$(LOOP_OVER_TOOL_DIRS)
