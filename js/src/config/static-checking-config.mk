DEHYDRA_SCRIPT = $(topsrcdir)/config/static-checking.js

DEHYDRA_MODULES = \
  $(NULL)

TREEHYDRA_MODULES = \
  $(topsrcdir)/jsstack.js \
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
