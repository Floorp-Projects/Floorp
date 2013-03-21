# -*- makefile -*-
# vim:set ts=8 sw=8 sts=8 noet:
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

ifndef INCLUDED_JAVA_BUILD_MK #{

ifdef RES_FILES #{
res-dep := .deps-copy-java-res

GENERATED_DIRS += res
GARBAGE        += $(res-dep)

export:: $(res-dep)

res-dep-preqs := \
  $(addprefix $(srcdir)/,$(RES_FILES)) \
  $(call mkdir_deps,res) \
  $(if $(IS_LANGUAGE_REPACK),FORCE) \
  $(NULL)

# nop-build: only copy res/ files when needed
$(res-dep): $(res-dep-preqs)
	$(call copy_dir,$(srcdir)/res,$(CURDIR)/res)
	@$(TOUCH) $@
endif #}


ifdef JAVAFILES #{
GENERATED_DIRS += classes

export:: classes
classes: $(call mkdir_deps,classes)
endif #}

INCLUDED_JAVA_BUILD_MK := 1

endif #} INCLUDED_JAVA_BUILD_MK
