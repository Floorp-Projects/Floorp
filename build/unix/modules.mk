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
BUILD_MODULE_CVS = config build include

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

# BM_DIRS_mod: list of directories to traverse from toplevel Makefile 
# BM_DEP_DIRS_mod: list of directories to run "export" over
# BM_CVS_mod: list of directories to check out of cvs recursively
# BM_CVS_NS_mod: list of directories to check out of cvs non-recursively

ifneq ($(BUILD_MODULES),all)

#
# dbm
#
BM_DIRS_dbm	= $(NSPRPUB_DIR) dbm
BM_CVS_dbm	= $(BM_DIRS_dbm)

#
# js
#
BM_DIRS_js	= $(NSPRPUB_DIR) js/src
BM_CVS_js	= $(NSPRPUB_DIR)
BM_CVS_NS_js 	= js js/src js/src/fdlibm

#
# xpcom
#
BM_DIRS_xpcom		= $(NSPRPUB_DIR) modules/libreg xpcom
BM_DEP_DIRS_xpcom	= intl/unicharutil/public intl/uconv/public modules/libjar
BM_CVS_NS_xpcom		= xpcom xpcom/typelib xpcom/typelib/xpidl intl/unicharutil/public intl/uconv/public
BM_CVS_xpcom		= modules/libreg xpcom/typelib/xpt xpcom/base xpcom/ds xpcom/io xpcom/components xpcom/threads xpcom/reflect xpcom/proxy xpcom/build xpcom/tools xpcom/sample modules/libjar

#
# xpconnect
#
BM_DIRS_xpconnect	= $(BM_DIRS_xpcom) $(BM_DIRS_js) js/src/xpconnect
BM_CVS_xpconnect	= $(BM_CVS_xpcom) $(BM_CVS_js) js/src/xpconnect
BM_CVS_NS_xpconnect	= $(BM_CVS_NS_xpcom) $(BM_CVS_NS_js) js/src/xpconnect

#
# necko
#
BM_DIRS_necko		= $(BM_DIRS_xpcom) $(BM_DIRS_dbm) netwerk
BM_DEP_DIRS_necko	= $(BM_DEP_DIRS_xpcom) $(BM_DEP_DIRS_dbm) uriloader/exthandler intl/locale/idl intl/strres/public js/src modules/libpref/public
BM_CVS_necko		= $(BM_CVS_xpcom) $(BM_CVS_dbm) netwerk uriloader/exthandler
BM_CVS_NS_necko		= $(BM_CVS_NS_xpcom) intl/locale/idl intl/strres/public modules/libpref/public $(BM_CVS_NS_js)

#
# tranformiix
#
BM_DIRS_transformiix	= extensions/transformiix
BM_CVS_transformiix     = extensions/transformiix

#
# psm
#
BM_DIRS_psm	= $(BM_DIRS_dbm) $(BM_DIRS_xpcom) security netwerk/base/public netwerk/socket/base dom/public $(BM_DIRS_js) extensions/psm-glue
BM_DEP_DIRS_psm	= $(BM_DEP_DIRS_dbm) $(BM_DEP_DIRS_xpcom) $(BM_DEP_DIRS_js) intl/locale/idl intl/locale/public intl/strres/public uriloader/base modules/libpref/public profile/public caps/idl netwerk/protocol/http/public gfx/idl gfx/public rdf/base/idl xpfe/appshell/public widget/public docshell/base layout/html/forms/public layout/base/public rdf/content/public dom/src/base modules/oji/public caps/include
BM_CVS_psm	= $(BM_CVS_dbm) $(BM_CVS_xpcom) $(BM_CVS_js) security netwerk/base/public netwerk/socket/base dom/public $(BM_CVS_js) extensions/psm-glue
BM_CVS_NS_psm	= $(BM_CVS_NS_dbm) $(BM_CVS_NS_xpcom) $(BM_CVS_NS_js) intl/locale/idl intl/locale/public intl/strres/public uriloader/base modules/libpref/public profile/public caps/idl netwerk/protocol/http/public gfx/idl gfx/public rdf/base/idl xpfe/appshell/public widget/public docshell/base layout/html/forms/public layout/base/public rdf/content/public dom/src/base modules/oji/public caps/include


#
# Tally
#

BUILD_MODULE_DIRS     += $(foreach mod,$(BUILD_MODULES), $(BM_DIRS_$(mod)))
BUILD_MODULE_DEP_DIRS = $(foreach mod,$(BUILD_MODULES), $(BM_DEP_DIRS_$(mod)))
BUILD_MODULE_CVS      += $(foreach mod,$(BUILD_MODULES), $(BM_CVS_$(mod)))
BUILD_MODULE_CVS_NS   = $(foreach mod,$(BUILD_MODULES), $(BM_CVS_NS_$(mod)))

# Remove dups from the list to speed up the build
#
ifdef PERL

BUILD_MODULE_DIRS := $(shell $(PERL) $(topsrcdir)/build/unix/uniq.pl $(BUILD_MODULE_DIRS))
BUILD_MODULE_DEP_DIRS := $(shell $(PERL) $(topsrcdir)/build/unix/uniq.pl $(BUILD_MODULE_DEP_DIRS))

else
# Since PERL isn't defined, client.mk must've called us so order doesn't matter
BUILD_MODULE_DIRS := $(sort $(BUILD_MODULE_DIRS))
BUILD_MODULE_DEP_DIRS := $(sort $(BUILD_MODULE_DEP_DIRS))
BUILD_MODULE_CVS := $(sort $(BUILD_MODULE_CVS))
BUILD_MODULE_CVS_NS := $(sort $(BUILD_MODULE_CVS_NS))
endif

endif # BUILD_MODULES
