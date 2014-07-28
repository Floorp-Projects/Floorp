# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

ifndef INCLUDED_RULES_MK
include $(topsrcdir)/config/rules.mk
endif

# Make sure that anything that needs to be defined in moz.build wasn't
# overwritten after including rules.mk.
_eval_for_side_effects := $(CHECK_MOZBUILD_VARIABLES)

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

ifeq (.,$(DEPTH))

include root.mk

# Disable build status for mach in top directories without TIERS.
# In practice this disables it when recursing under js/src, which confuses mach.
ifndef TIERS
BUILDSTATUS =
endif

# Main rules (export, compile, libs and tools) call recurse_* rules.
# This wrapping is only really useful for build status.
compile libs export tools::
	$(call BUILDSTATUS,TIER_START $@)
	+$(MAKE) recurse_$@
	$(call BUILDSTATUS,TIER_FINISH $@)

# Special rule that does install-manifests (cf. Makefile.in) + compile
binaries::
	+$(MAKE) recurse_compile

# Carefully avoid $(eval) type of rule generation, which makes pymake slower
# than necessary.
# Get current tier and corresponding subtiers from the data in root.mk.
CURRENT_TIER := $(filter $(foreach tier,compile libs export tools,recurse_$(tier) $(tier)-deps),$(MAKECMDGOALS))
ifneq (,$(filter-out 0 1,$(words $(CURRENT_TIER))))
$(error $(CURRENT_TIER) not supported on the same make command line)
endif
CURRENT_TIER := $(subst recurse_,,$(CURRENT_TIER:-deps=))

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
CURRENT_DIRS := $($(CURRENT_TIER)_dirs)

# The compile tier has different rules from other tiers.
ifeq ($(CURRENT_TIER),compile)

# Need a list of compile targets because we can't use pattern rules:
# https://savannah.gnu.org/bugs/index.php?42833
.PHONY: $(compile_targets)
$(compile_targets):
	$(call SUBMAKE,$(if $(filter $(@D),$(staticdirs)),,$(@F)),$(@D))

else

# Recursion rule for all directories traversed for all subtiers in the
# current tier.
$(addsuffix /$(CURRENT_TIER),$(CURRENT_DIRS)): %/$(CURRENT_TIER):
	$(call SUBMAKE,$(CURRENT_TIER),$*)

.PHONY: $(addsuffix /$(CURRENT_TIER),$(CURRENT_DIRS))

# Dummy rules for possibly inexisting dependencies for the above tier targets
$(addsuffix /Makefile,$(CURRENT_DIRS)) $(addsuffix /backend.mk,$(CURRENT_DIRS)):

ifeq ($(CURRENT_TIER),export)
# At least build/export requires config/export for buildid, but who knows what
# else, so keep this global dependency to make config/export first for now.
$(addsuffix /$(CURRENT_TIER),$(filter-out config,$(CURRENT_DIRS))): config/$(CURRENT_TIER)

# The export tier requires nsinstall, which is built from config. So every
# subdirectory traversal needs to happen after building nsinstall in config, which
# is done with the config/host target. Note the config/host target only exists if
# nsinstall is actually built, which it is not on Windows, because we use
# nsinstall.py there.
ifneq (,$(filter config/host, $(compile_targets)))
$(addsuffix /$(CURRENT_TIER),$(CURRENT_DIRS)): config/host

# Ensure rules for config/host and its possible dependencies.
.PHONY: $(filter %/host, $(compile_targets))
$(filter %/host, $(compile_targets)):
	$(call SUBMAKE,host,$(@D))
endif
endif

endif # ifeq ($(CURRENT_TIER),compile)

else

# Don't recurse if MAKELEVEL is NO_RECURSE_MAKELEVEL as defined above
ifeq ($(NO_RECURSE_MAKELEVEL),$(MAKELEVEL))

compile libs export tools::

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
.PHONY: $(1)

$(1):: $$(SUBMAKEFILES)
	$$(LOOP_OVER_DIRS)

endef

$(foreach subtier,export libs tools,$(eval $(call CREATE_SUBTIER_TRAVERSAL_RULE,$(subtier))))

endif # ifdef TIERS

endif # ifeq ($(NO_RECURSE_MAKELEVEL),$(MAKELEVEL))

endif # ifeq (.,$(DEPTH))

recurse:
	@$(RECURSED_COMMAND)
	$(LOOP_OVER_DIRS)
