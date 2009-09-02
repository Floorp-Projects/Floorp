#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

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

STATIC_EXTRA_LIBS += \
	$(addsuffix .$(LIB_SUFFIX),$(addprefix $(DEPTH)/staticlib/components/$(LIB_PREFIX),$(shell cat $(FINAL_LINK_COMPS)))) \
	$(addsuffix .$(LIB_SUFFIX),$(addprefix $(DEPTH)/staticlib/$(LIB_PREFIX),$(shell cat $(FINAL_LINK_LIBS)))) \
	$(NULL)

STATIC_COMPONENT_LIST = $(shell cat $(FINAL_LINK_COMP_NAMES))

STATIC_EXTRA_DEPS	+= $(FINAL_LINK_COMPS) $(FINAL_LINK_LIBS) $(addsuffix .$(LIB_SUFFIX),$(addprefix $(DEPTH)/staticlib/components/$(LIB_PREFIX),$(shell cat $(FINAL_LINK_COMPS)))) $(addsuffix .$(LIB_SUFFIX),$(addprefix $(DEPTH)/staticlib/$(LIB_PREFIX),$(shell cat $(FINAL_LINK_LIBS))))

STATIC_EXTRA_DEPS	+= \
	$(topsrcdir)/config/static-config.mk \
	$(topsrcdir)/config/static-rules.mk \
	$(NULL)

ifdef MOZ_PSM
STATIC_EXTRA_DEPS	+= $(NSS_DEP_LIBS)
endif

STATIC_EXTRA_LIBS	+= \
		$(PNG_LIBS) \
		$(JPEG_LIBS) \
		$(ZLIB_LIBS) \
		$(NULL)

ifdef MOZ_PSM
STATIC_EXTRA_LIBS	+= \
		$(NSS_LIBS) \
		$(NULL)
endif

STATIC_EXTRA_LIBS	+= $(MOZ_CAIRO_LIBS)

STATIC_EXTRA_LIBS	+= $(QCMS_LIBS)

ifdef MOZ_ENABLE_GTK2
STATIC_EXTRA_LIBS	+= $(XLDFLAGS) $(XT_LIBS) -lgthread-2.0
STATIC_EXTRA_LIBS	+= $(MOZ_PANGO_LIBS)
endif

ifdef MOZ_STORAGE
STATIC_EXTRA_LIBS	+= $(SQLITE_LIBS)
endif

ifdef MOZ_ENABLE_STARTUP_NOTIFICATION
STATIC_EXTRA_LIBS	+= $(MOZ_STARTUP_NOTIFICATION_LIBS)
endif

ifdef MOZ_SYDNEYAUDIO
ifeq ($(OS_ARCH),Linux)
STATIC_EXTRA_LIBS += $(MOZ_ALSA_LIBS)
endif
endif

# Component Makefile always brings in this.
# STATIC_EXTRA_LIBS	+= $(TK_LIBS)

ifeq ($(OS_ARCH),WINNT)
STATIC_EXTRA_LIBS += $(call EXPAND_LIBNAME,comctl32 comdlg32 uuid shell32 ole32 oleaut32 version winspool imm32)
# XXX temporary workaround until link ordering issue is solved
ifdef GNU_CC
STATIC_EXTRA_LIBS += $(call EXPAND_LIBNAME,winmm wsock32 gdi32)
endif
STATIC_EXTRA_LIBS += $(call EXPAND_LIBNAME, usp10)
endif

ifeq ($(OS_ARCH),AIX)
STATIC_EXTRA_LIBS += $(call EXPAND_LIBNAME,odm cfg)
endif

LOCAL_INCLUDES += -I$(topsrcdir)/config
