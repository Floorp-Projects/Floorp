# -*- makefile -*-
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

ifdef VERBOSE
  $(warning loading test)
endif

space=$(null) $(null)

testD1 = test-dir-1
testD2 = test-dir-2
testD3 = test-dir-3
testD4 = test-dir-4

# Append goals to force var expansion
MAKECMDGOALS := $(MAKECMDGOALS) libs tools

#dir-var = test-dir-1
GENERATED_DIRS = test-dir-1 # test data
GENERATED_DIRS_libs = $(testD2)
GENERATED_DIRS_tools = test-dir-3 $(testD4)


###########################################################################
# undefine directive not supported by older make versions but needed here
# to force a re-include/re-eval for testing.  Unit test should be non-fatal
# unless hacking on makefiles.
###########################################################################

undefine_supported=no
$(eval undefine undefine_supported)
undefine_supported ?= yes

ifeq (yes,$(undefine_supported))
  # Clear defs to re-include for testing
  undefine USE_AUTOTARGETS_MK
  undefine INCLUDED_AUTOTARGETS_MK
else
  $(info ===========================================================================)
  $(warning $(MAKE)[$(MAKE_VERSION)]: makefile directive 'undefined' not supported, disabling test)
  $(info ===========================================================================)
endif

include $(topsrcdir)/config/makefiles/autotargets.mk

ifndef INCLUDED_AUTOTARGETS_MK
  $(error autotargets.mk was not included)
endif

$(call requiredfunction,mkdir_deps)


# Verify test data populated makefile vars correctly
vars = AUTO_DEPS GARBAGE_DIRS GENERATED_DIRS_tools
$(foreach var,$(vars),$(call errorIfEmpty,$(var)))

# Verify target dirs were expanded into GENERATED_DIRS
$(foreach path,$(testD1) $(testD2) $(testD3) $(testD4),\
  $(if $(findstring $(path),$(AUTO_DEPS))\
    ,,$(error AUTO_DEPS missing path $(path))))

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
