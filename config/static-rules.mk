
ifdef _NO_AUTO_VARS
_TARGET = $(srcdir)/$(@F)
else
_TARGET = $@
endif

nsStaticComponents.cpp: $(MOZILLA_DIR)/xpfe/bootstrap/nsStaticComponents.cpp.in Makefile Makefile.in $(FINAL_LINK_COMP_NAMES)
	rm -f $@
	cat $< | \
	sed -e "s|%DECL_NSGETMODULES%|$(foreach m,$(STATIC_COMPONENT_LIST),DECL_NSGETMODULE($(m)))|" | \
	sed -e "s|%MODULE_LIST%|$(foreach m, $(STATIC_COMPONENT_LIST),MODULE($(m)),)|" \
	> $(_TARGET)

ifeq ($(OS_ARCH),IRIX)
LDFLAGS	+= -Wl,-LD_LAYOUT:lgot_buffer=80
endif

