#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "NPL"); you may not use this file except in
# compliance with the NPL.  You may obtain a copy of the NPL at
# http://www.mozilla.org/NPL/
#
# Software distributed under the NPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
# for the specific language governing rights and limitations under the
# NPL.
#
# The Initial Developer of this code under the NPL is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
# Reserved.

# mozcpl.mak
#
#   This file makes moz.cpl, the Mozilla control panel extension .dll.

DEPTH = ..\..\..

!if "$(MOZ_DEBUG)"!=""
C_OPTS_DBG = /Zi
EXE_OPTS_DBG = /DEBUG
!if "$(MOZ_BITS)"=="32"
OBJDIR = $(MOZ_OUT)\x86Dbg
!else
OBJDIR = $(MOZ_OUT)\16x86Dbg
!endif
!else
C_OPTS_DBG = /O1
!if "$(MOZ_BITS)"=="32"
OBJDIR = $(MOZ_OUT)\x86Rel
!else
OBJDIR = $(MOZ_OUT)\16x86Rel
!endif
!endif

DLL    = $(OBJDIR)\mozilla.cpl

C_OPTS  = /nologo /c /I.. /Fo$@ $(C_OPTS_DBG)
CPP_OPTS = $(C_OPTS)
EXE_OPTS = $(EXE_OPTS_DBG) /OUT:$@
DLL_OPTS = $(EXE_OPTS) /DLL

all : dll

dll : $(DLL)

clean :
	-@erase $(DLL) 1>nul 2>&1
	-@erase $(OBJDIR)\mozcpl.* 1>nul 2>&1

$(DLL) : $(OBJDIR)\mozcpl.obj $(OBJDIR)\cpl_name.obj $(OBJDIR)\mozcpl.res ..\mozcpl.def mozcpl.mak
	link $(DLL_OPTS) /DEF:..\mozcpl.def /OUT:$(DLL) $(OBJDIR)\mozcpl.obj $(OBJDIR)\cpl_name.obj $(OBJDIR)\mozcpl.res user32.lib shell32.lib

$(OBJDIR)\mozcpl.obj : ..\mozcpl.cpp mozcpl.mak
	cl $(CPP_OPTS) ..\mozcpl.cpp

$(OBJDIR)\cpl_name.obj : ..\cpl_name.cpp mozcpl.mak
	cl $(CPP_OPTS) ..\cpl_name.cpp

$(OBJDIR)\mozcpl.res : ..\mozcpl.rc mozcpl.mak
	rc -r -fo$@ ..\mozcpl.rc
