# -*- Mode: Makefile -*-
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

#  */
# Microsoft Visual C++ generated build script - Do not modify

PROJ = MUC
DEBUG = 
PROGTYPE = 1
CALLER = 
ARGS = 
DLLS = 
D_RCDEFINES = -d_DEBUG
R_RCDEFINES = -dNDEBUG
ORIGIN = MSVC
ORIGIN_VER = 1.00
PROJPATH = E:\NS\CMD\DIALUP4\WIN\MUC\WIN16\
USEMFC = 1
CC = cl
CPP = cl
CXX = cl
CCREATEPCHFLAG = 
CPPCREATEPCHFLAG = 
CUSEPCHFLAG = 
CPPUSEPCHFLAG = 
FIRSTC =             
FIRSTCPP = STDAFX.CPP  
RC = rc
CFLAGS_D_WDLL = /nologo /G2 /W3 /Zi /ALw /YX /Od /D "_DEBUG" /D "_USRDLL" /D "WIN16" /GD /Fd"Muc.PDB" /Fp"Muc.PCH"
CFLAGS_R_WDLL = /nologo /W3 /ALw /YX /O1 /D "NDEBUG" /D "_USRDLL" /D "WIN16" /GD /Fp"Muc.PCH"
LFLAGS_D_WDLL = /NOLOGO /NOD /NOE /PACKC:61440 /ALIGN:16 /ONERROR:NOEXE /CO /MAP /MAP:FULL
LFLAGS_R_WDLL = /NOLOGO /NOD /NOE /PACKC:61440 /ALIGN:16 /ONERROR:NOEXE /MAP:FULL
LIBS_D_WDLL = lafxdwd oldnames libw ldllcew commdlg.lib olecli.lib olesvr.lib shell.lib 
LIBS_R_WDLL = lafxdw oldnames libw ldllcew commdlg.lib olecli.lib olesvr.lib shell.lib 
RCFLAGS = /nologo
RESFLAGS = /nologo
RUNFLAGS = 
DEFFILE = ..\src\MUC.DEF
OBJS_EXT = 
LIBS_EXT = 
!if "$(DEBUG)" == "1"
CFLAGS = $(CFLAGS_D_WDLL)
LFLAGS = $(LFLAGS_D_WDLL)
LIBS = $(LIBS_D_WDLL)
MAPFILE = nul
RCDEFINES = $(D_RCDEFINES)
!else
CFLAGS = $(CFLAGS_R_WDLL)
LFLAGS = $(LFLAGS_R_WDLL)
LIBS = $(LIBS_R_WDLL)
MAPFILE = nul
RCDEFINES = $(R_RCDEFINES)
!endif
!if [if exist MSVC.BND del MSVC.BND]
!endif
SBRS = RAS16.SBR \
		DIALSHR.SBR \
		MUC.SBR \
		STDAFX.SBR


RAS16_DEP = 

DIALSHR_DEP = 

MUC_DEP = 

STDAFX_DEP = 

all:    $(PROJ).DLL

RAS16.OBJ:      ..\src\RAS16.CPP $(RAS16_DEP)
	$(CPP) $(CFLAGS) $(CPPUSEPCHFLAG) /c ..\src\RAS16.CPP

DIALSHR.OBJ:    ..\src\DIALSHR.CPP $(DIALSHR_DEP)
	$(CPP) $(CFLAGS) $(CPPUSEPCHFLAG) /c ..\src\DIALSHR.CPP

MUC.OBJ:        ..\src\MUC.CPP $(MUC_DEP)
	$(CPP) $(CFLAGS) $(CPPUSEPCHFLAG) /c ..\src\MUC.CPP

STDAFX.OBJ:     ..\src\STDAFX.CPP $(STDAFX_DEP)
	$(CPP) $(CFLAGS) $(CPPCREATEPCHFLAG) /c ..\src\STDAFX.CPP


$(PROJ).DLL::   RAS16.OBJ DIALSHR.OBJ MUC.OBJ STDAFX.OBJ $(OBJS_EXT) $(DEFFILE)
	echo >NUL @<<$(PROJ).CRF
RAS16.OBJ +
DIALSHR.OBJ +
MUC.OBJ +
STDAFX.OBJ +
$(OBJS_EXT)
$(PROJ).DLL
$(MAPFILE)
c:\TOOLS\VC152\LIB\+
c:\TOOLS\VC152\MFC\LIB\+
$(LIBS)
$(DEFFILE);
<<
	link $(LFLAGS) @$(PROJ).CRF
	$(RC) $(RESFLAGS) $@
	implib /nowep $(PROJ).LIB $(PROJ).DLL


run: $(PROJ).DLL
	$(PROJ) $(RUNFLAGS)


$(PROJ).BSC: $(SBRS)
	bscmake @<<
/o$@ $(SBRS)
<<
