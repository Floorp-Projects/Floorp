# The entire tree should be subject to static analysis using the XPCOM
# script. Additional scripts may be added by specific subdirectories.

DEHYDRA_SCRIPT = $(topsrcdir)/xpcom/analysis/static-checking.js

DEHYDRA_MODULES = \
  $(topsrcdir)/xpcom/analysis/final.js \
  $(NULL)

TREEHYDRA_MODULES = \
  $(topsrcdir)/xpcom/analysis/outparams.js \
  $(topsrcdir)/xpcom/analysis/stack.js \
  $(topsrcdir)/xpcom/analysis/flow.js \
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
