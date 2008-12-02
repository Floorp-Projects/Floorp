# Currently spidermonkey has no static checking infrastructure, but it will...
# This is all dummy values now: see the Mozilla version of this file for
# an example with real data.

DEHYDRA_SCRIPT = $(error No Spidermonkey static-checking.js yet!)

DEHYDRA_MODULES = \
  $(NULL)

TREEHYDRA_MODULES = \
  $(NULL)

DEHYDRA_ARGS = \
  --topsrcdir=$(topsrcdir) \
  --objdir=$(DEPTH) \
  --dehydra-modules=$(subst $(NULL) ,$(COMMA),$(strip $(DEHYDRA_MODULES))) \
  --treehydra-modules=$(subst $(NULL) ,$(COMMA),$(strip $(TREEHYDRA_MODULES))) \
  $(NULL)

DEHYDRA_FLAGS = -fplugin=$(DEHYDRA_PATH) -fplugin-arg='$(DEHYDRA_SCRIPT) $(DEHYDRA_ARGS)'

# ifdef DEHYDRA_PATH
# OS_CXXFLAGS += $(DEHYDRA_FLAGS)
# endif
