# -*- makefile -*-
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

ifdef VERBOSE
  $(warning loading test)
endif


$(call requiredfunction,getargv)
$(call requiredfunction,subargv)
$(call requiredfunction,istype isval isvar)

# arg_scalar = [scalar|literal]
arg_list = foo bar
arg_ref  = arg_list

## Identify type of function argument(s)
########################################
ifneq (scalar,$(call istype,arg_scalar))
  $(error istype(arg_scalar)=scalar, found [$(call istype,arg_scalar)])
endif
ifneq (list,$(call istype,arg_list))
  $(error istype(arg_list)=list, found [$(call istype,arg_list)])
endif
ifneq (list,$(call istype,arg_ref))
  $(error istype(arg_ref)=list, found [$(call istype,arg_ref)])
endif

## Type == scalar or a list of values
#####################################
ifneq (true,$(call isval,scalar))
  $(error isval(scalar)=true, found [$(call isval,scalar)])
endif
ifneq ($(NULL),$(call isval,arg_list))
  $(error isval(arg_list)=null, found [$(call isval,arg_list)])
endif

## type == reference: macro=>macro => $($(1))
#############################################
ifneq ($(NULL),$(call isvar,scalar))
  $(error isvar(scalar)=$(NULL), found [$(call isvar,scalar)])
endif
ifneq (true,$(call isvar,arg_list))
  $(error isvar(arg_list)=true, found [$(call isvar,arg_list)])
endif
ifneq (true,$(call isvar,arg_ref))
  $(error isvar(arg_ref)=true, found [$(call isvar,arg_ref)])
endif

# Verify getargv expansion
##########################
ifneq (scalar,$(call getargv,scalar))
  $(error getargv(scalar)=scalar, found [$(call getargv,scalar)])
endif
ifneq ($(arg_list),$(call getargv,arg_list))
  $(error getargv(arg_list)=list, found [$(call getargv,arg_list)])
endif
ifneq (arg_list,$(call getargv,arg_ref))
  $(error getargv(arg_ref)=list, found [$(call getargv,arg_ref)])
endif

###########################################################################
##
###########################################################################
ifdef MANUAL_TEST #{
  # For automated testing a callback is needed that can set an external status
  # variable that can be tested.  Syntax is tricky to get correct functionality.
  ifdef VERBOSE
    $(info )
    $(info ===========================================================================)
    $(info Running test: checkIfEmpty)
    $(info ===========================================================================)
  endif

  #status =
  #setTRUE =status=true
  #setFALSE =status=$(NULL)
  #$(call checkIfEmpty,setFALSE NULL)
  #$(if $(status),$(error checkIfEmpty(xyz) failed))
  #$(call checkIfEmpty,setTRUE xyz)
  #$(if $(status),$(error checkIfEmpty(xyz) failed))
  xyz=abc
  $(info STATUS: warnIfEmpty - two vars)
  $(call warnIfEmpty,foo xyz bar)
  $(info STATUS: errorIfEmpty - on first var)
  $(call errorIfEmpty,foo xyz bar)
  $(error TEST FAILED: processing should not reach this point)
endif #}

# Verify subargv expansion
##########################
subargs=foo bar tans fans
subargs_exp=tans fans
subargs_found=$(call subargv,4,$(subargs))
ifneq ($(subargs_exp),$(subargs_found))
  $(error subargv(4,$(subargs)): expected [$(subargs_exp)] found [$(subargs_found)])
endif
