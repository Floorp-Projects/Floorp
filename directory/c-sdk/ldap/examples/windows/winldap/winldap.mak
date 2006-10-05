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
# The Original Code is Mozilla Communicator client code.
# 
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1996-1999
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

# Microsoft Developer Studio Generated NMAKE File, Format Version 4.20
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

!IF "$(CFG)" == ""
CFG=winldap - Win32 Debug
!MESSAGE No configuration specified.  Defaulting to winldap - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "winldap - Win32 Release" && "$(CFG)" !=\
 "winldap - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "winldap.mak" CFG="winldap - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "winldap - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "winldap - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 
################################################################################
# Begin Project
# PROP Target_Last_Scanned "winldap - Win32 Debug"
MTL=mktyplib.exe
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "winldap - Win32 Release"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
OUTDIR=.\Release
INTDIR=.\Release

ALL : "$(OUTDIR)\winldap.exe"

CLEAN : 
	-@erase "$(INTDIR)\ConnDlg.obj"
	-@erase "$(INTDIR)\LdapDoc.obj"
	-@erase "$(INTDIR)\LdapView.obj"
	-@erase "$(INTDIR)\MainFrm.obj"
	-@erase "$(INTDIR)\PropDlg.obj"
	-@erase "$(INTDIR)\SrchDlg.obj"
	-@erase "$(INTDIR)\StdAfx.obj"
	-@erase "$(INTDIR)\winldap.obj"
	-@erase "$(INTDIR)\winldap.pch"
	-@erase "$(INTDIR)\winldap.res"
	-@erase "$(OUTDIR)\winldap.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /Yu"stdafx.h" /c
# ADD CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /Yu"stdafx.h" /c
CPP_PROJ=/nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D\
 "_AFXDLL" /D "_MBCS" /Fp"$(INTDIR)/winldap.pch" /Yu"stdafx.h" /Fo"$(INTDIR)/"\
 /c 
CPP_OBJS=.\Release/
CPP_SBRS=.\.
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /win32
MTL_PROJ=/nologo /D "NDEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
RSC_PROJ=/l 0x409 /fo"$(INTDIR)/winldap.res" /d "NDEBUG" /d "_AFXDLL" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/winldap.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /machine:I386
# ADD LINK32 nsldapssl32v60.lib /nologo /subsystem:windows /machine:I386
LINK32_FLAGS=nsldapssl32v60.lib /nologo /subsystem:windows /incremental:no\
 /pdb:"$(OUTDIR)/winldap.pdb" /machine:I386 /out:"$(OUTDIR)/winldap.exe" 
LINK32_OBJS= \
	"$(INTDIR)\ConnDlg.obj" \
	"$(INTDIR)\LdapDoc.obj" \
	"$(INTDIR)\LdapView.obj" \
	"$(INTDIR)\MainFrm.obj" \
	"$(INTDIR)\PropDlg.obj" \
	"$(INTDIR)\SrchDlg.obj" \
	"$(INTDIR)\StdAfx.obj" \
	"$(INTDIR)\winldap.obj" \
	"$(INTDIR)\winldap.res"

"$(OUTDIR)\winldap.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "winldap - Win32 Debug"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
OUTDIR=.\Debug
INTDIR=.\Debug

ALL : "$(OUTDIR)\winldap.exe"

CLEAN : 
	-@erase "$(INTDIR)\ConnDlg.obj"
	-@erase "$(INTDIR)\LdapDoc.obj"
	-@erase "$(INTDIR)\LdapView.obj"
	-@erase "$(INTDIR)\MainFrm.obj"
	-@erase "$(INTDIR)\PropDlg.obj"
	-@erase "$(INTDIR)\SrchDlg.obj"
	-@erase "$(INTDIR)\StdAfx.obj"
	-@erase "$(INTDIR)\vc40.idb"
	-@erase "$(INTDIR)\vc40.pdb"
	-@erase "$(INTDIR)\winldap.obj"
	-@erase "$(INTDIR)\winldap.pch"
	-@erase "$(INTDIR)\winldap.res"
	-@erase "$(OUTDIR)\winldap.exe"
	-@erase "$(OUTDIR)\winldap.ilk"
	-@erase "$(OUTDIR)\winldap.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /Yu"stdafx.h" /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /Yu"stdafx.h" /c
CPP_PROJ=/nologo /MDd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /D "_AFXDLL" /D "_MBCS" /Fp"$(INTDIR)/winldap.pch" /Yu"stdafx.h"\
 /Fo"$(INTDIR)/" /Fd"$(INTDIR)/" /c 
CPP_OBJS=.\Debug/
CPP_SBRS=.\.
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /win32
MTL_PROJ=/nologo /D "_DEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "_DEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "_DEBUG" /d "_AFXDLL"
RSC_PROJ=/l 0x409 /fo"$(INTDIR)/winldap.res" /d "_DEBUG" /d "_AFXDLL" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/winldap.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /debug /machine:I386
# ADD LINK32 nsldapssl32v60.lib /nologo /subsystem:windows /debug /machine:I386
LINK32_FLAGS=nsldapssl32v60.lib /nologo /subsystem:windows /incremental:yes\
 /pdb:"$(OUTDIR)/winldap.pdb" /debug /machine:I386 /out:"$(OUTDIR)/winldap.exe" 
LINK32_OBJS= \
	"$(INTDIR)\ConnDlg.obj" \
	"$(INTDIR)\LdapDoc.obj" \
	"$(INTDIR)\LdapView.obj" \
	"$(INTDIR)\MainFrm.obj" \
	"$(INTDIR)\PropDlg.obj" \
	"$(INTDIR)\SrchDlg.obj" \
	"$(INTDIR)\StdAfx.obj" \
	"$(INTDIR)\winldap.obj" \
	"$(INTDIR)\winldap.res"

"$(OUTDIR)\winldap.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.c{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

################################################################################
# Begin Target

# Name "winldap - Win32 Release"
# Name "winldap - Win32 Debug"

!IF  "$(CFG)" == "winldap - Win32 Release"

!ELSEIF  "$(CFG)" == "winldap - Win32 Debug"

!ENDIF 

################################################################################
# Begin Source File

SOURCE=.\winldap.cpp
DEP_CPP_WINLD=\
	".\ConnDlg.h"\
	".\LdapDoc.h"\
	".\LdapView.h"\
	".\MainFrm.h"\
	".\SrchDlg.h"\
	".\StdAfx.h"\
	".\winldap.h"\
	"d:\projects\ldapsdk\include\lber.h"\
	{$(INCLUDE)}"\ldap.h"\
	{$(INCLUDE)}"\sys\time.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\winldap.obj" : $(SOURCE) $(DEP_CPP_WINLD) "$(INTDIR)"\
 "$(INTDIR)\winldap.pch"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\StdAfx.cpp
DEP_CPP_STDAF=\
	".\StdAfx.h"\
	

!IF  "$(CFG)" == "winldap - Win32 Release"

# ADD CPP /Yc"stdafx.h"

BuildCmds= \
	$(CPP) /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D\
 "_AFXDLL" /D "_MBCS" /Fp"$(INTDIR)/winldap.pch" /Yc"stdafx.h" /Fo"$(INTDIR)/"\
 /c $(SOURCE) \
	

"$(INTDIR)\StdAfx.obj" : $(SOURCE) $(DEP_CPP_STDAF) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\winldap.pch" : $(SOURCE) $(DEP_CPP_STDAF) "$(INTDIR)"
   $(BuildCmds)

!ELSEIF  "$(CFG)" == "winldap - Win32 Debug"

# ADD CPP /Yc"stdafx.h"

BuildCmds= \
	$(CPP) /nologo /MDd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /D "_AFXDLL" /D "_MBCS" /Fp"$(INTDIR)/winldap.pch" /Yc"stdafx.h"\
 /Fo"$(INTDIR)/" /Fd"$(INTDIR)/" /c $(SOURCE) \
	

"$(INTDIR)\StdAfx.obj" : $(SOURCE) $(DEP_CPP_STDAF) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\winldap.pch" : $(SOURCE) $(DEP_CPP_STDAF) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\MainFrm.cpp
DEP_CPP_MAINF=\
	".\MainFrm.h"\
	".\StdAfx.h"\
	".\winldap.h"\
	

"$(INTDIR)\MainFrm.obj" : $(SOURCE) $(DEP_CPP_MAINF) "$(INTDIR)"\
 "$(INTDIR)\winldap.pch"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\LdapDoc.cpp
DEP_CPP_LDAPD=\
	".\LdapDoc.h"\
	".\StdAfx.h"\
	".\winldap.h"\
	

"$(INTDIR)\LdapDoc.obj" : $(SOURCE) $(DEP_CPP_LDAPD) "$(INTDIR)"\
 "$(INTDIR)\winldap.pch"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\LdapView.cpp
DEP_CPP_LDAPV=\
	".\LdapDoc.h"\
	".\LdapView.h"\
	".\PropDlg.h"\
	".\StdAfx.h"\
	".\winldap.h"\
	"d:\projects\ldapsdk\include\lber.h"\
	{$(INCLUDE)}"\ldap.h"\
	{$(INCLUDE)}"\sys\time.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\LdapView.obj" : $(SOURCE) $(DEP_CPP_LDAPV) "$(INTDIR)"\
 "$(INTDIR)\winldap.pch"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\winldap.rc
DEP_RSC_WINLDA=\
	".\res\LdapDoc.ico"\
	".\res\winldap.ico"\
	".\res\winldap.rc2"\
	

"$(INTDIR)\winldap.res" : $(SOURCE) $(DEP_RSC_WINLDA) "$(INTDIR)"
   $(RSC) $(RSC_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\ConnDlg.cpp
DEP_CPP_CONND=\
	".\ConnDlg.h"\
	".\StdAfx.h"\
	".\winldap.h"\
	

"$(INTDIR)\ConnDlg.obj" : $(SOURCE) $(DEP_CPP_CONND) "$(INTDIR)"\
 "$(INTDIR)\winldap.pch"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\SrchDlg.cpp
DEP_CPP_SRCHD=\
	".\SrchDlg.h"\
	".\StdAfx.h"\
	".\winldap.h"\
	"d:\projects\ldapsdk\include\lber.h"\
	{$(INCLUDE)}"\ldap.h"\
	{$(INCLUDE)}"\sys\time.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\SrchDlg.obj" : $(SOURCE) $(DEP_CPP_SRCHD) "$(INTDIR)"\
 "$(INTDIR)\winldap.pch"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\PropDlg.cpp
DEP_CPP_PROPD=\
	".\PropDlg.h"\
	".\StdAfx.h"\
	".\winldap.h"\
	

"$(INTDIR)\PropDlg.obj" : $(SOURCE) $(DEP_CPP_PROPD) "$(INTDIR)"\
 "$(INTDIR)\winldap.pch"


# End Source File
# End Target
# End Project
################################################################################
