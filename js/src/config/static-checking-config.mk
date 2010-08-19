DEHYDRA_SCRIPT = $(topsrcdir)/config/static-checking.js

DEHYDRA_MODULES = \
  $(NULL)

TREEHYDRA_MODULES = \
  $(topsrcdir)/jsstack.js \
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
