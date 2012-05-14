# -*- makefile -*-
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

ifdef VERBOSE
  $(warning loading test)
endif

GENERATED_DIRS = bogus # test data

undefine USE_AUTOTARGETS_MK
undefine INCLUDED_AUTOTARGETS_MK
include $(topsrcdir)/config/makefiles/autotargets.mk

ifndef INCLUDED_AUTOTARGETS_MK
  $(error autotargets.mk was not included
endif

$(call requiredfunction,mkdir_deps)


# Verify test data populated makefile vars correctly
vars = AUTO_DEPS GARBAGE_DIRS GENERATED_DIRS_DEPS 
$(foreach var,$(vars),$(call errorIfEmpty,$(var)))

# Data should also be valid
ifneq (bogus,$(findstring bogus,$(AUTO_DEPS)))
  $(error AUTO_DEPS=[$(AUTO_DEPS)] is not set correctly)
endif

path  = foo/bar.c
exp = foo/.mkdir.done
found = $(call mkdir_deps,$(dir $(path)))
ifneq ($(exp),$(found))
  $(error mkdir_deps($(path))=$(exp) not set correctly [$(found)])
endif
