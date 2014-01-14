# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# The entire tree should be subject to static analysis using the XPCOM
# script. Additional scripts may be added by specific subdirectories.

DEHYDRA_SCRIPT = $(topsrcdir)/config/static-checking.js

ifndef BUILDING_JS
DEHYDRA_MODULES = \
  $(topsrcdir)/xpcom/analysis/final.js \
  $(topsrcdir)/xpcom/analysis/must-override.js \
  $(NULL)

TREEHYDRA_MODULES = \
  $(topsrcdir)/xpcom/analysis/outparams.js \
  $(topsrcdir)/xpcom/analysis/stack.js \
  $(topsrcdir)/xpcom/analysis/flow.js \
  $(topsrcdir)/xpcom/analysis/static-init.js \
  $(topsrcdir)/layout/generic/frame-verify.js \
  $(NULL)
endif

TREEHYDRA_MODULES += \
  $(topsrcdir)/js/src/jsstack.js \
  $(NULL)

DEHYDRA_ARG_PREFIX=-fplugin-arg-gcc_treehydra-

DEHYDRA_ARGS = \
  $(DEHYDRA_ARG_PREFIX)script=$(DEHYDRA_SCRIPT) \
  $(DEHYDRA_ARG_PREFIX)topsrcdir=$(topsrcdir) \
  $(DEHYDRA_ARG_PREFIX)objdir=$(DEPTH) \
  $(DEHYDRA_ARG_PREFIX)dehydra-modules=$(subst $(NULL) ,$(COMMA),$(strip $(DEHYDRA_MODULES))) \
  $(DEHYDRA_ARG_PREFIX)treehydra-modules=$(subst $(NULL) ,$(COMMA),$(strip $(TREEHYDRA_MODULES))) \
  $(NULL)

DEHYDRA_FLAGS = -fplugin=$(DEHYDRA_PATH) $(DEHYDRA_ARGS)

ifdef DEHYDRA_PATH
OS_CXXFLAGS += $(DEHYDRA_FLAGS)
endif

ifdef ENABLE_CLANG_PLUGIN
ifndef BUILDING_JS
CLANG_PLUGIN := $(DEPTH)/build/clang-plugin/$(DLL_PREFIX)clang-plugin$(DLL_SUFFIX)
else
CLANG_PLUGIN := $(DEPTH)/../../build/clang-plugin/$(DLL_PREFIX)clang-plugin$(DLL_SUFFIX)
endif
OS_CXXFLAGS += -Xclang -load -Xclang $(CLANG_PLUGIN) -Xclang -add-plugin -Xclang moz-check
OS_CFLAGS += -Xclang -load -Xclang $(CLANG_PLUGIN) -Xclang -add-plugin -Xclang moz-check
endif
