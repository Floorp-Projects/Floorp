#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 
#

# Static components makefile
# 	Include this makefile after config/config.mk & before config/rules.mk
#	This makefile will provide the defines for statically linking
#	all of the components into the binary.

STATIC_CPPSRCS	+= nsStaticComponents.cpp
STATIC_DEFINES	+= -D_BUILD_STATIC_BIN=1
STATIC_REQUIRES += \
	xpcom \
	string \
	$(NULL)

ifeq (,$(filter-out OS2 WINNT,$(OS_ARCH)))
STATIC_EXTRA_LIBS += \
	$(addsuffix .$(LIB_SUFFIX),$(addprefix $(DIST)/lib/components/$(LIB_PREFIX),$(shell cat $(FINAL_LINK_COMPS)))) \
	$(addsuffix .$(LIB_SUFFIX),$(addprefix $(DIST)/lib/$(LIB_PREFIX),$(shell cat $(FINAL_LINK_LIBS)))) \
	$(NULL)
else
STATIC_EXTRA_LIBS += -L$(DIST)/lib/components
STATIC_EXTRA_DSO_LIBS += $(shell cat $(FINAL_LINK_COMPS) $(FINAL_LINK_LIBS))
endif # OS2 || WINNT

STATIC_COMPONENT_LIST := $(shell cat $(FINAL_LINK_COMP_NAMES))

STATIC_EXTRA_DEPS	+= $(FINAL_LINK_COMPS) $(FINAL_LINK_LIBS) $(addsuffix .$(LIB_SUFFIX),$(addprefix $(DIST)/lib/components/$(LIB_PREFIX),$(shell cat $(FINAL_LINK_COMPS)))) $(addsuffix .$(LIB_SUFFIX),$(addprefix $(DIST)/lib/$(LIB_PREFIX),$(shell cat $(FINAL_LINK_LIBS))))

STATIC_EXTRA_DEPS	+= \
	$(topsrcdir)/config/static-config.mk \
	$(topsrcdir)/config/static-rules.mk \
	$(NULL)

ifdef MOZ_PSM
STATIC_EXTRA_DEPS	+= $(NSS_DEP_LIBS)
endif

STATIC_EXTRA_LIBS	+= \
		$(PNG_LIBS) \
		$(MNG_LIBS) \
		$(JPEG_LIBS) \
		$(ZLIB_LIBS) \
		$(MOZ_GDK_PIXBUF_LIBS) \
		$(MOZ_XIE_LIBS) \
		$(MOZ_XPRINT_LDFLAGS) \
		$(NULL)

ifdef MOZ_PSM
STATIC_EXTRA_LIBS	+= \
		$(NSS_LIBS) \
		$(NULL)
endif

ifdef MOZ_LDAP_XPCOM
STATIC_EXTRA_LIBS	+= \
		$(LDAP_LIBS) \
		$(NULL)
endif

ifdef MOZ_SVG
STATIC_EXTRA_LIBS	+= $(MOZ_LIBART_LIBS)
endif

ifneq  (,$(MOZ_ENABLE_GTK)$(MOZ_ENABLE_XLIB))
STATIC_EXTRA_LIBS	+= $(XLDFLAGS) $(XT_LIBS)
endif

ifdef MOZ_ENABLE_XINERAMA
STATIC_EXTRA_LIBS	+= $(MOZ_XINERAMA_LIBS)
endif

ifdef MOZ_CALENDAR
STATIC_EXTRA_LIBS	+= -lical -licalss
endif

# Component Makefile always brings in this.
# STATIC_EXTRA_LIBS	+= $(TK_LIBS)

ifeq ($(OS_ARCH),WINNT)
STATIC_EXTRA_LIBS += comctl32.lib comdlg32.lib uuid.lib shell32.lib ole32.lib oleaut32.lib version.lib winspool.lib
endif


