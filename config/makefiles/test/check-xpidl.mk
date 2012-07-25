# -*- makefile -*-
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

ifdef VERBOSE
  $(warning loading test)
endif

USE_AUTOTARGETS_MK = 1
include $(topsrcdir)/config/makefiles/makeutils.mk

basedir  = blah
DIST     = $(basedir)/dist
DI       = $(DIST)/include
IDL_DIR  = $(basedir)/idl
INSTALL := cp

XPIDLSRCS = $(srcdir)/check-xpidl.mk

# Avoid permutations induced by 'include {config,kitchen-sink}.mk'
install_cmd ?= $(INSTALL) $(1)
include $(topsrcdir)/config/makefiles/xpidl.mk


$(call requiredfunction,topsrcdir)
$(call requiredfunction,XPIDL_GEN_DIR)

HIDE=@
check-xpidl: xpidl-install-src xpidl-install-headers
	$(HIDE)test -d $(DIST)                   || exit 90
	$(HIDE)test -f $(DI)/check-xpidl.mk      || exit 91
	$(HIDE)test -f $(IDL_DIR)/check-xpidl.mk || exit 92

# Declare targets to avoid including rules.mk
$(DI) $(IDL_DIR):
	mkdir -p $@

clean-xpidl:
	$(RM) -r $(basedir)
