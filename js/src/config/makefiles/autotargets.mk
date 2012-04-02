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


###########################################################################
# Threadsafe directory creation
# GENERATED_DIRS - Automated creation of these directories.
###########################################################################
mkdir_deps =$(foreach dir,$($(1)),$(dir)/.mkdir.done)

ifneq (,$(GENERATED_DIRS))
  tmpauto :=$(call mkdir_deps,GENERATED_DIRS)
  GENERATED_DIRS_DEPS +=$(tmpauto)
  GARBAGE_DIRS        +=$(tmpauto)
endif

%/.mkdir.done:
	$(subst $(SPACE)-p,$(null),$(MKDIR)) -p $(dir $@)
	@$(TOUCH) $@

#################################################################
# One ring/dep to rule them all:
#   config/rules.mk::all target is available by default
#   Add $(AUTO_DEPS) as an explicit target dependency when needed.
#################################################################
AUTO_DEPS +=$(GENERATED_DIRS_DEPS)

