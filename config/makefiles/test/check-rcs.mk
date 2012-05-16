# -*- makefile -*-
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

ifdef VERBOSE
  $(warning loading test)
endif

include $(topsrcdir)/config/makefiles/makeutils.mk

USE_RCS_MK = 1
include $(topsrcdir)/config/makefiles/rcs.mk

$(call requiredfunction,getargv)
$(call requiredfunction,getRcsType)
$(call errorIfEmpty,INCLUDED_RCS_MK)

saved := $(MOZ_RCS_TYPE)
undefine MOZ_RCS_TYPE # Clear to force value gathering
type = $(call getRcsType)
ifneq ($(type),$(saved))
  $(error getRcsType[$(type)] != MOZ_RCS_TYPE[$(saved)])
endif

# Limit testing to systems getSourceRepo() has been written for
ifneq (,$(MOZ_RCS_TYPE))
  $(call requiredfunction,getSourceRepo)

  repo := $(call getSourceRepo)
  $(if $(eq $(repo),$(NULL)),$(error getSourceRepo() failed))

  repo := $(call getSourceRepo,../..)
  $(if $(eq $(repo),$(NULL)),$(error getSourceRepo(../..) failed))
endif


compile-time-tests:
	@true
