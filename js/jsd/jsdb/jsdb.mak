
PROJ = jsdb
JSDB = .
JSD = $(JSDB)\..
JSSRC = $(JSD)\..\src

!IF "$(BUILD_OPT)" != ""
OBJ = Release
CC_FLAGS = /DNDEBUG
!ELSE
OBJ = Debug
CC_FLAGS = /DDEBUG
LINK_FLAGS = /DEBUG
!ENDIF 

CFLAGS = /nologo /MDd /W3 /Gm /GX /Zi /Od\
         /I $(JSSRC)\
         /I $(JSD)\
         /I $(JSDB)\
         $(CC_FLAGS)\
         /DWIN32 /DXP_PC /D_WINDOWS /D_WIN32\
         /DJSDEBUGGER\
         /DJSDEBUGGER_C_UI\
         /DJSD_LOWLEVEL_SOURCE\
         /DJSFILE\
         /c /Fp$(OBJ)\$(PROJ).pch /Fd$(OBJ)\$(PROJ).pdb /YX -Fo$@ $<

LFLAGS = /nologo /subsystem:console /incremental:no /machine:I386 $(LINK_FLAGS)\
         /pdb:$(OBJ)\$(PROJ).pdb -out:$(OBJ)\$(PROJ).exe

LLIBS = kernel32.lib advapi32.lib $(JSSRC)\$(OBJ)\js32.lib $(JSD)\$(OBJ)\jsd.lib

CPP=cl.exe
LINK32=link.exe

all: $(OBJ) dlls $(OBJ)\$(PROJ).exe $(OBJ)\debugger.js


HEADERS =   $(JSDB)\jsdb.h      \
            $(JSDB)\jsdbpriv.h


OBJECTS =   $(OBJ)\js.obj       \
            $(OBJ)\jsdb.obj     \
            $(OBJ)\jsdrefl.obj  


$(OBJECTS) : $(HEADERS)


$(OBJ)\$(PROJ).exe: $(OBJECTS)
  $(LINK32) $(LFLAGS) $** $(LLIBS)

.c{$(OBJ)}.obj:
  $(CPP) $(CFLAGS)

{$(JSSRC)}.c.obj:
  $(CPP) $(CFLAGS)

$(OBJ) :
    mkdir $(OBJ)

$(OBJ)\js32.dll :
    @cd ..\..\src
    @nmake -f js.mak CFG="js - Win32 $(OBJ)"
    @cd ..\jsd\jsdb
    @echo Copying dll from js/src
    @copy $(JSSRC)\$(OBJ)\js32.dll  $(OBJ)  >NUL
    @if "$(OBJ)" == "Debug" copy $(JSSRC)\$(OBJ)\js32.pdb  $(OBJ)  >NUL

$(OBJ)\jsd.dll :
    @cd ..
    @nmake -f jsd.mak JSD_THREADSAFE=1 BUILD_OPT=$(BUILD_OPT)
    @cd jsdb
    @echo Copying dll from js/jsd
    @copy $(JSD)\$(OBJ)\jsd.dll  $(OBJ)  >NUL
    @if "$(OBJ)" == "Debug" copy $(JSD)\$(OBJ)\jsd.pdb  $(OBJ)  >NUL

dlls : $(OBJ)\js32.dll $(OBJ)\jsd.dll

$(OBJ)\debugger.js: *.js
    @echo Copying *.js
    @copy *.js  $(OBJ) >NUL
    
clean:
    @echo Deleting built files
    @del $(OBJ)\*.pch >NUL
    @del $(OBJ)\*.obj >NUL
    @del $(OBJ)\*.exp >NUL
    @del $(OBJ)\*.lib >NUL
    @del $(OBJ)\*.idb >NUL
    @del $(OBJ)\*.pdb >NUL
    @del $(OBJ)\*.dll >NUL
    @del $(OBJ)\*.exe >NUL

deep_clean: clean
    @cd ..\..\src
    @nmake -f js.mak CFG="js - Win32 $(OBJ)" clean
    @cd ..\jsd\jsdb
    @cd ..
    @nmake -f jsd.mak clean BUILD_OPT=$(BUILD_OPT)
    @cd jsdb
