# -*- makefile -*-
# vim:set ts=8 sw=8 sts=8 noet:
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

## Identify function argument types
istype =$(if $(value ${1}),list,scalar)
isval  =$(if $(filter-out list,$(call istype,${1})),true)
isvar  =$(if $(filter-out scalar,$(call istype,${1})),true)

# Access up to 9 arguments passed, option needed to emulate $*
# Inline for function expansion, do not use $(call )
argv  =$(strip
argv +=$(if $(1), $(1))$(if $(2), $(2))$(if $(3), $(3))$(if $(4), $(4))
argv +=$(if $(5), $(5))$(if $(6), $(6))$(if $(7), $(7))$(if $(8), $(8))
argv +=$(if $(9), $(9))
argv +=$(if $(10), $(error makeutils.mk::argv can only handle 9 arguments))
argv +=)

###########################################################################
## Access function args as a simple list, inline within user functions.
## Usage: $(info ** $(call banner,$(getargv)))
##    $(call banner,scalar)
##    $(call banner,list0 list1 list2)
##    $(call banner,ref) ; ref=foo bar tans
## getarglist() would be a more accurate name but is longer to type
getargv = $(if $(call isvar,$(1)),$($(1)),$(argv))

###########################################################################
# Strip [n] leading options from an argument list.  This will allow passing
# extra args to user functions that will not propogate to sub-$(call )'s
# Usage: $(call subargv,2)
subargv =$(wordlist $(1),$(words $(getargv)),$(getargv))

###########################################################################
# Intent: Display a distinct banner heading in the output stream
# Usage: $(call banner,BUILDING: foo bar tans)
# Debug:
#   target-preqs = \
#     $(call banner,target-preqs-BEGIN) \
#     foo bar tans \
#     $(call banner,target-preqs-END) \
#     $(NULL)
#   target: $(target-preqs)

banner = \
$(info ) \
$(info ***************************************************************************) \
$(info ** $(getargv)) \
$(info ***************************************************************************) \
$(NULL)

#####################################################################
# Intent: Determine if a string or pattern is contained in a list
# Usage: strcmp  - $(call if_XinY,clean,$(MAKECMDGOALS))
#      : pattern - $(call if_XinY,clean%,$(MAKECMDGOALS))
is_XinY =$(filter $(1),$(call subargv,3,$(getargv)))

#####################################################################
# Provide an alternate var to support testing
ifdef MAKEUTILS_UNIT_TEST
  mcg_goals=TEST_MAKECMDGOALS
else
  mcg_goals=MAKECMDGOALS
endif

# Intent: Conditionals for detecting common/tier target use
isTargetStem       = $(sort \
  $(foreach var,$(getargv),\
    $(foreach pat,$(var)% %$(var),\
      $(call is_XinY,$(pat),${$(mcg_goals)})\
  )))
isTargetStemClean  = $(call isTargetStem,clean)
isTargetStemExport = $(call isTargetStem,export)
isTargetStemLibs   = $(call isTargetStem,libs)
isTargetStemTools  = $(call isTargetStem,tools)

##################################################
# Intent: Validation functions / unit test helpers

errorifneq =$(if $(subst $(strip $(1)),$(NULL),$(strip $(2))),$(error expected [$(1)] but found [$(2)]))

# Intent: verify function declaration exists
requiredfunction =$(foreach func,$(1) $(2) $(3) $(4) $(5) $(6) $(7) $(8) $(9),$(if $(value $(func)),$(NULL),$(error required function [$(func)] is unavailable)))



## http://www.gnu.org/software/make/manual/make.html#Call-Function
## Usage: o = $(call map,origin,o map $(MAKE))
map = $(foreach val,$(2),$(call $(1),$(val)))


## Disable checking for clean targets
ifeq (,$(filter %clean clean%,$(MAKECMDGOALS))) #{

# Usage: $(call checkIfEmpty,[error|warning] foo NULL bar)
checkIfEmpty =$(foreach var,$(wordlist 2,100,$(argv)),$(if $(strip $($(var))),$(NOP),$(call $(1),Variable $(var) does not contain a value)))

# Usage: $(call errorIfEmpty,foo NULL bar)
errorIfEmpty =$(call checkIfEmpty,error $(argv))
warnIfEmpty  =$(call checkIfEmpty,warning $(argv))

endif #}

###########################################################################
## Common makefile library loader
###########################################################################
ifdef MOZILLA_DIR
topORerr = $(MOZILLA_DIR)
else
topORerr = $(if $(topsrcdir),$(topsrcdir),$(error topsrcdir is not defined))
endif

ifdef USE_AUTOTARGETS_MK # mkdir_deps
  include $(topORerr)/config/makefiles/autotargets.mk
endif

## copy(src, dst): recursive copy
copy_dir = (cd $(1)/. && $(TAR) $(TAR_CREATE_FLAGS) - .) | (cd $(2)/. && tar -xf -)
