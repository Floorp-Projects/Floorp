
PROJ = jsdjava
JSDJAVA = .
JSD = $(JSDJAVA)\..
JS = $(JSD)\..\src
JSPROJ = js32
JSDPROJ = jsd

!IF "$(BUILD_OPT)" != ""
OBJ = Release
CC_FLAGS = /DNDEBUG 
!ELSE
OBJ = Debug
CC_FLAGS = /DDEBUG 
LINK_FLAGS = /DEBUG
!ENDIF 

QUIET=@

CFLAGS = /nologo /MDd /W3 /Gm /GX /Zi /Od\
         /I $(JS)\
         /I $(JSD)\
         /I $(JSDJAVA)\
         /DDEBUG /DWIN32 /DXP_PC /D_WINDOWS /D_WIN32\
         /DJSD_THREADSAFE\
         /DEXPORT_JSDJ_API\
         /DJSDEBUGGER\
!IF "$(JSD_STANDALONE_JAVA_VM)" != ""
         /I $(JSDJAVA)\jre\
         /I $(JSDJAVA)\jre\win32\
         /DJSD_STANDALONE_JAVA_VM\
!ENDIF 
         $(CC_FLAGS)\
         /c /Fp$(OBJ)\$(PROJ).pch /Fd$(OBJ)\$(PROJ).pdb /YX -Fo$@ $<

LFLAGS = /nologo /subsystem:console /DLL /incremental:no /machine:I386 \
         $(LINK_FLAGS) /pdb:$(OBJ)\$(PROJ).pdb -out:$(OBJ)\$(PROJ).dll

LLIBS = kernel32.lib advapi32.lib \
        $(JS)\$(OBJ)\$(JSPROJ).lib $(JSD)\$(OBJ)\$(JSDPROJ).lib

CPP=cl.exe
LINK32=link.exe

all: $(OBJ) $(OBJ)\$(PROJ).dll
     

$(OBJ)\$(PROJ).dll:         \
!IF "$(JSD_STANDALONE_JAVA_VM)" != ""
        $(OBJ)\jsd_jvm.obj  \
        $(OBJ)\jre.obj      \
        $(OBJ)\jre_md.obj   \
!ENDIF 
        $(OBJ)\jsdjava.obj  \
        $(OBJ)\jsd_jntv.obj
  $(QUIET)$(LINK32) $(LFLAGS) $** $(LLIBS)

{$(JSDJAVA)}.c{$(OBJ)}.obj :
  $(QUIET)$(CPP) $(CFLAGS)

{$(JSDJAVA)\jre}.c{$(OBJ)}.obj :
  $(QUIET)$(CPP) $(CFLAGS)

{$(JSDJAVA)\jre\win32}.c{$(OBJ)}.obj :
  $(QUIET)$(CPP) $(CFLAGS)

$(OBJ) :
    $(QUIET)mkdir $(OBJ)

clean:
    @echo deleting old output
    $(QUIET)del $(OBJ)\*.pch >NUL
    $(QUIET)del $(OBJ)\*.obj >NUL
    $(QUIET)del $(OBJ)\*.exp >NUL
    $(QUIET)del $(OBJ)\*.lib >NUL
    $(QUIET)del $(OBJ)\*.idb >NUL
    $(QUIET)del $(OBJ)\*.pdb >NUL
    $(QUIET)del $(OBJ)\*.dll >NUL
