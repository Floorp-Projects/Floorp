
#need a fullpath here, otherwise uic complains about
#already defined symbols
FULLDIST = `(cd $(DIST); pwd)`
DIST_QTDESIGNERPLUGINS = $(FULLDIST)/lib/designer

ifneq (,$(filter Linux FreeBSD SunOS,$(OS_ARCH)))
UIC = $(QTDIR)/bin/uic -L $(DIST_QTDESIGNERPLUGINS)
endif
ifeq ($(OS_ARCH), WINNT)
UIC = $(CYGWIN_WRAPPER) $(QTDIR)/bin/uic$(BIN_SUFFIX) -L $(DIST_QTDESIGNERPLUGINS)
endif


UI_HSRCS = $(UICSRCS:%.ui=ui_%.h)
UI_CPPSRCS = $(UICSRCS:%.ui=ui_%.cpp)
MOCUI_CPPSRCS = $(UI_CPPSRCS:%=moc_%)
ifdef IMAGES
IMGUI_CPPSRCS = uiimg_collection.cpp
endif
CPPSRCS += $(UI_CPPSRCS) $(MOCUI_CPPSRCS) $(IMGUI_CPPSRCS)
GARBAGE += $(UI_HSRCS) $(UI_CPPSRCS) $(MOCUI_CPPSRCS) $(IMGUI_CPPSRCS)

