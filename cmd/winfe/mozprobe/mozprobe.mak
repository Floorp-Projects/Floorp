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

# mozprobe.mak
#
#   This file makes mozprobe.dll and two test programs:
#   probe1.exe (C++) and probe2.exe (C).  There are 4
#   pseudo-targets: dll (builds just the .dll), tests
#   (builds just the test programs), all (builds
#   both the .dll and the tests), and clean (erases .obj/.exe/.dll files.
#
#   "all" is the default target.

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

DLL    = $(OBJDIR)\mozprobe.dll
PROBE1 = $(OBJDIR)\probe1.exe
PROBE2 = $(OBJDIR)\probe2.exe

C_OPTS  = /nologo /c /I.. /Fo$@ $(C_OPTS_DBG)
CPP_OPTS = $(C_OPTS)
EXE_OPTS = $(EXE_OPTS_DBG) /OUT:$@
DLL_OPTS = $(EXE_OPTS) /DLL

all : dll tests

dll : $(DLL)

tests : $(PROBE1) $(PROBE2)

clean :
	-@erase $(DLL) $(PROBE1) $(PROBE2) 1>nul 2>&1
	-@erase $(OBJDIR)\mozprobe.* $(OBJDIR)\probe1.* $(OBJDIR)\probe2.obj 1>nul 2>&1

$(DLL) : $(OBJDIR)\mozprobe.obj mozprobe.def
	link $(DLL_OPTS) /IMPLIB:$(OBJDIR)\mozprobe.lib /DEF:mozprobe.def $(OBJDIR)\mozprobe.obj user32.lib

$(OBJDIR)\mozprobe.obj : mozprobe.cpp ..\mozprobe.h mozprobe.mak
	cl $(CPP_OPTS) mozprobe.cpp

$(PROBE1) : $(OBJDIR)\probe1.obj $(OBJDIR)\mozprobe.lib
	link $(EXE_OPTS) $**

$(PROBE2) : $(OBJDIR)\probe2.obj $(OBJDIR)\mozprobe.lib
	link $(EXE_OPTS) $**

$(OBJDIR)\mozprobe.lib : $(DLL)

$(OBJDIR)\probe1.obj : probe1.cpp ..\mozprobe.h
	cl $(CPP_OPTS) probe1.cpp

$(OBJDIR)\probe2.obj : probe2.c ..\mozprobe.h
	cl $(C_OPTS) probe2.c


