#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is build/unix/modules.mk from the Mozilla source tree.
#
# The Initial Developer of the Original Code is Christopher
# Seawood. Portions created by Christopher Seawood are
# Copyright (C) 2000 Christopher Seawood. All Rights Reserved.
#
#

BUILD_MODULE_DIRS := config build
_BUILD_MODS = 
NSPRPUB_DIR =

ifndef MOZ_NATIVE_NSPR
# Do not regenerate Makefile for NSPR
ifdef USE_NSPR_AUTOCONF
NSPRPUB_DIR		= nsprpub
STATIC_MAKEFILES	:= nsprpub
else
NSPRPUB_DIR		= $(topsrcdir)/nsprpub
STATIC_MAKEFILES	:= $(topsrcdir)/nsprpub
endif
endif

ifneq ($(BUILD_MODULES),all)

ifdef CROSS_COMPILE
BUILD_MODULE_DIRS_js		= $(NSPRPUB_DIR)
endif

BUILD_MODULE_DIRS_dbm 		= $(NSPRPUB_DIR) dbm
BUILD_MODULE_DIRS_js		+= js
BUILD_MODULE_DIRS_xpcom		= $(NSPRPUB_DIR) modules/libreg xpcom intl/unicharutil/public intl/uconv/public
BUILD_MODULE_DIRS_necko		= $(BUILD_MODULE_DIRS_xpcom) netwerk


BUILD_MODULE_DIRS += $(foreach mod,$(BUILD_MODULES), $(BUILD_MODULE_DIRS_$(mod)))

# Remove dups from the list to speed up the build
#
BUILD_MODULE_DIRS := $(shell $(PERL) -e 'undef @out; \
    foreach $$d (@ARGV) { \
	push @out, $$d if (!grep(/$$d/, @out)); \
    }; \
    print "@out\n"; ' $(BUILD_MODULE_DIRS))

endif # BUILD_MODULES


