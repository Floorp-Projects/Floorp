# Microsoft Developer Studio Project File - Name="fdlibm" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=fdlibm - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "fdlibm.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "fdlibm.mak" CFG="fdlibm - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "fdlibm - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "fdlibm - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "fdlibm - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /D "NDEBUG" /D "_LIB" /D "_IEEE_LIBM" /D "XP_PC" /D "WIN32" /D "_MBCS" /D "DIKDIK" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "fdlibm - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "fdlibm___Win32_Debug"
# PROP BASE Intermediate_Dir "fdlibm___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "fdlibm___Win32_Debug"
# PROP Intermediate_Dir "fdlibm___Win32_Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /D "_DEBUG" /D "_IEEE_LIBM" /D "_LIB" /D "XP_PC" /D "WIN32" /D "_MBCS" /D "DIKDIK" /FR /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "fdlibm - Win32 Release"
# Name "fdlibm - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\js\src\fdlibm\e_atan2.c
# End Source File
# Begin Source File

SOURCE=..\..\..\js\src\fdlibm\e_fmod.c
# End Source File
# Begin Source File

SOURCE=..\..\..\js\src\fdlibm\e_pow.c
# End Source File
# Begin Source File

SOURCE=..\..\..\js\src\fdlibm\e_sqrt.c
# End Source File
# Begin Source File

SOURCE=..\..\..\js\src\fdlibm\s_atan.c
# End Source File
# Begin Source File

SOURCE=..\..\..\js\src\fdlibm\s_copysign.c
# End Source File
# Begin Source File

SOURCE=..\..\..\js\src\fdlibm\s_fabs.c
# End Source File
# Begin Source File

SOURCE=..\..\..\js\src\fdlibm\s_finite.c
# End Source File
# Begin Source File

SOURCE=..\..\..\js\src\fdlibm\s_isnan.c
# End Source File
# Begin Source File

SOURCE=..\..\..\js\src\fdlibm\s_matherr.c
# End Source File
# Begin Source File

SOURCE=..\..\..\js\src\fdlibm\s_rint.c
# End Source File
# Begin Source File

SOURCE=..\..\..\js\src\fdlibm\s_scalbn.c
# End Source File
# Begin Source File

SOURCE=..\..\..\js\src\fdlibm\w_atan2.c
# End Source File
# Begin Source File

SOURCE=..\..\..\js\src\fdlibm\w_pow.c
# End Source File
# Begin Source File

SOURCE=..\..\..\js\src\fdlibm\w_sqrt.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# End Target
# End Project
