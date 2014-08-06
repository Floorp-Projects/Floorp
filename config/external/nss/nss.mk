# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

include $(DEPTH)/config/autoconf.mk

include $(topsrcdir)/config/config.mk

dirs :=

define add_dirs
SHARED_LIBRARY_DIRS :=
include $(topsrcdir)/security/$(1)/config.mk
dirs += $$(addprefix $(1)/,$$(SHARED_LIBRARY_DIRS)) $(1)
endef
$(foreach dir,util nss ssl smime,$(eval $(call add_dirs,nss/lib/$(dir))))

libs :=
define add_lib
LIBRARY_NAME :=
include $(topsrcdir)/security/$(1)/manifest.mn
libs += $$(addprefix $(1)/,$(LIB_PREFIX)$$(LIBRARY_NAME).$(LIB_SUFFIX))
endef
$(foreach dir,$(dirs),$(eval $(call add_lib,$(dir))))

echo-variable-%:
	@echo $($*)
