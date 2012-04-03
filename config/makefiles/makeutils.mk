# -*- makefile -*-
# vim:set ts=8 sw=8 sts=8 noet:
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# Usage: $(call banner,foo bar tans)
banner =\
$(info )\
$(info ***************************************************************************)\
$(info ** BANNER: $(1) $(2) $(3) $(4) $(5) $(6) $(7) $(8) $(9))\
$(info ***************************************************************************)\

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

## http://www.gnu.org/software/make/manual/make.html#Call-Function
## Usage: o = $(call map,origin,o map $(MAKE))
map = $(foreach val,$(2),$(call $(1),$(val)))


# Usage: $(call checkIfEmpty,[error|warning] foo NULL bar)
checkIfEmpty =$(foreach var,$(wordlist 2,100,$(getargv)),$(if $(strip $($(var))),$(NOP),$(call $(1),Variable $(var) does not contain a value)))

# Usage: $(call errorIfEmpty,foo NULL bar)
errorIfEmpty =$(call checkIfEmpty,error $(getargv))
warnIfEmpty  =$(call checkIfEmpty,warning $(getargv))


