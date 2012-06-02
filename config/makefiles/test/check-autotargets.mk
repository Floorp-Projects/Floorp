# -*- makefile -*-
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

ifdef VERBOSE
  $(warning loading test)
endif

space=$(null) $(null)
GENERATED_DIRS = bogus # test data

NOWARN_AUTOTARGETS = 1 # Unit test includes makefile twice.

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


# relpath
path  := foo/bar.c
exp   := foo/.mkdir.done
found := $(call mkdir_deps,$(dir $(path)))
ifneq ($(exp),$(found))
  $(error mkdir_deps($(path))=$(exp) not set correctly [$(found)])
endif

# abspath
path  := /foo//bar/
exp   := /foo/bar/.mkdir.done
found := $(call mkdir_deps,$(path))
ifneq ($(exp),$(found))
  $(error mkdir_deps($(path))=$(exp) not set correctly [$(found)])
endif


## verify strip_slash
#####################

path  := a/b//c///d////e/////
exp   := a/b/c/d/e/.mkdir.done
found := $(call mkdir_deps,$(path))
ifneq ($(exp),$(found))
  $(error mkdir_deps($(path))=$(exp) not set correctly [$(found)])
endif


## verify mkdir_stem()
######################
path  := verify/mkdir_stem
pathD = $(call mkdir_deps,$(path))
pathS = $(call mkdir_stem,$(pathD))
exp   := $(path)

ifeq ($(pathD),$(pathS))
  $(error mkdir_deps and mkdir_stem should not match [$(pathD)])
endif
ifneq ($(pathS),$(exp))
  $(error mkdir_stem=[$(pathS)] != exp=[$(exp)])
endif


## Verify embedded whitespace has been protected
path  := a/b$(space)c//d
exp   := a/b$(space)c/d
found := $(call slash_strip,$(path))
ifneq ($(exp),$(found))
  $(error slash_strip($(path))=$(exp) not set correctly [$(found)])
endif
