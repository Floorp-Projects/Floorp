# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
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
# Copyright (C) 1998-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
# 

# Microsoft Developer Studio Generated NMAKE File, Format Version 4.20
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103
# TARGTYPE "Win32 (x86) Static Library" 0x0104

!IF "$(CFG)" == ""
CFG=libldif - Win32 Debug
!MESSAGE No configuration specified.  Defaulting to libldif - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "ldapdelete - Win32 Release" && "$(CFG)" !=\
 "ldapdelete - Win32 Debug" && "$(CFG)" != "libutil - Win32 Release" && "$(CFG)"\
 != "libutil - Win32 Debug" && "$(CFG)" != "ldapmodify - Win32 Release" &&\
 "$(CFG)" != "ldapmodify - Win32 Debug" && "$(CFG)" !=\
 "ldapsearch - Win32 Release" && "$(CFG)" != "ldapsearch - Win32 Debug" &&\
 "$(CFG)" != "ldapmodrdn - Win32 Release" && "$(CFG)" !=\
 "ldapmodrdn - Win32 Debug" && "$(CFG)" != "libldif - Win32 Release" && "$(CFG)"\
 != "libldif - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "tools.mak" CFG="libldif - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "ldapdelete - Win32 Release" (based on\
 "Win32 (x86) Console Application")
!MESSAGE "ldapdelete - Win32 Debug" (based on\
 "Win32 (x86) Console Application")
!MESSAGE "libutil - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libutil - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "ldapmodify - Win32 Release" (based on\
 "Win32 (x86) Console Application")
!MESSAGE "ldapmodify - Win32 Debug" (based on\
 "Win32 (x86) Console Application")
!MESSAGE "ldapsearch - Win32 Release" (based on\
 "Win32 (x86) Console Application")
!MESSAGE "ldapsearch - Win32 Debug" (based on\
 "Win32 (x86) Console Application")
!MESSAGE "ldapmodrdn - Win32 Release" (based on\
 "Win32 (x86) Console Application")
!MESSAGE "ldapmodrdn - Win32 Debug" (based on\
 "Win32 (x86) Console Application")
!MESSAGE "libldif - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libldif - Win32 Debug" (based on "Win32 (x86) Static Library")
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
# PROP Target_Last_Scanned "libldif - Win32 Debug"

!IF  "$(CFG)" == "ldapdelete - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "ldapdelete\Release"
# PROP BASE Intermediate_Dir "ldapdelete\Release"
# PROP BASE Target_Dir "ldapdelete"
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "release"
# PROP Intermediate_Dir "release\ldapdelete"
# PROP Target_Dir "ldapdelete"
OUTDIR=.\release
INTDIR=.\release\ldapdelete

ALL : "libutil - Win32 Release" "$(OUTDIR)\ldapdelete.exe"

CLEAN : 
	-@erase "$(INTDIR)\ldapdelete.obj"
	-@erase "$(OUTDIR)\ldapdelete.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

"$(INTDIR)" :
    if not exist "$(INTDIR)/$(NULL)" mkdir "$(INTDIR)"

CPP=cl.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /YX /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "..\..\dist\WINNT3.51_dbg.obj\include\nspr20\pr" /I "include" /I "..\..\include" /I "..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr" /D "NET_SSL" /D "NDEBUG" /D "_CONSOLE" /D "_WINDOWS" /D "LDAP_DEBUG" /D "LDBM_USE_DBBTREE" /D "NEEDPROTOS" /D "WIN32_KERNEL_THREADS" /D "LDAP_REFERRALS" /D "WINDOWS" /D "XP_PC" /D "XP_WIN32" /D "USE_NSPR_MT" /D "SYSERRLIST_IN_STDIO" /D "WIN32" /YX /c
# SUBTRACT CPP /u
CPP_PROJ=/nologo /MD /W3 /GX /O2 /I\
 "..\..\dist\WINNT3.51_dbg.obj\include\nspr20\pr" /I "include" /I\
 "..\..\include" /I "..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr" /D "NET_SSL"\
 /D "NDEBUG" /D "_CONSOLE" /D "_WINDOWS" /D "LDAP_DEBUG" /D "LDBM_USE_DBBTREE"\
 /D "NEEDPROTOS" /D "WIN32_KERNEL_THREADS" /D "LDAP_REFERRALS" /D "WINDOWS" /D\
 "XP_PC" /D "XP_WIN32" /D "USE_NSPR_MT" /D "SYSERRLIST_IN_STDIO" /D "WIN32"\
 /Fp"$(INTDIR)/ldapdelete.pch" /YX /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\release\ldapdelete/
CPP_SBRS=.\.

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

RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/ldapdelete.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 nssldap32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib nsldap32.lib nslch32.lib rpcrt4.lib ..\..\dist\WINNT4.0_OPT.OBJ\lib\libsec-export.lib ..\..\dist\WINNT4.0_OPT.OBJ\lib\libdbm.lib ..\..\dist\WINNT4.0_OPT.OBJ\lib\libnspr20.lib ..\..\dist\WINNT4.0_OPT.OBJ\lib\libxp.lib libutil.lib /nologo /subsystem:console /machine:I386 /force /LIBPATH:release
# SUBTRACT LINK32 /pdb:none /nodefaultlib
LINK32_FLAGS=nssldap32.lib kernel32.lib user32.lib gdi32.lib winspool.lib\
 comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib\
 odbc32.lib odbccp32.lib wsock32.lib nsldap32.lib nslch32.lib rpcrt4.lib\
 ..\..\dist\WINNT4.0_OPT.OBJ\lib\libsec-export.lib\
 ..\..\dist\WINNT4.0_OPT.OBJ\lib\libdbm.lib\
 ..\..\dist\WINNT4.0_OPT.OBJ\lib\libnspr20.lib\
 ..\..\dist\WINNT4.0_OPT.OBJ\lib\libxp.lib libutil.lib /nologo\
 /subsystem:console /incremental:no /pdb:"$(OUTDIR)/ldapdelete.pdb"\
 /machine:I386 /force /out:"$(OUTDIR)/ldapdelete.exe" /LIBPATH:release 
LINK32_OBJS= \
	"$(INTDIR)\ldapdelete.obj" \
	"$(OUTDIR)\libutil.lib"

"$(OUTDIR)\ldapdelete.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "ldapdelete - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "ldapdelete\Debug"
# PROP BASE Intermediate_Dir "ldapdelete\Debug"
# PROP BASE Target_Dir "ldapdelete"
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "debug"
# PROP Intermediate_Dir "debug\ldapdelete"
# PROP Target_Dir "ldapdelete"
OUTDIR=.\debug
INTDIR=.\debug\ldapdelete

ALL : "libutil - Win32 Debug" "$(OUTDIR)\ldapdelete.exe"

CLEAN : 
	-@erase "$(INTDIR)\ldapdelete.obj"
	-@erase "$(INTDIR)\vc40.idb"
	-@erase "$(INTDIR)\vc40.pdb"
	-@erase "$(OUTDIR)\ldapdelete.exe"
	-@erase "$(OUTDIR)\ldapdelete.ilk"
	-@erase "$(OUTDIR)\ldapdelete.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

"$(INTDIR)" :
    if not exist "$(INTDIR)/$(NULL)" mkdir "$(INTDIR)"

CPP=cl.exe
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /YX /c
# ADD CPP /nologo /MD /W3 /Gm /GX /Zi /Od /I "include" /I "..\..\include" /I "..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr" /D "_DEBUG" /D "_CONSOLE" /D "_WINDOWS" /D "LDAP_DEBUG" /D "NET_SSL" /D "LDBM_USE_DBBTREE" /D "NEEDPROTOS" /D "WIN32_KERNEL_THREADS" /D "LDAP_REFERRALS" /D "WINDOWS" /D "XP_PC" /D "XP_WIN32" /D "USE_NSPR_MT" /D "SYSERRLIST_IN_STDIO" /D "WIN32" /YX /c
# SUBTRACT CPP /u
CPP_PROJ=/nologo /MD /W3 /Gm /GX /Zi /Od /I "include" /I "..\..\include" /I\
 "..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr" /D "_DEBUG" /D "_CONSOLE" /D\
 "_WINDOWS" /D "LDAP_DEBUG" /D "NET_SSL" /D "LDBM_USE_DBBTREE" /D "NEEDPROTOS"\
 /D "WIN32_KERNEL_THREADS" /D "LDAP_REFERRALS" /D "WINDOWS" /D "XP_PC" /D\
 "XP_WIN32" /D "USE_NSPR_MT" /D "SYSERRLIST_IN_STDIO" /D "WIN32"\
 /Fp"$(INTDIR)/ldapdelete.pch" /YX /Fo"$(INTDIR)/" /Fd"$(INTDIR)/" /c 
CPP_OBJS=.\debug\ldapdelete/
CPP_SBRS=.\.

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

RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/ldapdelete.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386
# ADD LINK32 nssldap32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib rpcrt4.lib nsldap32.lib nslch32.lib ..\..\dist\WINNT4.0_DBG.OBJ\lib\libsec-export.lib ..\..\dist\WINNT4.0_DBG.OBJ\lib\libdbm.lib ..\..\dist\WINNT4.0_DBG.OBJ\lib\libnspr20.lib ..\..\dist\WINNT4.0_DBG.OBJ\lib\libxp.lib libutil.lib /nologo /subsystem:console /debug /machine:I386 /force /LIBPATH:debug
# SUBTRACT LINK32 /pdb:none /nodefaultlib
LINK32_FLAGS=nssldap32.lib kernel32.lib user32.lib gdi32.lib winspool.lib\
 comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib\
 odbc32.lib odbccp32.lib wsock32.lib rpcrt4.lib nsldap32.lib nslch32.lib\
 ..\..\dist\WINNT4.0_DBG.OBJ\lib\libsec-export.lib\
 ..\..\dist\WINNT4.0_DBG.OBJ\lib\libdbm.lib\
 ..\..\dist\WINNT4.0_DBG.OBJ\lib\libnspr20.lib\
 ..\..\dist\WINNT4.0_DBG.OBJ\lib\libxp.lib libutil.lib /nologo\
 /subsystem:console /incremental:yes /pdb:"$(OUTDIR)/ldapdelete.pdb" /debug\
 /machine:I386 /force /out:"$(OUTDIR)/ldapdelete.exe" /LIBPATH:debug 
LINK32_OBJS= \
	"$(INTDIR)\ldapdelete.obj" \
	"$(OUTDIR)\libutil.lib"

"$(OUTDIR)\ldapdelete.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "libutil - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "libutil\Release"
# PROP BASE Intermediate_Dir "libutil\Release"
# PROP BASE Target_Dir "libutil"
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "release"
# PROP Intermediate_Dir "release\libutil"
# PROP Target_Dir "libutil"
OUTDIR=.\release
INTDIR=.\release\libutil

ALL : "$(OUTDIR)\libutil.lib"

CLEAN : 
	-@erase "$(INTDIR)\getopt.obj"
	-@erase "$(INTDIR)\ntdebug.obj"
	-@erase "$(INTDIR)\ntstubs.obj"
	-@erase "$(OUTDIR)\libutil.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

"$(INTDIR)" :
    if not exist "$(INTDIR)/$(NULL)" mkdir "$(INTDIR)"

CPP=cl.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "include" /I "o:\ns\include" /I "o:\ns\dist\public" /I "o:\ns\dist\public\nspr" /D "NDEBUG" /D "_WIN32" /D "_WINDOWS" /D "LDAP_DEBUG" /D "LDBM_USE_DBBTREE" /D "NEEDPROTOS" /D "WIN32_KERNEL_THREADS" /D "LDAP_REFERRALS" /D "WINDOWS" /D "XP_PC" /D "XP_WIN32" /D "USE_NSPR_MT" /D "SYSERRLIST_IN_STDIO" /D "WIN32" /D "NET_SSL" /YX /c
# SUBTRACT CPP /u
CPP_PROJ=/nologo /MD /W3 /GX /O2 /I "include" /I "o:\ns\include" /I\
 "o:\ns\dist\public" /I "o:\ns\dist\public\nspr" /D "NDEBUG" /D "_WIN32" /D\
 "_WINDOWS" /D "LDAP_DEBUG" /D "LDBM_USE_DBBTREE" /D "NEEDPROTOS" /D\
 "WIN32_KERNEL_THREADS" /D "LDAP_REFERRALS" /D "WINDOWS" /D "XP_PC" /D\
 "XP_WIN32" /D "USE_NSPR_MT" /D "SYSERRLIST_IN_STDIO" /D "WIN32" /D "NET_SSL"\
 /Fp"$(INTDIR)/libutil.pch" /YX /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\release\libutil/
CPP_SBRS=.\.

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

BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/libutil.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo
LIB32_FLAGS=/nologo /out:"$(OUTDIR)/libutil.lib" 
LIB32_OBJS= \
	"$(INTDIR)\getopt.obj" \
	"$(INTDIR)\ntdebug.obj" \
	"$(INTDIR)\ntstubs.obj"

"$(OUTDIR)\libutil.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ELSEIF  "$(CFG)" == "libutil - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "libutil\Debug"
# PROP BASE Intermediate_Dir "libutil\Debug"
# PROP BASE Target_Dir "libutil"
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "debug"
# PROP Intermediate_Dir "debug\libutil"
# PROP Target_Dir "libutil"
OUTDIR=.\debug
INTDIR=.\debug\libutil

ALL : "$(OUTDIR)\libutil.lib"

CLEAN : 
	-@erase "$(INTDIR)\getopt.obj"
	-@erase "$(INTDIR)\ntdebug.obj"
	-@erase "$(INTDIR)\ntstubs.obj"
	-@erase "$(OUTDIR)\libutil.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

"$(INTDIR)" :
    if not exist "$(INTDIR)/$(NULL)" mkdir "$(INTDIR)"

CPP=cl.exe
# ADD BASE CPP /nologo /W3 /GX /Z7 /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MD /W3 /GX /Z7 /Od /I "include" /I "o:\ns\include" /I "o:\ns\dist\public" /I "o:\ns\dist\public\nspr" /D "_DEBUG" /D "_WIN32" /D "_WINDOWS" /D "LDAP_DEBUG" /D "LDBM_USE_DBBTREE" /D "NEEDPROTOS" /D "WIN32_KERNEL_THREADS" /D "LDAP_REFERRALS" /D "WINDOWS" /D "XP_PC" /D "XP_WIN32" /D "USE_NSPR_MT" /D "SYSERRLIST_IN_STDIO" /D "WIN32" /D "NET_SSL" /YX /c
# SUBTRACT CPP /u
CPP_PROJ=/nologo /MD /W3 /GX /Z7 /Od /I "include" /I "o:\ns\include" /I\
 "o:\ns\dist\public" /I "o:\ns\dist\public\nspr" /D "_DEBUG" /D "_WIN32" /D\
 "_WINDOWS" /D "LDAP_DEBUG" /D "LDBM_USE_DBBTREE" /D "NEEDPROTOS" /D\
 "WIN32_KERNEL_THREADS" /D "LDAP_REFERRALS" /D "WINDOWS" /D "XP_PC" /D\
 "XP_WIN32" /D "USE_NSPR_MT" /D "SYSERRLIST_IN_STDIO" /D "WIN32" /D "NET_SSL"\
 /Fp"$(INTDIR)/libutil.pch" /YX /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\debug\libutil/
CPP_SBRS=.\.

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

BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/libutil.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo
LIB32_FLAGS=/nologo /out:"$(OUTDIR)/libutil.lib" 
LIB32_OBJS= \
	"$(INTDIR)\getopt.obj" \
	"$(INTDIR)\ntdebug.obj" \
	"$(INTDIR)\ntstubs.obj"

"$(OUTDIR)\libutil.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ELSEIF  "$(CFG)" == "ldapmodify - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "ldapmodify\Release"
# PROP BASE Intermediate_Dir "ldapmodify\Release"
# PROP BASE Target_Dir "ldapmodify"
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "release"
# PROP Intermediate_Dir "release\ldapmodify"
# PROP Target_Dir "ldapmodify"
OUTDIR=.\release
INTDIR=.\release\ldapmodify

ALL : "libldif - Win32 Release" "libutil - Win32 Release"\
 "$(OUTDIR)\ldapmodify.exe"

CLEAN : 
	-@erase "$(INTDIR)\ldapmodify.obj"
	-@erase "$(OUTDIR)\ldapmodify.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

"$(INTDIR)" :
    if not exist "$(INTDIR)/$(NULL)" mkdir "$(INTDIR)"

CPP=cl.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /YX /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "..\..\dist\WINNT3.51_dbg.obj\include\nspr20\pr" /I "include" /I "..\..\include" /I "..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr" /D "NET_SSL" /D "NDEBUG" /D "_CONSOLE" /D "_WINDOWS" /D "LDAP_DEBUG" /D "LDBM_USE_DBBTREE" /D "NEEDPROTOS" /D "WIN32_KERNEL_THREADS" /D "LDAP_REFERRALS" /D "WINDOWS" /D "XP_PC" /D "XP_WIN32" /D "USE_NSPR_MT" /D "SYSERRLIST_IN_STDIO" /D "WIN32" /YX /c
# SUBTRACT CPP /u
CPP_PROJ=/nologo /MD /W3 /GX /O2 /I\
 "..\..\dist\WINNT3.51_dbg.obj\include\nspr20\pr" /I "include" /I\
 "..\..\include" /I "..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr" /D "NET_SSL"\
 /D "NDEBUG" /D "_CONSOLE" /D "_WINDOWS" /D "LDAP_DEBUG" /D "LDBM_USE_DBBTREE"\
 /D "NEEDPROTOS" /D "WIN32_KERNEL_THREADS" /D "LDAP_REFERRALS" /D "WINDOWS" /D\
 "XP_PC" /D "XP_WIN32" /D "USE_NSPR_MT" /D "SYSERRLIST_IN_STDIO" /D "WIN32"\
 /Fp"$(INTDIR)/ldapmodify.pch" /YX /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\release\ldapmodify/
CPP_SBRS=.\.

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

RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/ldapmodify.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 nssldap32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib nsldap32.lib nslch32.lib rpcrt4.lib ..\..\dist\WINNT4.0_OPT.OBJ\lib\libsec-export.lib ..\..\dist\WINNT4.0_OPT.OBJ\lib\libdbm.lib ..\..\dist\WINNT4.0_OPT.OBJ\lib\libnspr20.lib ..\..\dist\WINNT4.0_OPT.OBJ\lib\libxp.lib libutil.lib /nologo /subsystem:console /machine:I386 /force /LIBPATH:release
# SUBTRACT LINK32 /pdb:none /nodefaultlib
LINK32_FLAGS=nssldap32.lib kernel32.lib user32.lib gdi32.lib winspool.lib\
 comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib\
 odbc32.lib odbccp32.lib wsock32.lib nsldap32.lib nslch32.lib rpcrt4.lib\
 ..\..\dist\WINNT4.0_OPT.OBJ\lib\libsec-export.lib\
 ..\..\dist\WINNT4.0_OPT.OBJ\lib\libdbm.lib\
 ..\..\dist\WINNT4.0_OPT.OBJ\lib\libnspr20.lib\
 ..\..\dist\WINNT4.0_OPT.OBJ\lib\libxp.lib libutil.lib /nologo\
 /subsystem:console /incremental:no /pdb:"$(OUTDIR)/ldapmodify.pdb"\
 /machine:I386 /force /out:"$(OUTDIR)/ldapmodify.exe" /LIBPATH:release 
LINK32_OBJS= \
	"$(INTDIR)\ldapmodify.obj" \
	"$(OUTDIR)\libutil.lib" \
	".\debug\libldif.lib"

"$(OUTDIR)\ldapmodify.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "ldapmodify - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "ldapmodify\Debug"
# PROP BASE Intermediate_Dir "ldapmodify\Debug"
# PROP BASE Target_Dir "ldapmodify"
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "debug"
# PROP Intermediate_Dir "debug\ldapmodify"
# PROP Target_Dir "ldapmodify"
OUTDIR=.\debug
INTDIR=.\debug\ldapmodify

ALL : "libldif - Win32 Debug" "libutil - Win32 Debug"\
 "$(OUTDIR)\ldapmodify.exe"

CLEAN : 
	-@erase "$(INTDIR)\ldapmodify.obj"
	-@erase "$(INTDIR)\vc40.idb"
	-@erase "$(INTDIR)\vc40.pdb"
	-@erase "$(OUTDIR)\ldapmodify.exe"
	-@erase "$(OUTDIR)\ldapmodify.ilk"
	-@erase "$(OUTDIR)\ldapmodify.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

"$(INTDIR)" :
    if not exist "$(INTDIR)/$(NULL)" mkdir "$(INTDIR)"

CPP=cl.exe
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /YX /c
# ADD CPP /nologo /MD /W3 /Gm /GX /Zi /Od /I "include" /I "..\..\include" /I "..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr" /D "_DEBUG" /D "_CONSOLE" /D "_WINDOWS" /D "LDAP_DEBUG" /D "NET_SSL" /D "LDBM_USE_DBBTREE" /D "NEEDPROTOS" /D "WIN32_KERNEL_THREADS" /D "LDAP_REFERRALS" /D "WINDOWS" /D "XP_PC" /D "XP_WIN32" /D "USE_NSPR_MT" /D "SYSERRLIST_IN_STDIO" /D "WIN32" /YX /c
# SUBTRACT CPP /u
CPP_PROJ=/nologo /MD /W3 /Gm /GX /Zi /Od /I "include" /I "..\..\include" /I\
 "..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr" /D "_DEBUG" /D "_CONSOLE" /D\
 "_WINDOWS" /D "LDAP_DEBUG" /D "NET_SSL" /D "LDBM_USE_DBBTREE" /D "NEEDPROTOS"\
 /D "WIN32_KERNEL_THREADS" /D "LDAP_REFERRALS" /D "WINDOWS" /D "XP_PC" /D\
 "XP_WIN32" /D "USE_NSPR_MT" /D "SYSERRLIST_IN_STDIO" /D "WIN32"\
 /Fp"$(INTDIR)/ldapmodify.pch" /YX /Fo"$(INTDIR)/" /Fd"$(INTDIR)/" /c 
CPP_OBJS=.\debug\ldapmodify/
CPP_SBRS=.\.

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

RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/ldapmodify.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386
# ADD LINK32 nssldap32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib rpcrt4.lib nsldap32.lib nslch32.lib ..\..\dist\WINNT4.0_DBG.OBJ\lib\libsec-export.lib ..\..\dist\WINNT4.0_DBG.OBJ\lib\libdbm.lib ..\..\dist\WINNT4.0_DBG.OBJ\lib\libnspr20.lib ..\..\dist\WINNT4.0_DBG.OBJ\lib\libxp.lib libutil.lib /nologo /subsystem:console /debug /machine:I386 /force /LIBPATH:debug
# SUBTRACT LINK32 /pdb:none /nodefaultlib
LINK32_FLAGS=nssldap32.lib kernel32.lib user32.lib gdi32.lib winspool.lib\
 comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib\
 odbc32.lib odbccp32.lib wsock32.lib rpcrt4.lib nsldap32.lib nslch32.lib\
 ..\..\dist\WINNT4.0_DBG.OBJ\lib\libsec-export.lib\
 ..\..\dist\WINNT4.0_DBG.OBJ\lib\libdbm.lib\
 ..\..\dist\WINNT4.0_DBG.OBJ\lib\libnspr20.lib\
 ..\..\dist\WINNT4.0_DBG.OBJ\lib\libxp.lib libutil.lib /nologo\
 /subsystem:console /incremental:yes /pdb:"$(OUTDIR)/ldapmodify.pdb" /debug\
 /machine:I386 /force /out:"$(OUTDIR)/ldapmodify.exe" /LIBPATH:debug 
LINK32_OBJS= \
	"$(INTDIR)\ldapmodify.obj" \
	"$(OUTDIR)\libldif.lib" \
	"$(OUTDIR)\libutil.lib"

"$(OUTDIR)\ldapmodify.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "ldapsearch - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "ldapsearch\Release"
# PROP BASE Intermediate_Dir "ldapsearch\Release"
# PROP BASE Target_Dir "ldapsearch"
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "release"
# PROP Intermediate_Dir "release\ldapsearch"
# PROP Target_Dir "ldapsearch"
OUTDIR=.\release
INTDIR=.\release\ldapsearch

ALL : "libldif - Win32 Release" "libutil - Win32 Release"\
 "$(OUTDIR)\ldapsearch.exe"

CLEAN : 
	-@erase "$(INTDIR)\ldapsearch.obj"
	-@erase "$(OUTDIR)\ldapsearch.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

"$(INTDIR)" :
    if not exist "$(INTDIR)/$(NULL)" mkdir "$(INTDIR)"

CPP=cl.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /YX /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "..\..\dist\WINNT3.51_opt.obj\include\nspr20\pr" /I "include" /I "..\..\include" /I "..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr" /D "NDEBUG" /D "_CONSOLE" /D "_WINDOWS" /D "NET_SSL" /D "LDAP_DEBUG" /D "LDBM_USE_DBBTREE" /D "NEEDPROTOS" /D "WIN32_KERNEL_THREADS" /D "LDAP_REFERRALS" /D "WINDOWS" /D "XP_PC" /D "XP_WIN32" /D "USE_NSPR_MT" /D "SYSERRLIST_IN_STDIO" /D "WIN32" /YX /c
# SUBTRACT CPP /u
CPP_PROJ=/nologo /MD /W3 /GX /O2 /I\
 "..\..\dist\WINNT3.51_opt.obj\include\nspr20\pr" /I "include" /I\
 "..\..\include" /I "..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr" /D "NDEBUG"\
 /D "_CONSOLE" /D "_WINDOWS" /D "NET_SSL" /D "LDAP_DEBUG" /D "LDBM_USE_DBBTREE"\
 /D "NEEDPROTOS" /D "WIN32_KERNEL_THREADS" /D "LDAP_REFERRALS" /D "WINDOWS" /D\
 "XP_PC" /D "XP_WIN32" /D "USE_NSPR_MT" /D "SYSERRLIST_IN_STDIO" /D "WIN32"\
 /Fp"$(INTDIR)/ldapsearch.pch" /YX /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\release\ldapsearch/
CPP_SBRS=.\.

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

RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/ldapsearch.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 nssldap32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib nsldap32.lib nslch32.lib rpcrt4.lib ..\..\dist\WINNT4.0_OPT.OBJ\lib\libsec-export.lib ..\..\dist\WINNT4.0_OPT.OBJ\lib\libdbm.lib ..\..\dist\WINNT4.0_OPT.OBJ\lib\libnspr20.lib ..\..\dist\WINNT4.0_OPT.OBJ\lib\libxp.lib libutil.lib /nologo /subsystem:console /machine:I386 /force /LIBPATH:release
# SUBTRACT LINK32 /pdb:none /nodefaultlib
LINK32_FLAGS=nssldap32.lib kernel32.lib user32.lib gdi32.lib winspool.lib\
 comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib\
 odbc32.lib odbccp32.lib wsock32.lib nsldap32.lib nslch32.lib rpcrt4.lib\
 ..\..\dist\WINNT4.0_OPT.OBJ\lib\libsec-export.lib\
 ..\..\dist\WINNT4.0_OPT.OBJ\lib\libdbm.lib\
 ..\..\dist\WINNT4.0_OPT.OBJ\lib\libnspr20.lib\
 ..\..\dist\WINNT4.0_OPT.OBJ\lib\libxp.lib libutil.lib /nologo\
 /subsystem:console /incremental:no /pdb:"$(OUTDIR)/ldapsearch.pdb"\
 /machine:I386 /force /out:"$(OUTDIR)/ldapsearch.exe" /LIBPATH:release 
LINK32_OBJS= \
	"$(INTDIR)\ldapsearch.obj" \
	"$(OUTDIR)\libutil.lib" \
	".\debug\libldif.lib"

"$(OUTDIR)\ldapsearch.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "ldapsearch - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "ldapsearch\Debug"
# PROP BASE Intermediate_Dir "ldapsearch\Debug"
# PROP BASE Target_Dir "ldapsearch"
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "debug"
# PROP Intermediate_Dir "debug\ldapsearch"
# PROP Target_Dir "ldapsearch"
OUTDIR=.\debug
INTDIR=.\debug\ldapsearch

ALL : "libldif - Win32 Debug" "libutil - Win32 Debug"\
 "$(OUTDIR)\ldapsearch.exe"

CLEAN : 
	-@erase "$(INTDIR)\ldapsearch.obj"
	-@erase "$(INTDIR)\vc40.idb"
	-@erase "$(INTDIR)\vc40.pdb"
	-@erase "$(OUTDIR)\ldapsearch.exe"
	-@erase "$(OUTDIR)\ldapsearch.ilk"
	-@erase "$(OUTDIR)\ldapsearch.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

"$(INTDIR)" :
    if not exist "$(INTDIR)/$(NULL)" mkdir "$(INTDIR)"

CPP=cl.exe
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /YX /c
# ADD CPP /nologo /MD /W3 /Gm /GX /Zi /Od /I "include" /I "..\..\include" /I "..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr" /D "_DEBUG" /D "_CONSOLE" /D "_WINDOWS" /D "LDAP_DEBUG" /D "NET_SSL" /D "LDBM_USE_DBBTREE" /D "NEEDPROTOS" /D "WIN32_KERNEL_THREADS" /D "LDAP_REFERRALS" /D "WINDOWS" /D "XP_PC" /D "XP_WIN32" /D "USE_NSPR_MT" /D "SYSERRLIST_IN_STDIO" /D "WIN32" /YX /c
# SUBTRACT CPP /u
CPP_PROJ=/nologo /MD /W3 /Gm /GX /Zi /Od /I "include" /I "..\..\include" /I\
 "..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr" /D "_DEBUG" /D "_CONSOLE" /D\
 "_WINDOWS" /D "LDAP_DEBUG" /D "NET_SSL" /D "LDBM_USE_DBBTREE" /D "NEEDPROTOS"\
 /D "WIN32_KERNEL_THREADS" /D "LDAP_REFERRALS" /D "WINDOWS" /D "XP_PC" /D\
 "XP_WIN32" /D "USE_NSPR_MT" /D "SYSERRLIST_IN_STDIO" /D "WIN32"\
 /Fp"$(INTDIR)/ldapsearch.pch" /YX /Fo"$(INTDIR)/" /Fd"$(INTDIR)/" /c 
CPP_OBJS=.\debug\ldapsearch/
CPP_SBRS=.\.

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

RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/ldapsearch.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386
# ADD LINK32 nssldap32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib rpcrt4.lib nsldap32.lib nslch32.lib ..\..\dist\WINNT4.0_DBG.OBJ\lib\libsec-export.lib ..\..\dist\WINNT4.0_DBG.OBJ\lib\libdbm.lib ..\..\dist\WINNT4.0_DBG.OBJ\lib\libnspr20.lib ..\..\dist\WINNT4.0_DBG.OBJ\lib\libxp.lib libutil.lib /nologo /subsystem:console /debug /machine:I386 /force /LIBPATH:debug
# SUBTRACT LINK32 /pdb:none /nodefaultlib
LINK32_FLAGS=nssldap32.lib kernel32.lib user32.lib gdi32.lib winspool.lib\
 comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib\
 odbc32.lib odbccp32.lib wsock32.lib rpcrt4.lib nsldap32.lib nslch32.lib\
 ..\..\dist\WINNT4.0_DBG.OBJ\lib\libsec-export.lib\
 ..\..\dist\WINNT4.0_DBG.OBJ\lib\libdbm.lib\
 ..\..\dist\WINNT4.0_DBG.OBJ\lib\libnspr20.lib\
 ..\..\dist\WINNT4.0_DBG.OBJ\lib\libxp.lib libutil.lib /nologo\
 /subsystem:console /incremental:yes /pdb:"$(OUTDIR)/ldapsearch.pdb" /debug\
 /machine:I386 /force /out:"$(OUTDIR)/ldapsearch.exe" /LIBPATH:debug 
LINK32_OBJS= \
	"$(INTDIR)\ldapsearch.obj" \
	"$(OUTDIR)\libldif.lib" \
	"$(OUTDIR)\libutil.lib"

"$(OUTDIR)\ldapsearch.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "ldapmodrdn - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "ldapmodrdn\Release"
# PROP BASE Intermediate_Dir "ldapmodrdn\Release"
# PROP BASE Target_Dir "ldapmodrdn"
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "release"
# PROP Intermediate_Dir "release\ldapmodrdn"
# PROP Target_Dir "ldapmodrdn"
OUTDIR=.\release
INTDIR=.\release\ldapmodrdn

ALL : "libutil - Win32 Release" "$(OUTDIR)\ldapmodrdn.exe"

CLEAN : 
	-@erase "$(INTDIR)\ldapmodrdn.obj"
	-@erase "$(OUTDIR)\ldapmodrdn.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

"$(INTDIR)" :
    if not exist "$(INTDIR)/$(NULL)" mkdir "$(INTDIR)"

CPP=cl.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /YX /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "..\..\dist\WINNT3.51_dbg.obj\include\nspr20\pr" /I "include" /I "..\..\include" /I "..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr" /D "NET_SSL" /D "NDEBUG" /D "_CONSOLE" /D "_WINDOWS" /D "LDAP_DEBUG" /D "LDBM_USE_DBBTREE" /D "NEEDPROTOS" /D "WIN32_KERNEL_THREADS" /D "LDAP_REFERRALS" /D "WINDOWS" /D "XP_PC" /D "XP_WIN32" /D "USE_NSPR_MT" /D "SYSERRLIST_IN_STDIO" /D "WIN32" /YX /c
# SUBTRACT CPP /u
CPP_PROJ=/nologo /MD /W3 /GX /O2 /I\
 "..\..\dist\WINNT3.51_dbg.obj\include\nspr20\pr" /I "include" /I\
 "..\..\include" /I "..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr" /D "NET_SSL"\
 /D "NDEBUG" /D "_CONSOLE" /D "_WINDOWS" /D "LDAP_DEBUG" /D "LDBM_USE_DBBTREE"\
 /D "NEEDPROTOS" /D "WIN32_KERNEL_THREADS" /D "LDAP_REFERRALS" /D "WINDOWS" /D\
 "XP_PC" /D "XP_WIN32" /D "USE_NSPR_MT" /D "SYSERRLIST_IN_STDIO" /D "WIN32"\
 /Fp"$(INTDIR)/ldapmodrdn.pch" /YX /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\release\ldapmodrdn/
CPP_SBRS=.\.

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

RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/ldapmodrdn.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 nssldap32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib nsldap32.lib nslch32.lib rpcrt4.lib ..\..\dist\WINNT4.0_OPT.OBJ\lib\libsec-export.lib ..\..\dist\WINNT4.0_OPT.OBJ\lib\libdbm.lib ..\..\dist\WINNT4.0_OPT.OBJ\lib\libnspr20.lib ..\..\dist\WINNT4.0_OPT.OBJ\lib\libxp.lib libutil.lib /nologo /subsystem:console /machine:I386 /force /LIBPATH:release
# SUBTRACT LINK32 /pdb:none /nodefaultlib
LINK32_FLAGS=nssldap32.lib kernel32.lib user32.lib gdi32.lib winspool.lib\
 comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib\
 odbc32.lib odbccp32.lib wsock32.lib nsldap32.lib nslch32.lib rpcrt4.lib\
 ..\..\dist\WINNT4.0_OPT.OBJ\lib\libsec-export.lib\
 ..\..\dist\WINNT4.0_OPT.OBJ\lib\libdbm.lib\
 ..\..\dist\WINNT4.0_OPT.OBJ\lib\libnspr20.lib\
 ..\..\dist\WINNT4.0_OPT.OBJ\lib\libxp.lib libutil.lib /nologo\
 /subsystem:console /incremental:no /pdb:"$(OUTDIR)/ldapmodrdn.pdb"\
 /machine:I386 /force /out:"$(OUTDIR)/ldapmodrdn.exe" /LIBPATH:release 
LINK32_OBJS= \
	"$(INTDIR)\ldapmodrdn.obj" \
	"$(OUTDIR)\libutil.lib"

"$(OUTDIR)\ldapmodrdn.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "ldapmodrdn - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "ldapmodrdn\Debug"
# PROP BASE Intermediate_Dir "ldapmodrdn\Debug"
# PROP BASE Target_Dir "ldapmodrdn"
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "debug"
# PROP Intermediate_Dir "debug\ldapmodrdn"
# PROP Target_Dir "ldapmodrdn"
OUTDIR=.\debug
INTDIR=.\debug\ldapmodrdn

ALL : "libutil - Win32 Debug" "$(OUTDIR)\ldapmodrdn.exe"

CLEAN : 
	-@erase "$(INTDIR)\ldapmodrdn.obj"
	-@erase "$(INTDIR)\vc40.idb"
	-@erase "$(INTDIR)\vc40.pdb"
	-@erase "$(OUTDIR)\ldapmodrdn.exe"
	-@erase "$(OUTDIR)\ldapmodrdn.ilk"
	-@erase "$(OUTDIR)\ldapmodrdn.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

"$(INTDIR)" :
    if not exist "$(INTDIR)/$(NULL)" mkdir "$(INTDIR)"

CPP=cl.exe
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /YX /c
# ADD CPP /nologo /MD /W3 /Gm /GX /Zi /Od /I "include" /I "..\..\include" /I "..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr" /D "_DEBUG" /D "_CONSOLE" /D "_WINDOWS" /D "LDAP_DEBUG" /D "NET_SSL" /D "LDBM_USE_DBBTREE" /D "NEEDPROTOS" /D "WIN32_KERNEL_THREADS" /D "LDAP_REFERRALS" /D "WINDOWS" /D "XP_PC" /D "XP_WIN32" /D "USE_NSPR_MT" /D "SYSERRLIST_IN_STDIO" /D "WIN32" /YX /c
# SUBTRACT CPP /u
CPP_PROJ=/nologo /MD /W3 /Gm /GX /Zi /Od /I "include" /I "..\..\include" /I\
 "..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr" /D "_DEBUG" /D "_CONSOLE" /D\
 "_WINDOWS" /D "LDAP_DEBUG" /D "NET_SSL" /D "LDBM_USE_DBBTREE" /D "NEEDPROTOS"\
 /D "WIN32_KERNEL_THREADS" /D "LDAP_REFERRALS" /D "WINDOWS" /D "XP_PC" /D\
 "XP_WIN32" /D "USE_NSPR_MT" /D "SYSERRLIST_IN_STDIO" /D "WIN32"\
 /Fp"$(INTDIR)/ldapmodrdn.pch" /YX /Fo"$(INTDIR)/" /Fd"$(INTDIR)/" /c 
CPP_OBJS=.\debug\ldapmodrdn/
CPP_SBRS=.\.

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

RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/ldapmodrdn.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386
# ADD LINK32 nssldap32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib rpcrt4.lib nsldap32.lib nslch32.lib ..\..\dist\WINNT4.0_DBG.OBJ\lib\libsec-export.lib ..\..\dist\WINNT4.0_DBG.OBJ\lib\libdbm.lib ..\..\dist\WINNT4.0_DBG.OBJ\lib\libnspr20.lib ..\..\dist\WINNT4.0_DBG.OBJ\lib\libxp.lib libutil.lib /nologo /subsystem:console /debug /machine:I386 /force /LIBPATH:debug
# SUBTRACT LINK32 /pdb:none /nodefaultlib
LINK32_FLAGS=nssldap32.lib kernel32.lib user32.lib gdi32.lib winspool.lib\
 comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib\
 odbc32.lib odbccp32.lib wsock32.lib rpcrt4.lib nsldap32.lib nslch32.lib\
 ..\..\dist\WINNT4.0_DBG.OBJ\lib\libsec-export.lib\
 ..\..\dist\WINNT4.0_DBG.OBJ\lib\libdbm.lib\
 ..\..\dist\WINNT4.0_DBG.OBJ\lib\libnspr20.lib\
 ..\..\dist\WINNT4.0_DBG.OBJ\lib\libxp.lib libutil.lib /nologo\
 /subsystem:console /incremental:yes /pdb:"$(OUTDIR)/ldapmodrdn.pdb" /debug\
 /machine:I386 /force /out:"$(OUTDIR)/ldapmodrdn.exe" /LIBPATH:debug 
LINK32_OBJS= \
	"$(INTDIR)\ldapmodrdn.obj" \
	"$(OUTDIR)\libutil.lib"

"$(OUTDIR)\ldapmodrdn.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "libldif - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "libldif\Release"
# PROP BASE Intermediate_Dir "libldif\Release"
# PROP BASE Target_Dir "libldif"
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "debug"
# PROP Intermediate_Dir "debug\libldif"
# PROP Target_Dir "libldif"
OUTDIR=.\debug
INTDIR=.\debug\libldif

ALL : "libutil - Win32 Release" "$(OUTDIR)\libldif.lib"

CLEAN : 
	-@erase "$(INTDIR)\line64.obj"
	-@erase "$(OUTDIR)\libldif.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

"$(INTDIR)" :
    if not exist "$(INTDIR)/$(NULL)" mkdir "$(INTDIR)"

CPP=cl.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "include" /I "..\..\include" /I "o:\ns\include" /I "o:\ns\dist\public\nspr" /D "NDEBUG" /D "_WINDOWS" /D "LDAP_DEBUG" /D "LDBM_USE_DBBTREE" /D "NEEDPROTOS" /D "WIN32_KERNEL_THREADS" /D "LDAP_REFERRALS" /D "WINDOWS" /D "XP_PC" /D "XP_WIN32" /D "USE_NSPR_MT" /D "SYSERRLIST_IN_STDIO" /D "WIN32" /D "NET_SSL" /YX /c
# SUBTRACT CPP /u
CPP_PROJ=/nologo /MD /W3 /GX /O2 /I "include" /I "..\..\include" /I\
 "o:\ns\include" /I "o:\ns\dist\public\nspr" /D "NDEBUG" /D "_WINDOWS" /D\
 "LDAP_DEBUG" /D "LDBM_USE_DBBTREE" /D "NEEDPROTOS" /D "WIN32_KERNEL_THREADS" /D\
 "LDAP_REFERRALS" /D "WINDOWS" /D "XP_PC" /D "XP_WIN32" /D "USE_NSPR_MT" /D\
 "SYSERRLIST_IN_STDIO" /D "WIN32" /D "NET_SSL" /Fp"$(INTDIR)/libldif.pch" /YX\
 /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\debug\libldif/
CPP_SBRS=.\.

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

BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/libldif.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo
LIB32_FLAGS=/nologo /out:"$(OUTDIR)/libldif.lib" 
LIB32_OBJS= \
	"$(INTDIR)\line64.obj" \
	".\release\libutil.lib"

"$(OUTDIR)\libldif.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ELSEIF  "$(CFG)" == "libldif - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "libldif\Debug"
# PROP BASE Intermediate_Dir "libldif\Debug"
# PROP BASE Target_Dir "libldif"
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "debug"
# PROP Intermediate_Dir "debug\libldif"
# PROP Target_Dir "libldif"
OUTDIR=.\debug
INTDIR=.\debug\libldif

ALL : "libutil - Win32 Debug" "$(OUTDIR)\libldif.lib"

CLEAN : 
	-@erase "$(INTDIR)\line64.obj"
	-@erase "$(OUTDIR)\libldif.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

"$(INTDIR)" :
    if not exist "$(INTDIR)/$(NULL)" mkdir "$(INTDIR)"

CPP=cl.exe
# ADD BASE CPP /nologo /W3 /GX /Z7 /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MD /W3 /GX /Z7 /Od /I "include" /I "..\..\include" /I "o:\ns\include" /I "o:\ns\dist\public\nspr" /D "_DEBUG" /D "_WINDOWS" /D "LDAP_DEBUG" /D "LDBM_USE_DBBTREE" /D "NEEDPROTOS" /D "WIN32_KERNEL_THREADS" /D "LDAP_REFERRALS" /D "WINDOWS" /D "XP_PC" /D "XP_WIN32" /D "USE_NSPR_MT" /D "SYSERRLIST_IN_STDIO" /D "WIN32" /D "NET_SSL" /YX /c
# SUBTRACT CPP /u
CPP_PROJ=/nologo /MD /W3 /GX /Z7 /Od /I "include" /I "..\..\include" /I\
 "o:\ns\include" /I "o:\ns\dist\public\nspr" /D "_DEBUG" /D "_WINDOWS" /D\
 "LDAP_DEBUG" /D "LDBM_USE_DBBTREE" /D "NEEDPROTOS" /D "WIN32_KERNEL_THREADS" /D\
 "LDAP_REFERRALS" /D "WINDOWS" /D "XP_PC" /D "XP_WIN32" /D "USE_NSPR_MT" /D\
 "SYSERRLIST_IN_STDIO" /D "WIN32" /D "NET_SSL" /Fp"$(INTDIR)/libldif.pch" /YX\
 /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\debug\libldif/
CPP_SBRS=.\.

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

BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/libldif.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo
LIB32_FLAGS=/nologo /out:"$(OUTDIR)/libldif.lib" 
LIB32_OBJS= \
	"$(INTDIR)\line64.obj" \
	"$(OUTDIR)\libutil.lib"

"$(OUTDIR)\libldif.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ENDIF 

################################################################################
# Begin Target

# Name "ldapdelete - Win32 Release"
# Name "ldapdelete - Win32 Debug"

!IF  "$(CFG)" == "ldapdelete - Win32 Release"

!ELSEIF  "$(CFG)" == "ldapdelete - Win32 Debug"

!ENDIF 

################################################################################
# Begin Source File

SOURCE=.\clients\tools\ldapdelete.c

!IF  "$(CFG)" == "ldapdelete - Win32 Release"

DEP_CPP_LDAPD=\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\md/_unixos.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\md/sunos4.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\md\prcpucfg.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\obsolete\protypes.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prarena.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prcpucfg.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prinrval.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prio.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prlong.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prmem.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prprf.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prtime.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prtypes.h"\
	"..\..\include\cdefs.h"\
	"..\..\include\ds.h"\
	"..\..\include\mcom_db.h"\
	"..\..\include\sec.h"\
	"..\..\include\secerr.h"\
	"..\..\include\winfile.h"\
	"..\..\include\xp_core.h"\
	"..\..\include\xp_debug.h"\
	"..\..\include\xp_error.h"\
	"..\..\include\xp_file.h"\
	"..\..\include\xp_list.h"\
	"..\..\include\xp_mcom.h"\
	"..\..\include\xp_mem.h"\
	"..\..\include\xp_str.h"\
	"..\..\include\xp_trace.h"\
	"..\..\include\xpassert.h"\
	".\include\lber.h"\
	".\include\ldap.h"\
	".\include\ldap_ssl.h"\
	".\include\ldaplog.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_LDAPD=\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\protypes.h"\
	"..\..\include\macmem.h"\
	"..\..\include\prmacos.h"\
	

"$(INTDIR)\ldapdelete.obj" : $(SOURCE) $(DEP_CPP_LDAPD) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "ldapdelete - Win32 Debug"

DEP_CPP_LDAPD=\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\md/_unixos.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\md/sunos4.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\md\prcpucfg.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\obsolete\protypes.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prarena.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prcpucfg.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prinrval.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prio.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prlong.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prmem.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prprf.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prtime.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prtypes.h"\
	"..\..\include\cdefs.h"\
	"..\..\include\ds.h"\
	"..\..\include\mcom_db.h"\
	"..\..\include\sec.h"\
	"..\..\include\secerr.h"\
	"..\..\include\winfile.h"\
	"..\..\include\xp_core.h"\
	"..\..\include\xp_debug.h"\
	"..\..\include\xp_error.h"\
	"..\..\include\xp_file.h"\
	"..\..\include\xp_list.h"\
	"..\..\include\xp_mcom.h"\
	"..\..\include\xp_mem.h"\
	"..\..\include\xp_str.h"\
	"..\..\include\xp_trace.h"\
	"..\..\include\xpassert.h"\
	".\include\lber.h"\
	".\include\ldap.h"\
	".\include\ldap_ssl.h"\
	".\include\ldaplog.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_LDAPD=\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\protypes.h"\
	"..\..\include\macmem.h"\
	"..\..\include\prmacos.h"\
	

"$(INTDIR)\ldapdelete.obj" : $(SOURCE) $(DEP_CPP_LDAPD) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Project Dependency

# Project_Dep_Name "libutil"

!IF  "$(CFG)" == "ldapdelete - Win32 Release"

"libutil - Win32 Release" : 
   $(MAKE) /$(MAKEFLAGS) /F ".\tools.mak" CFG="libutil - Win32 Release" 

!ELSEIF  "$(CFG)" == "ldapdelete - Win32 Debug"

"libutil - Win32 Debug" : 
   $(MAKE) /$(MAKEFLAGS) /F ".\tools.mak" CFG="libutil - Win32 Debug" 

!ENDIF 

# End Project Dependency
# End Target
################################################################################
# Begin Target

# Name "libutil - Win32 Release"
# Name "libutil - Win32 Debug"

!IF  "$(CFG)" == "libutil - Win32 Release"

!ELSEIF  "$(CFG)" == "libutil - Win32 Debug"

!ENDIF 

################################################################################
# Begin Source File

SOURCE=.\libraries\libutil\getopt.c
DEP_CPP_GETOP=\
	".\include\lber.h"\
	

"$(INTDIR)\getopt.obj" : $(SOURCE) $(DEP_CPP_GETOP) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\libraries\libutil\ntdebug.c
DEP_CPP_NTDEB=\
	".\include\lber.h"\
	".\include\ldap.h"\
	".\include\ldaplog.h"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_NTDEB=\
	".\libraries\libutil\proto-slap.h"\
	".\libraries\libutil\slap.h"\
	

"$(INTDIR)\ntdebug.obj" : $(SOURCE) $(DEP_CPP_NTDEB) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\libraries\libutil\ntstubs.c

"$(INTDIR)\ntstubs.obj" : $(SOURCE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
# End Target
################################################################################
# Begin Target

# Name "ldapmodify - Win32 Release"
# Name "ldapmodify - Win32 Debug"

!IF  "$(CFG)" == "ldapmodify - Win32 Release"

!ELSEIF  "$(CFG)" == "ldapmodify - Win32 Debug"

!ENDIF 

################################################################################
# Begin Project Dependency

# Project_Dep_Name "libutil"

!IF  "$(CFG)" == "ldapmodify - Win32 Release"

"libutil - Win32 Release" : 
   $(MAKE) /$(MAKEFLAGS) /F ".\tools.mak" CFG="libutil - Win32 Release" 

!ELSEIF  "$(CFG)" == "ldapmodify - Win32 Debug"

"libutil - Win32 Debug" : 
   $(MAKE) /$(MAKEFLAGS) /F ".\tools.mak" CFG="libutil - Win32 Debug" 

!ENDIF 

# End Project Dependency
################################################################################
# Begin Source File

SOURCE=.\clients\tools\ldapmodify.c

!IF  "$(CFG)" == "ldapmodify - Win32 Release"

DEP_CPP_LDAPM=\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\md/_unixos.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\md/sunos4.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\md\prcpucfg.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\obsolete\protypes.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prarena.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prcpucfg.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prinrval.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prio.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prlong.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prmem.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prprf.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prtime.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prtypes.h"\
	"..\..\include\cdefs.h"\
	"..\..\include\ds.h"\
	"..\..\include\mcom_db.h"\
	"..\..\include\sec.h"\
	"..\..\include\secerr.h"\
	"..\..\include\winfile.h"\
	"..\..\include\xp_core.h"\
	"..\..\include\xp_debug.h"\
	"..\..\include\xp_error.h"\
	"..\..\include\xp_file.h"\
	"..\..\include\xp_list.h"\
	"..\..\include\xp_mcom.h"\
	"..\..\include\xp_mem.h"\
	"..\..\include\xp_str.h"\
	"..\..\include\xp_trace.h"\
	"..\..\include\xpassert.h"\
	".\include\lber.h"\
	".\include\ldap.h"\
	".\include\ldap_ssl.h"\
	".\include\ldaplog.h"\
	".\include\ldif.h"\
	".\include\portable.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_LDAPM=\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\protypes.h"\
	"..\..\include\macmem.h"\
	"..\..\include\prmacos.h"\
	

"$(INTDIR)\ldapmodify.obj" : $(SOURCE) $(DEP_CPP_LDAPM) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "ldapmodify - Win32 Debug"

DEP_CPP_LDAPM=\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\md/_unixos.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\md/sunos4.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\md\prcpucfg.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\obsolete\protypes.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prarena.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prcpucfg.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prinrval.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prio.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prlong.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prmem.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prprf.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prtime.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prtypes.h"\
	"..\..\include\cdefs.h"\
	"..\..\include\ds.h"\
	"..\..\include\mcom_db.h"\
	"..\..\include\sec.h"\
	"..\..\include\secerr.h"\
	"..\..\include\winfile.h"\
	"..\..\include\xp_core.h"\
	"..\..\include\xp_debug.h"\
	"..\..\include\xp_error.h"\
	"..\..\include\xp_file.h"\
	"..\..\include\xp_list.h"\
	"..\..\include\xp_mcom.h"\
	"..\..\include\xp_mem.h"\
	"..\..\include\xp_str.h"\
	"..\..\include\xp_trace.h"\
	"..\..\include\xpassert.h"\
	".\include\lber.h"\
	".\include\ldap.h"\
	".\include\ldap_ssl.h"\
	".\include\ldaplog.h"\
	".\include\ldif.h"\
	".\include\portable.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_LDAPM=\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\protypes.h"\
	"..\..\include\macmem.h"\
	"..\..\include\prmacos.h"\
	

"$(INTDIR)\ldapmodify.obj" : $(SOURCE) $(DEP_CPP_LDAPM) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Project Dependency

# Project_Dep_Name "libldif"

!IF  "$(CFG)" == "ldapmodify - Win32 Release"

"libldif - Win32 Release" : 
   $(MAKE) /$(MAKEFLAGS) /F ".\tools.mak" CFG="libldif - Win32 Release" 

!ELSEIF  "$(CFG)" == "ldapmodify - Win32 Debug"

"libldif - Win32 Debug" : 
   $(MAKE) /$(MAKEFLAGS) /F ".\tools.mak" CFG="libldif - Win32 Debug" 

!ENDIF 

# End Project Dependency
# End Target
################################################################################
# Begin Target

# Name "ldapsearch - Win32 Release"
# Name "ldapsearch - Win32 Debug"

!IF  "$(CFG)" == "ldapsearch - Win32 Release"

!ELSEIF  "$(CFG)" == "ldapsearch - Win32 Debug"

!ENDIF 

################################################################################
# Begin Source File

SOURCE=.\clients\tools\ldapsearch.c

!IF  "$(CFG)" == "ldapsearch - Win32 Release"

DEP_CPP_LDAPS=\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\md/_unixos.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\md/sunos4.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\md\prcpucfg.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\obsolete\protypes.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prarena.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prcpucfg.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prinrval.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prio.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prlong.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prmem.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prprf.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prtime.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prtypes.h"\
	"..\..\include\cdefs.h"\
	"..\..\include\ds.h"\
	"..\..\include\mcom_db.h"\
	"..\..\include\sec.h"\
	"..\..\include\secerr.h"\
	"..\..\include\winfile.h"\
	"..\..\include\xp_core.h"\
	"..\..\include\xp_debug.h"\
	"..\..\include\xp_error.h"\
	"..\..\include\xp_file.h"\
	"..\..\include\xp_list.h"\
	"..\..\include\xp_mcom.h"\
	"..\..\include\xp_mem.h"\
	"..\..\include\xp_str.h"\
	"..\..\include\xp_trace.h"\
	"..\..\include\xpassert.h"\
	".\include\lber.h"\
	".\include\ldap.h"\
	".\include\ldap_ssl.h"\
	".\include\ldaplog.h"\
	".\include\ldif.h"\
	".\include\portable.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_LDAPS=\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\protypes.h"\
	"..\..\include\macmem.h"\
	"..\..\include\prmacos.h"\
	

"$(INTDIR)\ldapsearch.obj" : $(SOURCE) $(DEP_CPP_LDAPS) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "ldapsearch - Win32 Debug"

DEP_CPP_LDAPS=\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\md/_unixos.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\md/sunos4.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\md\prcpucfg.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\obsolete\protypes.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prarena.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prcpucfg.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prinrval.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prio.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prlong.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prmem.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prprf.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prtime.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prtypes.h"\
	"..\..\include\cdefs.h"\
	"..\..\include\ds.h"\
	"..\..\include\mcom_db.h"\
	"..\..\include\sec.h"\
	"..\..\include\secerr.h"\
	"..\..\include\winfile.h"\
	"..\..\include\xp_core.h"\
	"..\..\include\xp_debug.h"\
	"..\..\include\xp_error.h"\
	"..\..\include\xp_file.h"\
	"..\..\include\xp_list.h"\
	"..\..\include\xp_mcom.h"\
	"..\..\include\xp_mem.h"\
	"..\..\include\xp_str.h"\
	"..\..\include\xp_trace.h"\
	"..\..\include\xpassert.h"\
	".\include\lber.h"\
	".\include\ldap.h"\
	".\include\ldap_ssl.h"\
	".\include\ldaplog.h"\
	".\include\ldif.h"\
	".\include\portable.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_LDAPS=\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\protypes.h"\
	"..\..\include\macmem.h"\
	"..\..\include\prmacos.h"\
	

"$(INTDIR)\ldapsearch.obj" : $(SOURCE) $(DEP_CPP_LDAPS) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Project Dependency

# Project_Dep_Name "libutil"

!IF  "$(CFG)" == "ldapsearch - Win32 Release"

"libutil - Win32 Release" : 
   $(MAKE) /$(MAKEFLAGS) /F ".\tools.mak" CFG="libutil - Win32 Release" 

!ELSEIF  "$(CFG)" == "ldapsearch - Win32 Debug"

"libutil - Win32 Debug" : 
   $(MAKE) /$(MAKEFLAGS) /F ".\tools.mak" CFG="libutil - Win32 Debug" 

!ENDIF 

# End Project Dependency
################################################################################
# Begin Project Dependency

# Project_Dep_Name "libldif"

!IF  "$(CFG)" == "ldapsearch - Win32 Release"

"libldif - Win32 Release" : 
   $(MAKE) /$(MAKEFLAGS) /F ".\tools.mak" CFG="libldif - Win32 Release" 

!ELSEIF  "$(CFG)" == "ldapsearch - Win32 Debug"

"libldif - Win32 Debug" : 
   $(MAKE) /$(MAKEFLAGS) /F ".\tools.mak" CFG="libldif - Win32 Debug" 

!ENDIF 

# End Project Dependency
# End Target
################################################################################
# Begin Target

# Name "ldapmodrdn - Win32 Release"
# Name "ldapmodrdn - Win32 Debug"

!IF  "$(CFG)" == "ldapmodrdn - Win32 Release"

!ELSEIF  "$(CFG)" == "ldapmodrdn - Win32 Debug"

!ENDIF 

################################################################################
# Begin Source File

SOURCE=.\clients\tools\ldapmodrdn.c

!IF  "$(CFG)" == "ldapmodrdn - Win32 Release"

DEP_CPP_LDAPMO=\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\md/_unixos.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\md/sunos4.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\md\prcpucfg.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\obsolete\protypes.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prarena.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prcpucfg.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prinrval.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prio.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prlong.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prmem.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prprf.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prtime.h"\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\prtypes.h"\
	"..\..\include\cdefs.h"\
	"..\..\include\ds.h"\
	"..\..\include\mcom_db.h"\
	"..\..\include\sec.h"\
	"..\..\include\secerr.h"\
	"..\..\include\winfile.h"\
	"..\..\include\xp_core.h"\
	"..\..\include\xp_debug.h"\
	"..\..\include\xp_error.h"\
	"..\..\include\xp_file.h"\
	"..\..\include\xp_list.h"\
	"..\..\include\xp_mcom.h"\
	"..\..\include\xp_mem.h"\
	"..\..\include\xp_str.h"\
	"..\..\include\xp_trace.h"\
	"..\..\include\xpassert.h"\
	".\include\lber.h"\
	".\include\ldap.h"\
	".\include\ldap_ssl.h"\
	".\include\ldaplog.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_LDAPMO=\
	"..\..\dist\WINNT4.0_OPT.OBJ\include\nspr20\pr\protypes.h"\
	"..\..\include\macmem.h"\
	"..\..\include\prmacos.h"\
	

"$(INTDIR)\ldapmodrdn.obj" : $(SOURCE) $(DEP_CPP_LDAPMO) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "ldapmodrdn - Win32 Debug"

DEP_CPP_LDAPMO=\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\md/_unixos.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\md/sunos4.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\md\prcpucfg.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\obsolete\protypes.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prarena.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prcpucfg.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prinrval.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prio.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prlong.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prmem.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prprf.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prtime.h"\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\prtypes.h"\
	"..\..\include\cdefs.h"\
	"..\..\include\ds.h"\
	"..\..\include\mcom_db.h"\
	"..\..\include\sec.h"\
	"..\..\include\secerr.h"\
	"..\..\include\winfile.h"\
	"..\..\include\xp_core.h"\
	"..\..\include\xp_debug.h"\
	"..\..\include\xp_error.h"\
	"..\..\include\xp_file.h"\
	"..\..\include\xp_list.h"\
	"..\..\include\xp_mcom.h"\
	"..\..\include\xp_mem.h"\
	"..\..\include\xp_str.h"\
	"..\..\include\xp_trace.h"\
	"..\..\include\xpassert.h"\
	".\include\lber.h"\
	".\include\ldap.h"\
	".\include\ldap_ssl.h"\
	".\include\ldaplog.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_LDAPMO=\
	"..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr\protypes.h"\
	"..\..\include\macmem.h"\
	"..\..\include\prmacos.h"\
	

"$(INTDIR)\ldapmodrdn.obj" : $(SOURCE) $(DEP_CPP_LDAPMO) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Project Dependency

# Project_Dep_Name "libutil"

!IF  "$(CFG)" == "ldapmodrdn - Win32 Release"

"libutil - Win32 Release" : 
   $(MAKE) /$(MAKEFLAGS) /F ".\tools.mak" CFG="libutil - Win32 Release" 

!ELSEIF  "$(CFG)" == "ldapmodrdn - Win32 Debug"

"libutil - Win32 Debug" : 
   $(MAKE) /$(MAKEFLAGS) /F ".\tools.mak" CFG="libutil - Win32 Debug" 

!ENDIF 

# End Project Dependency
# End Target
################################################################################
# Begin Target

# Name "libldif - Win32 Release"
# Name "libldif - Win32 Debug"

!IF  "$(CFG)" == "libldif - Win32 Release"

!ELSEIF  "$(CFG)" == "libldif - Win32 Debug"

!ENDIF 

################################################################################
# Begin Source File

SOURCE=.\libraries\libldif\line64.c
DEP_CPP_LINE6=\
	".\include\ldaplog.h"\
	".\include\ldif.h"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

"$(INTDIR)\line64.obj" : $(SOURCE) $(DEP_CPP_LINE6) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Project Dependency

# Project_Dep_Name "libutil"

!IF  "$(CFG)" == "libldif - Win32 Release"

"libutil - Win32 Release" : 
   $(MAKE) /$(MAKEFLAGS) /F ".\tools.mak" CFG="libutil - Win32 Release" 

!ELSEIF  "$(CFG)" == "libldif - Win32 Debug"

"libutil - Win32 Debug" : 
   $(MAKE) /$(MAKEFLAGS) /F ".\tools.mak" CFG="libutil - Win32 Debug" 

!ENDIF 

# End Project Dependency
# End Target
# End Project
################################################################################
