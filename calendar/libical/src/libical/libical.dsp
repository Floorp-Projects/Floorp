# Microsoft Developer Studio Project File - Name="libical" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=libical - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libical.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libical.mak" CFG="libical - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libical - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libical - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libical - Win32 Release"

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
# ADD CPP /nologo /MD /W3 /GX /O2 /I "./autogenex" /I "./" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "libical - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "." /I ".." /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /FR /YX /FD /GZ /c
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

# Name "libical - Win32 Release"
# Name "libical - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\caldate.c
# End Source File
# Begin Source File

SOURCE=.\icalarray.c
# End Source File
# Begin Source File

SOURCE=.\icalattach.c
# End Source File
# Begin Source File

SOURCE=icalcomponent.c
# End Source File
# Begin Source File

SOURCE=.\icalderivedparameter.c
# End Source File
# Begin Source File

SOURCE=icalderivedparameter.c.in

!IF  "$(CFG)" == "libical - Win32 Release"

# PROP Ignore_Default_Tool 1
USERDEP__ICALD="../../scripts/mkderivedparameters.pl"	"../../design-data/parameters.csv"	
# Begin Custom Build
InputPath=icalderivedparameter.c.in
InputName=icalderivedparameter.c

"$(InputName)" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	perl -I ../../scripts ../../scripts/mkderivedparameters.pl -i $(InputName).in -c ../../design-data/parameters.csv > $(InputName)

# End Custom Build

!ELSEIF  "$(CFG)" == "libical - Win32 Debug"

USERDEP__ICALD="../../scripts/mkderivedparameters.pl"	"../../design-data/parameters.csv"	
# Begin Custom Build
InputPath=icalderivedparameter.c.in
InputName=icalderivedparameter.c

"$(InputName)" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	perl -I ../../scripts ../../scripts/mkderivedparameters.pl -i $(InputName).in -c ../../design-data/parameters.csv > $(InputName)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\icalderivedproperty.c
# End Source File
# Begin Source File

SOURCE=icalderivedproperty.c.in

!IF  "$(CFG)" == "libical - Win32 Release"

# PROP Ignore_Default_Tool 1
USERDEP__ICALDE="../../scripts/mkderivedproperties.pl"	"../../design-data/properties.csv"	"../../design-data/value-types.csv"	
# Begin Custom Build
InputPath=icalderivedproperty.c.in
InputName=icalderivedproperty.c

"$(InputName)" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	perl -I ../../scripts ../../scripts/mkderivedproperties.pl -i $(InputName).in -c ../../design-data/properties.csv ../../design-data/value-types.csv > $(InputName)

# End Custom Build

!ELSEIF  "$(CFG)" == "libical - Win32 Debug"

USERDEP__ICALDE="../../scripts/mkderivedproperties.pl"	"../../design-data/properties.csv"	"../../design-data/value-types.csv"	
# Begin Custom Build
InputPath=icalderivedproperty.c.in
InputName=icalderivedproperty.c

"$(InputName)" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	perl -I ../../scripts ../../scripts/mkderivedproperties.pl -i $(InputName).in -c ../../design-data/properties.csv ../../design-data/value-types.csv > $(InputName)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\icalderivedvalue.c
# End Source File
# Begin Source File

SOURCE=icalderivedvalue.c.in

!IF  "$(CFG)" == "libical - Win32 Release"

# PROP Ignore_Default_Tool 1
USERDEP__ICALDER="../../scripts/mkderivedvalues.pl"	"../../design-data/value-types.csv"	
# Begin Custom Build
InputPath=icalderivedvalue.c.in
InputName=icalderivedvalue.c

"$(InputName)" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	perl -I ../../scripts ../../scripts/mkderivedvalues.pl -i $(InputName).in -c ../../design-data/value-types.csv > $(InputName)

# End Custom Build

!ELSEIF  "$(CFG)" == "libical - Win32 Debug"

USERDEP__ICALDER="../../scripts/mkderivedvalues.pl"	"../../design-data/value-types.csv"	
# Begin Custom Build
InputPath=icalderivedvalue.c.in
InputName=icalderivedvalue.c

"$(InputName)" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	perl -I ../../scripts ../../scripts/mkderivedvalues.pl -i $(InputName).in -c ../../design-data/value-types.csv > $(InputName)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=icalduration.c
# End Source File
# Begin Source File

SOURCE=icalenums.c
# End Source File
# Begin Source File

SOURCE=icalerror.c
# End Source File
# Begin Source File

SOURCE=icallangbind.c
# End Source File
# Begin Source File

SOURCE=icalmemory.c
# End Source File
# Begin Source File

SOURCE=icalmime.c
# End Source File
# Begin Source File

SOURCE=icalparameter.c
# End Source File
# Begin Source File

SOURCE=icalparser.c
# End Source File
# Begin Source File

SOURCE=icalperiod.c
# End Source File
# Begin Source File

SOURCE=icalproperty.c
# End Source File
# Begin Source File

SOURCE=icalrecur.c
# End Source File
# Begin Source File

SOURCE=.\icalrestriction.c
# End Source File
# Begin Source File

SOURCE=icalrestriction.c.in

!IF  "$(CFG)" == "libical - Win32 Release"

# PROP Ignore_Default_Tool 1
USERDEP__ICALR="../../scripts/mkrestrictiontable.pl"	"../../design-data/restrictions.csv"	
# Begin Custom Build
InputPath=icalrestriction.c.in
InputName=icalrestriction.c

"$(InputName)" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	perl -I ../../scripts ../../scripts/mkrestrictiontable.pl -i $(InputName).in ../../design-data/restrictions.csv > $(InputName)

# End Custom Build

!ELSEIF  "$(CFG)" == "libical - Win32 Debug"

# PROP Ignore_Default_Tool 1
USERDEP__ICALR="../../scripts/mkrestrictiontable.pl"	"../../design-data/restrictions.csv"	
# Begin Custom Build
InputPath=icalrestriction.c.in
InputName=icalrestriction.c

"$(InputName)" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	perl -I ../../scripts ../../scripts/mkrestrictiontable.pl -i $(InputName).in ../../design-data/restrictions.csv > $(InputName)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=icaltime.c
# End Source File
# Begin Source File

SOURCE=.\icaltimezone.c
# End Source File
# Begin Source File

SOURCE=icaltypes.c
# End Source File
# Begin Source File

SOURCE=icalvalue.c
# End Source File
# Begin Source File

SOURCE=pvl.c
# End Source File
# Begin Source File

SOURCE=sspm.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\astime.h
# End Source File
# Begin Source File

SOURCE=.\ical.h

!IF  "$(CFG)" == "libical - Win32 Release"

# PROP Ignore_Default_Tool 1
USERDEP__ICAL_="icalversion.h"	"icaltime.h"	"icalduration.h"	"icalperiod.h"	"icalenums.h"	"icaltypes.h"	"icalrecur.h"	"icalattach.h"	"icalderivedvalue.h"	"icalderivedparameter.h"	"icalvalue.h"	"icalparameter.h"	"icalderivedproperty.h"	"icalproperty.h"	"pvl.h"	"icalarray.h"	"icalcomponent.h"	"icaltimezone.h"	"icalparser.h"	"icalmemory.h"	"icalerror.h"	"icalrestriction.h"	"sspm.h"	"icalmime.h"	"icallangbind.h"	
# Begin Custom Build
InputPath=.\ical.h

"ical.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	sh ../../scripts/mkinclude.sh -o ical.h -e "#include.*\"ical" -e "#include.*\"config" -e "#include.*\"pvl\.h\"" -e '\$$(Id|Locker): .+\$$' icalversion.h icaltime.h icalduration.h icalperiod.h icalenums.h icaltypes.h icalrecur.h icalattach.h icalderivedvalue.h icalderivedparameter.h icalvalue.h icalparameter.h icalderivedproperty.h icalproperty.h pvl.h icalarray.h icalcomponent.h icaltimezone.h icalparser.h icalmemory.h icalerror.h icalrestriction.h sspm.h icalmime.h icallangbind.h

# End Custom Build

!ELSEIF  "$(CFG)" == "libical - Win32 Debug"

USERDEP__ICAL_="icalversion.h"	"icaltime.h"	"icalduration.h"	"icalperiod.h"	"icalenums.h"	"icaltypes.h"	"icalrecur.h"	"icalattach.h"	"icalderivedvalue.h"	"icalderivedparameter.h"	"icalvalue.h"	"icalparameter.h"	"icalderivedproperty.h"	"icalproperty.h"	"pvl.h"	"icalarray.h"	"icalcomponent.h"	"icaltimezone.h"	"icalparser.h"	"icalmemory.h"	"icalerror.h"	"icalrestriction.h"	"sspm.h"	"icalmime.h"	"icallangbind.h"	
# Begin Custom Build
InputPath=.\ical.h

"ical.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	sh ../../scripts/mkinclude.sh -o ical.h -e "#include.*\"ical" -e "#include.*\"config" -e "#include.*\"pvl\.h\"" -e '\$$(Id|Locker): .+\$$' icalversion.h icaltime.h icalduration.h icalperiod.h icalenums.h icaltypes.h icalrecur.h icalattach.h icalderivedvalue.h icalderivedparameter.h icalvalue.h icalparameter.h icalderivedproperty.h icalproperty.h pvl.h icalarray.h icalcomponent.h icaltimezone.h icalparser.h icalmemory.h icalerror.h icalrestriction.h sspm.h icalmime.h icallangbind.h

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\icalarray.h
# End Source File
# Begin Source File

SOURCE=.\icalattach.h
# End Source File
# Begin Source File

SOURCE=.\icalattachimpl.h
# End Source File
# Begin Source File

SOURCE=icalcomponent.h
# End Source File
# Begin Source File

SOURCE=.\icalderivedparameter.h
# End Source File
# Begin Source File

SOURCE=icalderivedparameter.h.in

!IF  "$(CFG)" == "libical - Win32 Release"

# PROP Ignore_Default_Tool 1
USERDEP__ICALDERI="../../scripts/mkderivedparameters.pl"	"../../design-data/parameters.csv"	
# Begin Custom Build
InputPath=icalderivedparameter.h.in
InputName=icalderivedparameter.h

"$(InputName)" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	perl -I ../../scripts ../../scripts/mkderivedparameters.pl -i $(InputName).in -h ../../design-data/parameters.csv > $(InputName)

# End Custom Build

!ELSEIF  "$(CFG)" == "libical - Win32 Debug"

USERDEP__ICALDERI="../../scripts/mkderivedparameters.pl"	"../../design-data/parameters.csv"	
# Begin Custom Build
InputPath=icalderivedparameter.h.in
InputName=icalderivedparameter.h

"$(InputName)" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	perl -I ../../scripts ../../scripts/mkderivedparameters.pl -i $(InputName).in -h ../../design-data/parameters.csv > $(InputName)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\icalderivedproperty.h
# End Source File
# Begin Source File

SOURCE=icalderivedproperty.h.in

!IF  "$(CFG)" == "libical - Win32 Release"

# PROP Ignore_Default_Tool 1
USERDEP__ICALDERIV="../../scripts/mkderivedproperties.pl"	"../../design-data/properties.csv"	"../../design-data/value-types.csv"	
# Begin Custom Build
InputPath=icalderivedproperty.h.in
InputName=icalderivedproperty.h

"$(InputName)" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	perl -I ../../scripts ../../scripts/mkderivedproperties.pl -i $(InputName).in -h ../../design-data/properties.csv ../../design-data/value-types.csv > $(InputName)

# End Custom Build

!ELSEIF  "$(CFG)" == "libical - Win32 Debug"

USERDEP__ICALDERIV="../../scripts/mkderivedproperties.pl"	"../../design-data/properties.csv"	"../../design-data/value-types.csv"	
# Begin Custom Build
InputPath=icalderivedproperty.h.in
InputName=icalderivedproperty.h

"$(InputName)" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	perl -I ../../scripts ../../scripts/mkderivedproperties.pl -i $(InputName).in -h ../../design-data/properties.csv ../../design-data/value-types.csv > $(InputName)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\icalderivedvalue.h
# End Source File
# Begin Source File

SOURCE=icalderivedvalue.h.in

!IF  "$(CFG)" == "libical - Win32 Release"

# PROP Ignore_Default_Tool 1
USERDEP__ICALDERIVE="../../scripts/mkderivedvalues.pl"	"../../design-data/value-types.csv"	
# Begin Custom Build
InputPath=icalderivedvalue.h.in
InputName=icalderivedvalue.h

"$(InputName)" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	perl -I ../../scripts ../../scripts/mkderivedvalues.pl -i $(InputName).in -h ../../design-data/value-types.csv > $(InputName)

# End Custom Build

!ELSEIF  "$(CFG)" == "libical - Win32 Debug"

USERDEP__ICALDERIVE="../../scripts/mkderivedvalues.pl"	"../../design-data/value-types.csv"	
# Begin Custom Build
InputPath=icalderivedvalue.h.in
InputName=icalderivedvalue.h

"$(InputName)" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	perl -I ../../scripts ../../scripts/mkderivedvalues.pl -i $(InputName).in -h ../../design-data/value-types.csv > $(InputName)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=icalduration.h
# End Source File
# Begin Source File

SOURCE=icalenums.h
# End Source File
# Begin Source File

SOURCE=icalerror.h
# End Source File
# Begin Source File

SOURCE=icallangbind.h
# End Source File
# Begin Source File

SOURCE=icalmemory.h
# End Source File
# Begin Source File

SOURCE=icalmime.h
# End Source File
# Begin Source File

SOURCE=icalparameter.h
# End Source File
# Begin Source File

SOURCE=icalparameterimpl.h
# End Source File
# Begin Source File

SOURCE=icalparser.h
# End Source File
# Begin Source File

SOURCE=icalperiod.h
# End Source File
# Begin Source File

SOURCE=icalproperty.h
# End Source File
# Begin Source File

SOURCE=icalrecur.h
# End Source File
# Begin Source File

SOURCE=icalrestriction.h
# End Source File
# Begin Source File

SOURCE=icaltime.h
# End Source File
# Begin Source File

SOURCE=.\icaltimezone.h
# End Source File
# Begin Source File

SOURCE=icaltypes.h
# End Source File
# Begin Source File

SOURCE=icalvalue.h
# End Source File
# Begin Source File

SOURCE=icalvalueimpl.h
# End Source File
# Begin Source File

SOURCE=.\icalversion.h
# End Source File
# Begin Source File

SOURCE=icalversion.h.in

!IF  "$(CFG)" == "libical - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
InputPath=icalversion.h.in
InputName=icalversion.h

"$(InputName)" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	sed -e "s/@VERSION@/0.24/g" -e "s/@PACKAGE@/libical/g" $(InputName).in > $(InputName)

# End Custom Build

!ELSEIF  "$(CFG)" == "libical - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
InputPath=icalversion.h.in
InputName=icalversion.h

"$(InputName)" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	sed -e "s/@VERSION@/0.24/g" -e "s/@PACKAGE@/libical/g" $(InputName).in > $(InputName)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=pvl.h
# End Source File
# Begin Source File

SOURCE=sspm.h
# End Source File
# End Group
# End Target
# End Project
