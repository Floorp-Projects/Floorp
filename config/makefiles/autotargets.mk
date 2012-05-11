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

###########################################################################
# Threadsafe directory creation
# GENERATED_DIRS - Automated creation of these directories.
# Squeeze '//' from the path, easily created by $(dir $(path))
###########################################################################
mkdir_deps =$(subst //,/,$(foreach dir,$(getargv),$(dir)/.mkdir.done))

%/.mkdir.done: # mkdir -p -p => mkdir -p
	$(subst $(SPACE)-p,$(null),$(MKDIR)) -p $(dir $@)
	@$(TOUCH) $@

# A handful of makefiles are attempting "mkdir dot".  Likely not intended
# or stale logic so add a stub target to handle the request and warn for now.
.mkdir.done:
	@echo "WARNING: $(MKDIR) -dot- requested by $(MAKE) -C $(CURDIR) $(MAKECMDGOALS)"
	@$(TOUCH) $@

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
