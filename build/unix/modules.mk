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

BUILD_MODULE_DIRS := config build include
BUILD_MODULE_DEP_DIRS	:=
_BUILD_MODS = 
NSPRPUB_DIR =

# client.mk does not have topsrcdir set
ifndef topsrcdir
topsrcdir=$(TOPSRCDIR)
endif

ifndef MOZ_NATIVE_NSPR
# Do not regenerate Makefile for NSPR
ifdef USE_NSPR_AUTOCONF
NSPRPUB_DIR		= nsprpub
else
NSPRPUB_DIR		= $(topsrcdir)/nsprpub
endif
endif

ifneq ($(BUILD_MODULES),all)

BUILD_MODULE_DIRS_dbm 		= $(NSPRPUB_DIR) dbm
BUILD_MODULE_DIRS_js		= $(NSPRPUB_DIR) js/src
BUILD_MODULE_DIRS_necko		= $(BUILD_MODULE_DIRS_xpcom) netwerk
BUILD_MODULE_DIRS_transformiix	= extensions/transformiix

BUILD_MODULE_DIRS_xpconnect	= $(BUILD_MODULE_DIRS_xpcom) $(BUILD_MODULE_DIRS_js) js/src/xpconnect
BUILD_MODULE_DIRS_security	= $(BUILD_MODULE_DIRS_xpcom) $(BUILD_MODULE_DIRS_dbm) security

BUILD_MODULE_DIRS_xpcom		= $(NSPRPUB_DIR) modules/libreg xpcom
BUILD_MODULE_DEP_DIRS_xpcom	= intl modules/libjar

BUILD_MODULE_DIRS_psm		= $(BUILD_MODULE_DIRS_dbm) $(BUILD_MODULE_DIRS_xpcom) security netwerk/base/public netwerk/socket/base dom/public $(BUILD_MODULE_DIRS_js) extensions/psm-glue
BUILD_MODULE_DEP_DIRS_psm	= $(BUILD_MODULE_DEP_DIRS_xpcom) uriloader/base modules/libpref/public profile/public caps/idl modules/appfilelocprovider/public netwerk/protocol/http/public js/src gfx/idl gfx/public rdf/base/idl xpfe/appshell/public widget/public docshell/base layout/html/forms/public layout/base/public rdf/content/public dom/src/base modules/oji/public caps/include

BUILD_MODULE_DIRS += $(foreach mod,$(BUILD_MODULES), $(BUILD_MODULE_DIRS_$(mod)))
BUILD_MODULE_DEP_DIRS += $(foreach mod,$(BUILD_MODULES), $(BUILD_MODULE_DEP_DIRS_$(mod)))

# Remove dups from the list to speed up the build
#
ifdef PERL
BUILD_MODULE_DIRS := $(shell $(PERL) -e 'undef @out; \
    foreach $$d (@ARGV) { \
	push @out, $$d if (!grep(/$$d/, @out)); \
    }; \
    print "@out\n"; ' $(BUILD_MODULE_DIRS))

BUILD_MODULE_DEP_DIRS := $(shell $(PERL) -e 'undef @out; \
    foreach $$d (@ARGV) { \
	push @out, $$d if (!grep(/$$d/, @out)); \
    }; \
    print "@out\n"; ' $(BUILD_MODULE_DEP_DIRS))
else
# Since PERL isn't defined, client.mk must've called us so order doesn't matter
BUILD_MODULE_DIRS := $(sort $(BUILD_MODULE_DIRS))
BUILD_MODULE_DEP_DIRS := $(sort $(BUILD_MODULE_DEP_DIRS))
endif

endif # BUILD_MODULES
