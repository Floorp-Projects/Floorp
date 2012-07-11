# -*- makefile -*-
# vim:set ts=8 sw=8 sts=8 noet:
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
#

# Always declared, general use by:
# js/xpconnect/tests/idl/Makefile.in:libs
# toolkit/crashreporter/test/Makefile.in
XPIDL_GEN_DIR ?= _xpidlgen
GARBAGE_DIRS  += $(XPIDL_GEN_DIR)


###########################################################################
## Conditional logic
###########################################################################
ifndef INCLUDED_XPIDL_MK #{
  INCLUDED_XPIDL_MK = 1

  ifneq (,$(XPIDLSRCS)) #{

    ifndef NO_DIST_INSTALL #{
      _xpidl-todo_ += xpidl-install-src
      _xpidl-todo_ += xpidl-install-headers
    endif #}

  endif #} XPIDLSRCS

  export:: $(_xpidl-todo_)

  $(call requiredfunction,mkdir_deps)
endif #} INCLUDED_XPIDL_MK


###########################################################################
## processing targets
###########################################################################
ifdef _xpidl-todo_ #{

## Logic batch #1
xpidl-install-src-preqs=\
  $(XPIDLSRCS) \
  $(call mkdir_deps,$(IDL_DIR)) \
  $(NULL)

xpidl-install-src: $(xpidl-install-src-preqs)
	$(call install_cmd,$(IFLAGS1) $(foreach val,$^,$(call mkdir_stem,$(val))))

xpidl-install-headers-preqs =\
  $(patsubst %.idl,$(XPIDL_GEN_DIR)/%.h, $(XPIDLSRCS)) \
  $(call mkdir_deps,$(DIST)/include) \
  $(NULL)
xpidl-install-headers: $(xpidl-install-headers-preqs)
	$(call install_cmd,$(IFLAGS1) $(foreach val,$^,$(call mkdir_stem,$(val))))

endif #} _xpidl-todo_
