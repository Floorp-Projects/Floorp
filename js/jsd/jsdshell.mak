
PROJ = jsdshell
JSD = .
JSDJAVA = $(JSD)\java
JS      = $(JSD)\..\src
RUN         = $(JSD)\run
JSPROJ      = js32
JSDPROJ     = jsd
JSDJAVAPROJ = jsdjava

!IF "$(BUILD_OPT)" != ""
OPT = BUILD_OPT=1
OBJ = Release
CC_FLAGS = /DNDEBUG 
!ELSE
OPT = 
OBJ = Debug
CC_FLAGS = /DDEBUG 
LINK_FLAGS = /DEBUG
!ENDIF 

QUIET=@

CFLAGS = /nologo /MDd /W3 /Gm /GX /Zi /Od\
         /I $(JS)\
         /I $(JSD)\
         /DDEBUG /DWIN32 /D_CONSOLE /DXP_WIN /D_WINDOWS /D_WIN32\
         /DJSDEBUGGER\
!IF "$(JSDEBUGGER_JAVA_UI)" != ""
         /I $(JSDJAVA)\
         /DJSDEBUGGER_JAVA_UI\
         /DJSD_STANDALONE_JAVA_VM\
!ENDIF 
         /DJSD_LOWLEVEL_SOURCE\
         /DJSFILE\
         $(CC_FLAGS)\
         /c /Fp$(OBJ)\$(PROJ).pch /Fd$(OBJ)\$(PROJ).pdb /YX -Fo$@ $<

LFLAGS = /nologo /subsystem:console /incremental:no /machine:I386 \
         $(LINK_FLAGS) /pdb:$(OBJ)\$(PROJ).pdb -out:$(OBJ)\$(PROJ).exe

LLIBS = kernel32.lib advapi32.lib \
        $(JS)\$(OBJ)\$(JSPROJ).lib \
        $(JSD)\$(OBJ)\$(JSDPROJ).lib \
        $(JSDJAVA)\$(OBJ)\$(JSDJAVAPROJ).lib

CPP=cl.exe
LINK32=link.exe

all: $(OBJ) $(RUN) dlls $(OBJ)\$(PROJ).exe copy_binaries

$(OBJ)\$(PROJ).exe:         \
        $(OBJ)\js.obj
  $(QUIET)$(LINK32) $(LFLAGS) $** $(LLIBS)


{$(JS)}.c{$(OBJ)}.obj :
  $(QUIET)$(CPP) $(CFLAGS)

dlls :
    $(QUIET)cd ..\src
!IF "$(BUILD_OPT)" != ""
    $(QUIET)nmake -f js.mak CFG="js - Win32 Release"
!ELSE
    $(QUIET)nmake -f js.mak CFG="js - Win32 Debug"
!ENDIF 
    $(QUIET)cd ..\jsd
    $(QUIET)nmake -f jsd.mak JSD_THREADSAFE=1 $(OPT)
    $(QUIET)cd java
    $(QUIET)nmake -f jsdjava.mak JSD_STANDALONE_JAVA_VM=1 $(OPT)
    $(QUIET)cd ..

copy_binaries :
    @echo copying binaries
    $(QUIET)copy $(JS)\$(OBJ)\$(JSPROJ).dll $(RUN)               >NUL
    $(QUIET)copy $(JS)\$(OBJ)\$(JSPROJ).pdb $(RUN)               >NUL
    $(QUIET)copy $(JSD)\$(OBJ)\$(JSDPROJ).dll $(RUN)             >NUL
    $(QUIET)copy $(JSD)\$(OBJ)\$(JSDPROJ).pdb $(RUN)             >NUL
    $(QUIET)copy $(JSDJAVA)\$(OBJ)\$(JSDJAVAPROJ).dll $(RUN)     >NUL
    $(QUIET)copy $(JSDJAVA)\$(OBJ)\$(JSDJAVAPROJ).pdb $(RUN)     >NUL
    $(QUIET)copy $(OBJ)\$(PROJ).pdb $(RUN)                       >NUL
    $(QUIET)copy $(OBJ)\$(PROJ).exe $(RUN)                       >NUL

$(OBJ) :
    $(QUIET)mkdir $(OBJ)

$(RUN) :
    $(QUIET)mkdir $(RUN)

clean:
    @echo deleting old output
    $(QUIET)del $(OBJ)\js.obj >NUL
    $(QUIET)del $(OBJ)\$(PROJ).pch >NUL
    $(QUIET)del $(OBJ)\$(PROJ)*.idb >NUL
    $(QUIET)del $(OBJ)\$(PROJ).pdb >NUL
    $(QUIET)del $(OBJ)\$(PROJ).exe >NUL
    $(QUIET)del $(RUN)\*.pdb >NUL
    $(QUIET)del $(RUN)\*.exe >NUL
    $(QUIET)del $(RUN)\*.dll >NUL

deep_clean: clean
    $(QUIET)cd ..\src
!IF "$(BUILD_OPT)" != ""
    $(QUIET)nmake -f js.mak CFG="js - Win32 Release" clean
!ELSE
    $(QUIET)nmake -f js.mak CFG="js - Win32 Debug" clean
!ENDIF 
    $(QUIET)cd ..\jsd
    $(QUIET)nmake -f jsd.mak clean
    $(QUIET)cd java
    $(QUIET)nmake -f jsdjava.mak clean
    $(QUIET)cd ..
