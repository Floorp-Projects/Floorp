# -*- makefile -*-
# vim:set ts=8 sw=8 sts=8 noet:
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
#

ifndef INCLUDED_AUTOTARGETS_MK #{

# Conditional does not wrap the entire file so multiple
# includes will be able to accumulate dependencies.

###########################################################################
#      AUTO_DEPS - A list of deps/targets drived from other macros.
###########################################################################

MKDIR ?= mkdir -p
TOUCH ?= touch

# declare for local use, rules.mk may not have been loaded
space = $(NULL) $(NULL)

# Deps will be considered intermediate when used as a pre-requisite for
# wildcard targets.  Inhibit their removal, mkdir -p is a standalone op.
.PRECIOUS: %/.mkdir.done

#########################
##---]  FUNCTIONS  [---##
#########################

# Squeeze can be overzealous, restore root for abspath
getPathPrefix =$(if $(filter /%,$(1)),/)

# Squeeze '//' from the path, easily created by string functions
_slashSqueeze =$(foreach val,$(getargv),$(call getPathPrefix,$(val))$(subst $(space),/,$(strip $(subst /,$(space),$(val)))))

# Squeeze extraneous directory slashes from the path
#  o protect embedded spaces within the path
#  o replace //+ sequences with /
slash_strip =\
  $(strip \
    $(subst <--[**]-->,$(space),\
	$(call _slashSqueeze,\
    $(subst $(space),<--[**]-->,$(1))\
  )))

# Extract directory path from a dependency file.
mkdir_stem =$(foreach val,$(getargv),$(subst /.mkdir.done,$(NULL),$(val)))

## Generate timestamp file for threadsafe directory creation
mkdir_deps =$(foreach dir,$(getargv),$(call slash_strip,$(dir)/.mkdir.done))

#######################
##---]  TARGETS  [---##
#######################

%/.mkdir.done: # mkdir -p -p => mkdir -p
	$(subst $(space)-p,$(null),$(MKDIR)) -p $(dir $@)
	@$(TOUCH) $@

# A handful of makefiles are attempting "mkdir dot".  Likely not intended
# or stale logic so add a stub target to handle the request and warn for now.
.mkdir.done:
ifndef NOWARN_AUTOTARGETS # {
	@echo "WARNING: $(MKDIR) -dot- requested by $(MAKE) -C $(CURDIR) $(MAKECMDGOALS)"
	@$(TOUCH) $@
endif #}

INCLUDED_AUTOTARGETS_MK = 1
endif #}


## Accumulate deps and cleanup
ifneq (,$(GENERATED_DIRS))
  tmpauto :=$(call mkdir_deps,GENERATED_DIRS)
  GENERATED_DIRS_DEPS +=$(tmpauto)
  GARBAGE_DIRS        +=$(GENERATED_DIRS)
endif

#################################################################
# One ring/dep to rule them all:
#   config/rules.mk::all target is available by default
#   Add $(AUTO_DEPS) as an explicit target dependency when needed.
#################################################################

AUTO_DEPS +=$(GENERATED_DIRS_DEPS)

# Complain loudly if deps have not loaded so getargv != $(NULL)
$(call requiredfunction,getargv)
