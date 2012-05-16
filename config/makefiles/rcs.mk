# -*- makefile -*-
# vim:set ts=8 sw=8 sts=8 noet:
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
#

###########################################################################
## Accessor functions:
##   $(call getRcsType): .git, .hg
##   $(call getSourceRepo) == http://hg.mozilla.org/mozilla-central
###########################################################################

ifdef USE_RCS_MK #{
ifndef INCLUDED_RCS_MK #{

# Determine revision control system in use: .hg, .git, etc
# Call getRcsType() at least once, $(RCS_TYPE) will be defined
#
define FUNC_rcstype =
  MOZ_RCS_TYPE := $(notdir $(firstword \
    $(wildcard \
      $(or $(1),$(topsrcdir))/.hg\
      $(or $(1),$(topsrcdir))/.git\
    ))
  )
endef
getRcsType = $(or $(MOZ_RCS_TYPE),$(eval $(call FUNC_rcstype))$(MOZ_RCS_TYPE))
MOZ_RCS_TYPE := $(call getRcsType)

###########################################################################
# HAVE_MERCURIAL_RCS (todo: include rcs.$(MOZ_RCS_TYPE).mk git, hg)
###########################################################################
ifneq (,$(findstring .hg,$(MOZ_RCS_TYPE)))

# Intent: Retrieve the http:// repository path for a directory.
# Usage: $(call getSourceRepo[,repo_dir|args])
# Args:
#   path (optional): repository to query.  Defaults to $(topsrcdir)
getSourceRepo =$(call FUNC_getSourceRepo,$(if $(1),cd $(1) && hg,hg --repository $(topsrcdir)))

# http://hg.mozilla.org/mozilla-central
FUNC_getSourceRepo=\
  $(patsubst %/,%,\
  $(patsubst ssh://%,http://%,\
  $(strip \
    $(firstword $(shell $(getargv) showconfig paths.default))\
    )))

endif #} HAVE_MERCURIAL_RCS

###########################################################################
# HAVE_GIT_RCS
###########################################################################
ifneq (,$(findstring .git,$(MOZ_RCS_TYPE)))
  # git://github.com/mozilla/...
  getSourceRepo = $(firstword $(shell $(getargv) git config --get remote.origin.url))
endif #} HAVE_GIT_RCS

INCLUDED_RCS_MK=1
endif #}

endif #}
