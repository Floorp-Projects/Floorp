# -*- makefile -*-
# vim:set ts=8 sw=8 sts=8 noet:
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
#

ifdef USE_RCS_MK #{

ifndef INCLUDED_RCS_MK #{

MOZ_RCS_TYPE ?= $(notdir $(wildcard $(topsrcdir)/.hg))


###########################################################################
# HAVE_MERCURIAL_RCS
###########################################################################
ifeq (.hg,$(MOZ_RCS_TYPE)) #{

# Intent: Retrieve the http:// repository path for a directory.
# Usage: $(call getSourceRepo[,repo_dir|args])
# Args:
#   path (optional): repository to query.  Defaults to $(topsrcdir)
getSourceRepo = \
  $(call FUNC_getSourceRepo,$(if $(1),cd $(1) && hg,hg --repository $(topsrcdir)))

# return: http://hg.mozilla.org/mozilla-central
FUNC_getSourceRepo = \
  $(strip \
    $(patsubst %/,%,\
    $(patsubst ssh://%,http://%,\
    $(firstword $(shell $(getargv) showconfig paths.default))\
    )))

endif #} HAVE_MERCURIAL_RCS


INCLUDED_RCS_MK := 1
endif #}

endif #}
