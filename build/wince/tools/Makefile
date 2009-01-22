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

MOZCE_SHUNT_SLN=../shunt/build/$(MOZCE_DEVENV)/mozce_shunt_static.sln
MOZCE_PROJECT="Release|Windows Mobile 6 Professional SDK (ARMV4I)"
MOZCE_SHUNT_DLL=../shunt/build/$(MOZCE_DEVENV)/mozce_shunt.dll
MOZCE_TOOLS_DIR=$(MOZCE_DEVENV)ppc2003arm

BUILD_SWITCH=$(DEVENV_FLAG)Build
REBUILD_SWITCH=$(DEVENV_FLAG)Rebuild
CLEAN_SWITCH=$(DEVENV_FLAG)clean


all: output_some_env \
	 $(MOZCE_TOOLS_DIR)/arm-wince-as.exe \
	 $(MOZCE_TOOLS_DIR)/arm-wince-gcc.exe \
	 $(MOZCE_TOOLS_DIR)/arm-wince-lib.exe \
	 $(MOZCE_TOOLS_DIR)/arm-wince-link.exe \
	 $(MOZCE_TOOLS_DIR)/arm-wince-res.exe 
	devenv $(MOZCE_SHUNT_SLN) $(BUILD_SWITCH) $(MOZCE_PROJECT)

clobber:
	rm -f $(MOZCE_TOOLS_DIR)/*.exe
	rm -f $(MOZCE_TOOLS_DIR)/*.obj
	rm -f *.obj
	rm -f *.exe
	rm -rf bin
	devenv $(MOZCE_SHUNT_SLN) $(CLEAN_SWITCH) $(MOZCE_PROJECT)

output_some_env:
	@echo FOUND $(MOZCE_DEVENV)
	@echo FOUND $(MOZCE_DEVENV): VSINSTALLDIR=$(VSINSTALLDIR) / MINGW32=$(MINGW32) / MSYSTEM=$(MSYSTEM)
	@echo FOUND $(MOZCE_DEVENV)

$(MOZCE_TOOLS_DIR)/arm-wince-as.exe:	$(MOZCE_TOOLS_DIR)/arm-wince-as.c $(MOZCE_TOOLS_DIR)/toolspath.h
	$(CC) $(MOZCE_TOOLS_DIR)/arm-wince-as.c
	mkdir -p bin;
	cp arm-wince-as.exe bin; mv arm-wince-as.exe $(MOZCE_TOOLS_DIR); rm *.obj

$(MOZCE_TOOLS_DIR)/arm-wince-gcc.exe:	$(MOZCE_TOOLS_DIR)/arm-wince-gcc.c $(MOZCE_TOOLS_DIR)/toolspath.h
	$(CC) $(MOZCE_TOOLS_DIR)/arm-wince-gcc.c
	mkdir -p bin;
	cp arm-wince-gcc.exe bin; mv arm-wince-gcc.exe $(MOZCE_TOOLS_DIR); rm *.obj

$(MOZCE_TOOLS_DIR)/arm-wince-lib.exe:	$(MOZCE_TOOLS_DIR)/arm-wince-lib.c $(MOZCE_TOOLS_DIR)/toolspath.h
	$(CC) $(MOZCE_TOOLS_DIR)/arm-wince-lib.c
	mkdir -p bin;
	cp arm-wince-lib.exe bin; mv arm-wince-lib.exe $(MOZCE_TOOLS_DIR); rm *.obj

$(MOZCE_TOOLS_DIR)/arm-wince-link.exe:	$(MOZCE_TOOLS_DIR)/arm-wince-link.c $(MOZCE_TOOLS_DIR)/toolspath.h
	$(CC) $(MOZCE_TOOLS_DIR)/arm-wince-link.c
	mkdir -p bin;
	cp arm-wince-link.exe bin; mv arm-wince-link.exe $(MOZCE_TOOLS_DIR); rm *.obj

$(MOZCE_TOOLS_DIR)/arm-wince-res.exe:	$(MOZCE_TOOLS_DIR)/arm-wince-res.c $(MOZCE_TOOLS_DIR)/toolspath.h
	$(CC) $(MOZCE_TOOLS_DIR)/arm-wince-res.c
	mkdir -p bin;
	cp arm-wince-res.exe bin; mv arm-wince-res.exe $(MOZCE_TOOLS_DIR); rm *.obj

