# -*- makefile -*-
# vim:set ts=8 sw=8 sts=8 noet:
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
#

###########################################################################
#      AUTO_DEPS - A list of deps/targets drived from other macros.
#         *_DEPS - Make dependencies derived from a given macro.
###########################################################################

MKDIR ?= mkdir -p
TOUCH ?= touch

###########################################################################
# Threadsafe directory creation
# GENERATED_DIRS - Automated creation of these directories.
###########################################################################
mkdir_deps =$(foreach dir,$(getargv),$(dir)/.mkdir.done)

ifneq (,$(GENERATED_DIRS))
  tmpauto :=$(call mkdir_deps,GENERATED_DIRS)
  GENERATED_DIRS_DEPS +=$(tmpauto)
  GARBAGE_DIRS        +=$(tmpauto)
endif

## Only define rules once
ifndef INCLUDED_AUTOTARGETS_MK

%/.mkdir.done: # mkdir -p -p => mkdir -p
	$(subst $(SPACE)-p,$(null),$(MKDIR)) -p $(dir $@)
	@$(TOUCH) $@

# A handful of makefiles are attempting "mkdir dot".  Likely not intended
# or stale logic so add a stub target to handle the request and warn for now.
.mkdir.done:
	@echo "WARNING: $(MKDIR) -dot- requested by $(MAKE) -C $(CURDIR) $(MAKECMDGOALS)"
	@$(TOUCH) $@

INCLUDED_AUTOTARGETS_MK = 1
endif

#################################################################
# One ring/dep to rule them all:
#   config/rules.mk::all target is available by default
#   Add $(AUTO_DEPS) as an explicit target dependency when needed.
#################################################################

AUTO_DEPS +=$(GENERATED_DIRS_DEPS)

# Complain loudly if deps have not loaded so getargv != $(NULL)
$(call requiredfunction,getargv)
