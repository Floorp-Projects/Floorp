# The entire tree should be subject to static analysis using the XPCOM
# script. Additional scripts may be added by specific subdirectories.

DEHYDRA_SCRIPT = $(topsrcdir)/config/static-checking.js

DEHYDRA_MODULES = \
  $(topsrcdir)/xpcom/analysis/final.js \
  $(topsrcdir)/xpcom/analysis/override.js \
  $(topsrcdir)/xpcom/analysis/must-override.js \
  $(NULL)

TREEHYDRA_MODULES = \
  $(topsrcdir)/xpcom/analysis/outparams.js \
  $(topsrcdir)/xpcom/analysis/stack.js \
  $(topsrcdir)/xpcom/analysis/flow.js \
  $(topsrcdir)/js/src/jsstack.js \
  $(topsrcdir)/layout/generic/frame-verify.js \
  $(NULL)

DEHYDRA_ARGS = \
  --topsrcdir=$(topsrcdir) \
  --objdir=$(DEPTH) \
  --dehydra-modules=$(subst $(NULL) ,$(COMMA),$(strip $(DEHYDRA_MODULES))) \
  --treehydra-modules=$(subst $(NULL) ,$(COMMA),$(strip $(TREEHYDRA_MODULES))) \
  $(NULL)

DEHYDRA_FLAGS = -fplugin=$(DEHYDRA_PATH) -fplugin-arg='$(DEHYDRA_SCRIPT) $(DEHYDRA_ARGS)'

ifdef DEHYDRA_PATH
OS_CXXFLAGS += $(DEHYDRA_FLAGS)
endif
