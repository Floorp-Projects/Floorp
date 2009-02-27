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
# The Original Code is Mozilla CE Shunt Library.
#
# The Initial Developer of the Original Code is Mozilla Corporation.
# Portions created by the Initial Developer are Copyright (C) 2008
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#	John Wolfe (wolfe@lobo.us)
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

DEVENV_FLAG=-

CC=cl -O2

MOZCE_DEVENV=vs$(MOZ_MSVCVERSION)

MOZCE_SHUNT_SLN=$(TOPSRCDIR)/build/wince/shunt/build/$(MOZCE_DEVENV)/mozce_shunt_static.sln
MOZCE_PROJECT="Release|Windows Mobile 6 Professional SDK (ARMV4I)"
MOZCE_SHUNT_DLL=../shunt/build/$(MOZCE_DEVENV)/mozce_shunt.dll
MOZCE_TOOLS_DIR=$(TOPSRCDIR)/build/wince/tools
MOZCE_TOOLS_BIN_DIR=$(OBJDIR)/dist/sdk/bin

BUILD_SWITCH=$(DEVENV_FLAG)Build
REBUILD_SWITCH=$(DEVENV_FLAG)Rebuild
CLEAN_SWITCH=$(DEVENV_FLAG)clean

ifeq ($(VCINSTALLDIR),)
$(error Environment variable VCINSTALLDIR not set! Are you using MozillaBuild?)
endif

ifeq ($(WINCE_SDK_DIR),)
$(error Environment variable WINCE_SDK_DIR not set! It must be passed to make if not running from configure)
endif

ifeq ($(MOZ_MSVCVERSION),)
$(error Environment variable MOZ_MSVCVERSION not set! Are you using MozillaBuild?)
endif

CFLAGS += \
  -DVC_PATH='"$(subst \,\\,$(VCINSTALLDIR))\\"' \
  -DWM_SDK_PATH='"$(subst \,\\,$(WINCE_SDK_DIR))\\"' \
  -DMOZCE_DEVENV='"$(MOZCE_DEVENV)"' \
  -DTOPSRCDIR='"$(TOPSRCDIR)"' \
  $(NULL)

ifneq ($(WINDOWSSDKDIR),)
CFLAGS += -DWIN_SDK_PATH='"$(subst \,\\,$(WINDOWSSDKDIR))"'
else
ifeq ($(SDKDIR),)
$(error Environment variable WINDOWSSDKDIR not set! Are you using MozillaBuild?)
else
CFLAGS += -DWIN_SDK_PATH='"$(subst \,\\,$(SDKDIR))"'
endif
endif

ifdef VPATH
CFLAGS	+= -DSHUNT_INC='"$(OBJDIR)/dist/include/shunt"'
else
CFLAGS	+= -DSHUNT_INC='"$(TOPSRCDIR)/build/wince/shunt/include"'
endif
CFLAGS += -DEBUG -Zi

all: libs

libs:  output_some_env \
	 $(MOZCE_TOOLS_BIN_DIR)/arm-wince-as.exe \
	 $(MOZCE_TOOLS_BIN_DIR)/arm-wince-gcc.exe \
	 $(MOZCE_TOOLS_BIN_DIR)/arm-wince-lib.exe \
	 $(MOZCE_TOOLS_BIN_DIR)/arm-wince-link.exe \
	 $(MOZCE_TOOLS_BIN_DIR)/arm-wince-res.exe 
	devenv $(MOZCE_SHUNT_SLN) $(BUILD_SWITCH) $(MOZCE_PROJECT)

clean: clobber


clobber:
	rm $(MOZCE_TOOLS_BIN_DIR)/arm-wince-as.exe 
	rm $(MOZCE_TOOLS_BIN_DIR)/arm-wince-gcc.exe 
	rm $(MOZCE_TOOLS_BIN_DIR)/arm-wince-lib.exe 
	rm $(MOZCE_TOOLS_BIN_DIR)/arm-wince-link.exe 
	rm $(MOZCE_TOOLS_BIN_DIR)/arm-wince-res.exe 
	devenv $(MOZCE_SHUNT_SLN) $(CLEAN_SWITCH) $(MOZCE_PROJECT)

output_some_env:
	@echo FOUND $(MOZCE_DEVENV)
	@echo FOUND $(MOZCE_DEVENV): VSINSTALLDIR=$(VSINSTALLDIR) / MINGW32=$(MINGW32) / MSYSTEM=$(MSYSTEM)
	@echo FOUND $(MOZCE_DEVENV)

$(MOZCE_TOOLS_BIN_DIR)/arm-wince-as.exe:	$(MOZCE_TOOLS_DIR)/arm-wince-as.c $(MOZCE_TOOLS_DIR)/toolspath.h $(MOZCE_TOOLS_DIR)/Makefile
	mkdir -p $(MOZCE_TOOLS_BIN_DIR);
	$(CC) $(CFLAGS) -Fe$@ $(MOZCE_TOOLS_DIR)/arm-wince-as.c

$(MOZCE_TOOLS_BIN_DIR)/arm-wince-gcc.exe:	$(MOZCE_TOOLS_DIR)/arm-wince-gcc.c $(MOZCE_TOOLS_DIR)/toolspath.h $(MOZCE_TOOLS_DIR)/Makefile
	mkdir -p $(MOZCE_TOOLS_BIN_DIR);
	$(CC) $(CFLAGS) -Fe$@ $(MOZCE_TOOLS_DIR)/arm-wince-gcc.c

$(MOZCE_TOOLS_BIN_DIR)/arm-wince-lib.exe:	$(MOZCE_TOOLS_DIR)/arm-wince-lib.c $(MOZCE_TOOLS_DIR)/toolspath.h $(MOZCE_TOOLS_DIR)/Makefile
	mkdir -p $(MOZCE_TOOLS_BIN_DIR);
	$(CC) $(CFLAGS) -Fe$@ $(MOZCE_TOOLS_DIR)/arm-wince-lib.c

$(MOZCE_TOOLS_BIN_DIR)/arm-wince-link.exe:	$(MOZCE_TOOLS_DIR)/arm-wince-link.c $(MOZCE_TOOLS_DIR)/toolspath.h $(MOZCE_TOOLS_DIR)/Makefile
	mkdir -p $(MOZCE_TOOLS_BIN_DIR);
	$(CC) $(CFLAGS) -Fe$@ $(MOZCE_TOOLS_DIR)/arm-wince-link.c

$(MOZCE_TOOLS_BIN_DIR)/arm-wince-res.exe:	$(MOZCE_TOOLS_DIR)/arm-wince-res.c $(MOZCE_TOOLS_DIR)/toolspath.h $(MOZCE_TOOLS_DIR)/Makefile
	mkdir -p $(MOZCE_TOOLS_BIN_DIR);
	$(CC) $(CFLAGS) -Fe$@ $(MOZCE_TOOLS_DIR)/arm-wince-res.c

