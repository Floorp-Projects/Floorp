# Microsoft Developer Studio Generated NMAKE File, Based on Burg.dsp
!IF "$(CFG)" == ""
CFG=Burg - Win32 Debug
!MESSAGE No configuration specified. Defaulting to Burg - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "Burg - Win32 Release" && "$(CFG)" != "Burg - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Burg.mak" CFG="Burg - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Burg - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "Burg - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Burg - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\Burg.exe" "$(OUTDIR)\Burg.bsc"

!ELSE 

ALL : "$(OUTDIR)\Burg.exe" "$(OUTDIR)\Burg.bsc"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\be.obj"
	-@erase "$(INTDIR)\be.sbr"
	-@erase "$(INTDIR)\burs.obj"
	-@erase "$(INTDIR)\burs.sbr"
	-@erase "$(INTDIR)\closure.obj"
	-@erase "$(INTDIR)\closure.sbr"
	-@erase "$(INTDIR)\delta.obj"
	-@erase "$(INTDIR)\delta.sbr"
	-@erase "$(INTDIR)\fe.obj"
	-@erase "$(INTDIR)\fe.sbr"
	-@erase "$(INTDIR)\item.obj"
	-@erase "$(INTDIR)\item.sbr"
	-@erase "$(INTDIR)\lex.obj"
	-@erase "$(INTDIR)\lex.sbr"
	-@erase "$(INTDIR)\list.obj"
	-@erase "$(INTDIR)\list.sbr"
	-@erase "$(INTDIR)\main.obj"
	-@erase "$(INTDIR)\main.sbr"
	-@erase "$(INTDIR)\map.obj"
	-@erase "$(INTDIR)\map.sbr"
	-@erase "$(INTDIR)\nonterminal.obj"
	-@erase "$(INTDIR)\nonterminal.sbr"
	-@erase "$(INTDIR)\operator.obj"
	-@erase "$(INTDIR)\operator.sbr"
	-@erase "$(INTDIR)\pattern.obj"
	-@erase "$(INTDIR)\pattern.sbr"
	-@erase "$(INTDIR)\plank.obj"
	-@erase "$(INTDIR)\plank.sbr"
	-@erase "$(INTDIR)\queue.obj"
	-@erase "$(INTDIR)\queue.sbr"
	-@erase "$(INTDIR)\rule.obj"
	-@erase "$(INTDIR)\rule.sbr"
	-@erase "$(INTDIR)\string.obj"
	-@erase "$(INTDIR)\string.sbr"
	-@erase "$(INTDIR)\symtab.obj"
	-@erase "$(INTDIR)\symtab.sbr"
	-@erase "$(INTDIR)\table.obj"
	-@erase "$(INTDIR)\table.sbr"
	-@erase "$(INTDIR)\test.obj"
	-@erase "$(INTDIR)\test.sbr"
	-@erase "$(INTDIR)\trim.obj"
	-@erase "$(INTDIR)\trim.sbr"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(INTDIR)\y.tab.obj"
	-@erase "$(INTDIR)\y.tab.sbr"
	-@erase "$(INTDIR)\zalloc.obj"
	-@erase "$(INTDIR)\zalloc.sbr"
	-@erase "$(OUTDIR)\Burg.bsc"
	-@erase "$(OUTDIR)\Burg.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /ML /W3 /GX /O2 /I "." /I "..\..\..\..\Tools\burg" /I\
 "..\..\..\Exports ..\..\..\Exports\md" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D\
 "_MBCS" /FR"$(INTDIR)\\" /Fp"$(INTDIR)\Burg.pch" /YX /Fo"$(INTDIR)\\"\
 /Fd"$(INTDIR)\\" /FD /c 
CPP_OBJS=.\Release/
CPP_SBRS=.\Release/
BSC32=bscmake.exe
BSC32_FLAGS=/o"$(OUTDIR)\Burg.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\be.sbr" \
	"$(INTDIR)\burs.sbr" \
	"$(INTDIR)\closure.sbr" \
	"$(INTDIR)\delta.sbr" \
	"$(INTDIR)\fe.sbr" \
	"$(INTDIR)\item.sbr" \
	"$(INTDIR)\lex.sbr" \
	"$(INTDIR)\list.sbr" \
	"$(INTDIR)\main.sbr" \
	"$(INTDIR)\map.sbr" \
	"$(INTDIR)\nonterminal.sbr" \
	"$(INTDIR)\operator.sbr" \
	"$(INTDIR)\pattern.sbr" \
	"$(INTDIR)\plank.sbr" \
	"$(INTDIR)\queue.sbr" \
	"$(INTDIR)\rule.sbr" \
	"$(INTDIR)\string.sbr" \
	"$(INTDIR)\symtab.sbr" \
	"$(INTDIR)\table.sbr" \
	"$(INTDIR)\test.sbr" \
	"$(INTDIR)\trim.sbr" \
	"$(INTDIR)\y.tab.sbr" \
	"$(INTDIR)\zalloc.sbr"

"$(OUTDIR)\Burg.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib wsock32.lib /nologo /subsystem:console /incremental:no\
 /pdb:"$(OUTDIR)\Burg.pdb" /machine:I386 /out:"$(OUTDIR)\Burg.exe" 
LINK32_OBJS= \
	"$(INTDIR)\be.obj" \
	"$(INTDIR)\burs.obj" \
	"$(INTDIR)\closure.obj" \
	"$(INTDIR)\delta.obj" \
	"$(INTDIR)\fe.obj" \
	"$(INTDIR)\item.obj" \
	"$(INTDIR)\lex.obj" \
	"$(INTDIR)\list.obj" \
	"$(INTDIR)\main.obj" \
	"$(INTDIR)\map.obj" \
	"$(INTDIR)\nonterminal.obj" \
	"$(INTDIR)\operator.obj" \
	"$(INTDIR)\pattern.obj" \
	"$(INTDIR)\plank.obj" \
	"$(INTDIR)\queue.obj" \
	"$(INTDIR)\rule.obj" \
	"$(INTDIR)\string.obj" \
	"$(INTDIR)\symtab.obj" \
	"$(INTDIR)\table.obj" \
	"$(INTDIR)\test.obj" \
	"$(INTDIR)\trim.obj" \
	"$(INTDIR)\y.tab.obj" \
	"$(INTDIR)\zalloc.obj"

"$(OUTDIR)\Burg.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "Burg - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "y.tab.h" "y.tab.c" "$(OUTDIR)\Burg.exe" "$(OUTDIR)\Burg.bsc"

!ELSE 

ALL : "y.tab.h" "y.tab.c" "$(OUTDIR)\Burg.exe" "$(OUTDIR)\Burg.bsc"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\be.obj"
	-@erase "$(INTDIR)\be.sbr"
	-@erase "$(INTDIR)\burs.obj"
	-@erase "$(INTDIR)\burs.sbr"
	-@erase "$(INTDIR)\closure.obj"
	-@erase "$(INTDIR)\closure.sbr"
	-@erase "$(INTDIR)\delta.obj"
	-@erase "$(INTDIR)\delta.sbr"
	-@erase "$(INTDIR)\fe.obj"
	-@erase "$(INTDIR)\fe.sbr"
	-@erase "$(INTDIR)\item.obj"
	-@erase "$(INTDIR)\item.sbr"
	-@erase "$(INTDIR)\lex.obj"
	-@erase "$(INTDIR)\lex.sbr"
	-@erase "$(INTDIR)\list.obj"
	-@erase "$(INTDIR)\list.sbr"
	-@erase "$(INTDIR)\main.obj"
	-@erase "$(INTDIR)\main.sbr"
	-@erase "$(INTDIR)\map.obj"
	-@erase "$(INTDIR)\map.sbr"
	-@erase "$(INTDIR)\nonterminal.obj"
	-@erase "$(INTDIR)\nonterminal.sbr"
	-@erase "$(INTDIR)\operator.obj"
	-@erase "$(INTDIR)\operator.sbr"
	-@erase "$(INTDIR)\pattern.obj"
	-@erase "$(INTDIR)\pattern.sbr"
	-@erase "$(INTDIR)\plank.obj"
	-@erase "$(INTDIR)\plank.sbr"
	-@erase "$(INTDIR)\queue.obj"
	-@erase "$(INTDIR)\queue.sbr"
	-@erase "$(INTDIR)\rule.obj"
	-@erase "$(INTDIR)\rule.sbr"
	-@erase "$(INTDIR)\string.obj"
	-@erase "$(INTDIR)\string.sbr"
	-@erase "$(INTDIR)\symtab.obj"
	-@erase "$(INTDIR)\symtab.sbr"
	-@erase "$(INTDIR)\table.obj"
	-@erase "$(INTDIR)\table.sbr"
	-@erase "$(INTDIR)\test.obj"
	-@erase "$(INTDIR)\test.sbr"
	-@erase "$(INTDIR)\trim.obj"
	-@erase "$(INTDIR)\trim.sbr"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(INTDIR)\vc50.pdb"
	-@erase "$(INTDIR)\y.tab.obj"
	-@erase "$(INTDIR)\y.tab.sbr"
	-@erase "$(INTDIR)\zalloc.obj"
	-@erase "$(INTDIR)\zalloc.sbr"
	-@erase "$(OUTDIR)\Burg.bsc"
	-@erase "$(OUTDIR)\Burg.exe"
	-@erase "$(OUTDIR)\Burg.ilk"
	-@erase "$(OUTDIR)\Burg.pdb"
	-@erase "y.tab.c"
	-@erase "y.tab.h"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MLd /W3 /Gm /GX /Zi /Od /I "." /I "..\..\..\..\Tools\burg\\"\
 /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "DEBUG" /FR"$(INTDIR)\\"\
 /Fp"$(INTDIR)\Burg.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
CPP_OBJS=.\Debug/
CPP_SBRS=.\Debug/
BSC32=bscmake.exe
BSC32_FLAGS=/o"$(OUTDIR)\Burg.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\be.sbr" \
	"$(INTDIR)\burs.sbr" \
	"$(INTDIR)\closure.sbr" \
	"$(INTDIR)\delta.sbr" \
	"$(INTDIR)\fe.sbr" \
	"$(INTDIR)\item.sbr" \
	"$(INTDIR)\lex.sbr" \
	"$(INTDIR)\list.sbr" \
	"$(INTDIR)\main.sbr" \
	"$(INTDIR)\map.sbr" \
	"$(INTDIR)\nonterminal.sbr" \
	"$(INTDIR)\operator.sbr" \
	"$(INTDIR)\pattern.sbr" \
	"$(INTDIR)\plank.sbr" \
	"$(INTDIR)\queue.sbr" \
	"$(INTDIR)\rule.sbr" \
	"$(INTDIR)\string.sbr" \
	"$(INTDIR)\symtab.sbr" \
	"$(INTDIR)\table.sbr" \
	"$(INTDIR)\test.sbr" \
	"$(INTDIR)\trim.sbr" \
	"$(INTDIR)\y.tab.sbr" \
	"$(INTDIR)\zalloc.sbr"

"$(OUTDIR)\Burg.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib wsock32.lib /nologo /subsystem:console /incremental:yes\
 /pdb:"$(OUTDIR)\Burg.pdb" /debug /machine:I386 /out:"$(OUTDIR)\Burg.exe"\
 /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\be.obj" \
	"$(INTDIR)\burs.obj" \
	"$(INTDIR)\closure.obj" \
	"$(INTDIR)\delta.obj" \
	"$(INTDIR)\fe.obj" \
	"$(INTDIR)\item.obj" \
	"$(INTDIR)\lex.obj" \
	"$(INTDIR)\list.obj" \
	"$(INTDIR)\main.obj" \
	"$(INTDIR)\map.obj" \
	"$(INTDIR)\nonterminal.obj" \
	"$(INTDIR)\operator.obj" \
	"$(INTDIR)\pattern.obj" \
	"$(INTDIR)\plank.obj" \
	"$(INTDIR)\queue.obj" \
	"$(INTDIR)\rule.obj" \
	"$(INTDIR)\string.obj" \
	"$(INTDIR)\symtab.obj" \
	"$(INTDIR)\table.obj" \
	"$(INTDIR)\test.obj" \
	"$(INTDIR)\trim.obj" \
	"$(INTDIR)\y.tab.obj" \
	"$(INTDIR)\zalloc.obj"

"$(OUTDIR)\Burg.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<


!IF "$(CFG)" == "Burg - Win32 Release" || "$(CFG)" == "Burg - Win32 Debug"
SOURCE=..\..\..\..\Tools\Burg\be.c

!IF  "$(CFG)" == "Burg - Win32 Release"

DEP_CPP_BE_C0=\
	"..\..\..\..\Tools\Burg\b.h"\
	"..\..\..\..\Tools\Burg\fe.h"\
	

"$(INTDIR)\be.obj"	"$(INTDIR)\be.sbr" : $(SOURCE) $(DEP_CPP_BE_C0) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Burg - Win32 Debug"

DEP_CPP_BE_C0=\
	"..\..\..\..\Tools\Burg\b.h"\
	"..\..\..\..\Tools\Burg\fe.h"\
	

"$(INTDIR)\be.obj"	"$(INTDIR)\be.sbr" : $(SOURCE) $(DEP_CPP_BE_C0) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\..\Tools\Burg\burs.c
DEP_CPP_BURS_=\
	"..\..\..\..\Tools\Burg\b.h"\
	

"$(INTDIR)\burs.obj"	"$(INTDIR)\burs.sbr" : $(SOURCE) $(DEP_CPP_BURS_)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\..\Tools\Burg\closure.c
DEP_CPP_CLOSU=\
	"..\..\..\..\Tools\Burg\b.h"\
	

"$(INTDIR)\closure.obj"	"$(INTDIR)\closure.sbr" : $(SOURCE) $(DEP_CPP_CLOSU)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\..\Tools\Burg\delta.c

!IF  "$(CFG)" == "Burg - Win32 Release"

DEP_CPP_DELTA=\
	"..\..\..\..\Tools\Burg\b.h"\
	"..\..\..\..\Tools\Burg\fe.h"\
	

"$(INTDIR)\delta.obj"	"$(INTDIR)\delta.sbr" : $(SOURCE) $(DEP_CPP_DELTA)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Burg - Win32 Debug"

DEP_CPP_DELTA=\
	"..\..\..\..\Tools\Burg\b.h"\
	"..\..\..\..\Tools\Burg\fe.h"\
	

"$(INTDIR)\delta.obj"	"$(INTDIR)\delta.sbr" : $(SOURCE) $(DEP_CPP_DELTA)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\..\Tools\Burg\fe.c

!IF  "$(CFG)" == "Burg - Win32 Release"

DEP_CPP_FE_C8=\
	"..\..\..\..\Tools\Burg\b.h"\
	"..\..\..\..\Tools\Burg\fe.h"\
	

"$(INTDIR)\fe.obj"	"$(INTDIR)\fe.sbr" : $(SOURCE) $(DEP_CPP_FE_C8) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Burg - Win32 Debug"

DEP_CPP_FE_C8=\
	"..\..\..\..\Tools\Burg\b.h"\
	"..\..\..\..\Tools\Burg\fe.h"\
	

"$(INTDIR)\fe.obj"	"$(INTDIR)\fe.sbr" : $(SOURCE) $(DEP_CPP_FE_C8) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\..\Tools\Burg\gram.y

!IF  "$(CFG)" == "Burg - Win32 Release"

InputPath=..\..\..\..\Tools\Burg\gram.y

"y.tab.c"	"y.tab.h"	 : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	$(NSTOOLS)\bin\yacc -l -b y.tab -d $(InputPath)

!ELSEIF  "$(CFG)" == "Burg - Win32 Debug"

InputPath=..\..\..\..\Tools\Burg\gram.y

"y.tab.c"	"y.tab.h"	 : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	$(NSTOOLS)\bin\yacc -l -b y.tab -d $(InputPath)

!ENDIF 

SOURCE=..\..\..\..\Tools\Burg\item.c

!IF  "$(CFG)" == "Burg - Win32 Release"

DEP_CPP_ITEM_=\
	"..\..\..\..\Tools\Burg\b.h"\
	"..\..\..\..\Tools\Burg\fe.h"\
	

"$(INTDIR)\item.obj"	"$(INTDIR)\item.sbr" : $(SOURCE) $(DEP_CPP_ITEM_)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Burg - Win32 Debug"

DEP_CPP_ITEM_=\
	"..\..\..\..\Tools\Burg\b.h"\
	"..\..\..\..\Tools\Burg\fe.h"\
	

"$(INTDIR)\item.obj"	"$(INTDIR)\item.sbr" : $(SOURCE) $(DEP_CPP_ITEM_)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\..\Tools\Burg\lex.c

!IF  "$(CFG)" == "Burg - Win32 Release"

DEP_CPP_LEX_C=\
	"..\..\..\..\Tools\Burg\b.h"\
	"..\..\..\..\Tools\Burg\fe.h"\
	".\y.tab.h"\
	

"$(INTDIR)\lex.obj"	"$(INTDIR)\lex.sbr" : $(SOURCE) $(DEP_CPP_LEX_C)\
 "$(INTDIR)" ".\y.tab.h"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Burg - Win32 Debug"

DEP_CPP_LEX_C=\
	"..\..\..\..\Tools\Burg\b.h"\
	"..\..\..\..\Tools\Burg\fe.h"\
	"..\..\..\..\tools\burg\y.tab.h"\
	

"$(INTDIR)\lex.obj"	"$(INTDIR)\lex.sbr" : $(SOURCE) $(DEP_CPP_LEX_C)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\..\Tools\Burg\list.c
DEP_CPP_LIST_=\
	"..\..\..\..\Tools\Burg\b.h"\
	

"$(INTDIR)\list.obj"	"$(INTDIR)\list.sbr" : $(SOURCE) $(DEP_CPP_LIST_)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\..\Tools\Burg\main.c

!IF  "$(CFG)" == "Burg - Win32 Release"

DEP_CPP_MAIN_=\
	"..\..\..\..\Tools\Burg\b.h"\
	"..\..\..\..\Tools\Burg\fe.h"\
	

"$(INTDIR)\main.obj"	"$(INTDIR)\main.sbr" : $(SOURCE) $(DEP_CPP_MAIN_)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Burg - Win32 Debug"

DEP_CPP_MAIN_=\
	"..\..\..\..\Tools\Burg\b.h"\
	"..\..\..\..\Tools\Burg\fe.h"\
	

"$(INTDIR)\main.obj"	"$(INTDIR)\main.sbr" : $(SOURCE) $(DEP_CPP_MAIN_)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\..\Tools\Burg\map.c
DEP_CPP_MAP_C=\
	"..\..\..\..\Tools\Burg\b.h"\
	

"$(INTDIR)\map.obj"	"$(INTDIR)\map.sbr" : $(SOURCE) $(DEP_CPP_MAP_C)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\..\Tools\Burg\nonterminal.c
DEP_CPP_NONTE=\
	"..\..\..\..\Tools\Burg\b.h"\
	

"$(INTDIR)\nonterminal.obj"	"$(INTDIR)\nonterminal.sbr" : $(SOURCE)\
 $(DEP_CPP_NONTE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\..\Tools\Burg\operator.c
DEP_CPP_OPERA=\
	"..\..\..\..\Tools\Burg\b.h"\
	

"$(INTDIR)\operator.obj"	"$(INTDIR)\operator.sbr" : $(SOURCE) $(DEP_CPP_OPERA)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\..\Tools\Burg\pattern.c
DEP_CPP_PATTE=\
	"..\..\..\..\Tools\Burg\b.h"\
	

"$(INTDIR)\pattern.obj"	"$(INTDIR)\pattern.sbr" : $(SOURCE) $(DEP_CPP_PATTE)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\..\Tools\Burg\plank.c

!IF  "$(CFG)" == "Burg - Win32 Release"

DEP_CPP_PLANK=\
	"..\..\..\..\Tools\Burg\b.h"\
	"..\..\..\..\Tools\Burg\fe.h"\
	

"$(INTDIR)\plank.obj"	"$(INTDIR)\plank.sbr" : $(SOURCE) $(DEP_CPP_PLANK)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Burg - Win32 Debug"

DEP_CPP_PLANK=\
	"..\..\..\..\Tools\Burg\b.h"\
	"..\..\..\..\Tools\Burg\fe.h"\
	

"$(INTDIR)\plank.obj"	"$(INTDIR)\plank.sbr" : $(SOURCE) $(DEP_CPP_PLANK)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\..\Tools\Burg\queue.c
DEP_CPP_QUEUE=\
	"..\..\..\..\Tools\Burg\b.h"\
	

"$(INTDIR)\queue.obj"	"$(INTDIR)\queue.sbr" : $(SOURCE) $(DEP_CPP_QUEUE)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\..\Tools\Burg\rule.c
DEP_CPP_RULE_=\
	"..\..\..\..\Tools\Burg\b.h"\
	

"$(INTDIR)\rule.obj"	"$(INTDIR)\rule.sbr" : $(SOURCE) $(DEP_CPP_RULE_)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\..\Tools\Burg\string.c

!IF  "$(CFG)" == "Burg - Win32 Release"

DEP_CPP_STRIN=\
	"..\..\..\..\Tools\Burg\b.h"\
	"..\..\..\..\Tools\Burg\fe.h"\
	

"$(INTDIR)\string.obj"	"$(INTDIR)\string.sbr" : $(SOURCE) $(DEP_CPP_STRIN)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Burg - Win32 Debug"

DEP_CPP_STRIN=\
	"..\..\..\..\Tools\Burg\b.h"\
	"..\..\..\..\Tools\Burg\fe.h"\
	

"$(INTDIR)\string.obj"	"$(INTDIR)\string.sbr" : $(SOURCE) $(DEP_CPP_STRIN)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\..\Tools\Burg\symtab.c

!IF  "$(CFG)" == "Burg - Win32 Release"

DEP_CPP_SYMTA=\
	"..\..\..\..\Tools\Burg\b.h"\
	"..\..\..\..\Tools\Burg\fe.h"\
	

"$(INTDIR)\symtab.obj"	"$(INTDIR)\symtab.sbr" : $(SOURCE) $(DEP_CPP_SYMTA)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Burg - Win32 Debug"

DEP_CPP_SYMTA=\
	"..\..\..\..\Tools\Burg\b.h"\
	"..\..\..\..\Tools\Burg\fe.h"\
	

"$(INTDIR)\symtab.obj"	"$(INTDIR)\symtab.sbr" : $(SOURCE) $(DEP_CPP_SYMTA)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\..\Tools\Burg\table.c
DEP_CPP_TABLE=\
	"..\..\..\..\Tools\Burg\b.h"\
	

"$(INTDIR)\table.obj"	"$(INTDIR)\table.sbr" : $(SOURCE) $(DEP_CPP_TABLE)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\..\Tools\Burg\test.c

"$(INTDIR)\test.obj"	"$(INTDIR)\test.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\..\Tools\Burg\trim.c

!IF  "$(CFG)" == "Burg - Win32 Release"

DEP_CPP_TRIM_=\
	"..\..\..\..\Tools\Burg\b.h"\
	"..\..\..\..\Tools\Burg\fe.h"\
	

"$(INTDIR)\trim.obj"	"$(INTDIR)\trim.sbr" : $(SOURCE) $(DEP_CPP_TRIM_)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Burg - Win32 Debug"

DEP_CPP_TRIM_=\
	"..\..\..\..\Tools\Burg\b.h"\
	"..\..\..\..\Tools\Burg\fe.h"\
	

"$(INTDIR)\trim.obj"	"$(INTDIR)\trim.sbr" : $(SOURCE) $(DEP_CPP_TRIM_)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=.\y.tab.c

!IF  "$(CFG)" == "Burg - Win32 Release"

DEP_CPP_Y_TAB=\
	"..\..\..\..\Tools\Burg\b.h"\
	"..\..\..\..\Tools\Burg\fe.h"\
	

"$(INTDIR)\y.tab.obj"	"$(INTDIR)\y.tab.sbr" : $(SOURCE) $(DEP_CPP_Y_TAB)\
 "$(INTDIR)"


!ELSEIF  "$(CFG)" == "Burg - Win32 Debug"

DEP_CPP_Y_TAB=\
	"..\..\..\..\Tools\Burg\b.h"\
	"..\..\..\..\Tools\Burg\fe.h"\
	

"$(INTDIR)\y.tab.obj"	"$(INTDIR)\y.tab.sbr" : $(SOURCE) $(DEP_CPP_Y_TAB)\
 "$(INTDIR)"


!ENDIF 

SOURCE=..\..\..\..\Tools\Burg\zalloc.c
DEP_CPP_ZALLO=\
	"..\..\..\..\Tools\Burg\b.h"\
	

"$(INTDIR)\zalloc.obj"	"$(INTDIR)\zalloc.sbr" : $(SOURCE) $(DEP_CPP_ZALLO)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

