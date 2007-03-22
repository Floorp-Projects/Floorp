
PROJ = jsd
JSD = .
JS = $(JSD)\..\src
JSPROJ = js32

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
         /DDEBUG /DWIN32 /D_CONSOLE /DXP_WIN /D_WINDOWS /D_WIN32\
         /DJSDEBUGGER\
!IF "$(JSD_THREADSAFE)" != ""
         /DJSD_THREADSAFE\
!ENDIF 
         /DEXPORT_JSD_API\
         $(CC_FLAGS)\
         /c /Fp$(OBJ)\$(PROJ).pch /Fd$(OBJ)\$(PROJ).pdb /YX -Fo$@ $<

LFLAGS = /nologo /subsystem:console /DLL /incremental:no /machine:I386 \
         $(LINK_FLAGS) /pdb:$(OBJ)\$(PROJ).pdb -out:$(OBJ)\$(PROJ).dll

LLIBS = kernel32.lib advapi32.lib $(JS)\$(OBJ)\$(JSPROJ).lib
# unused... user32.lib gdi32.lib winspool.lib comdlg32.lib shell32.lib                                    

CPP=cl.exe
LINK32=link.exe

all: $(OBJ) $(OBJ)\$(PROJ).dll
     

$(OBJ)\$(PROJ).dll:         \
        $(OBJ)\jsdebug.obj  \
        $(OBJ)\jsd_atom.obj \
        $(OBJ)\jsd_high.obj \
        $(OBJ)\jsd_hook.obj \
        $(OBJ)\jsd_obj.obj  \
        $(OBJ)\jsd_scpt.obj \
        $(OBJ)\jsd_stak.obj \
        $(OBJ)\jsd_step.obj \
        $(OBJ)\jsd_text.obj \
        $(OBJ)\jsd_lock.obj \
        $(OBJ)\jsd_val.obj 
  $(QUIET)$(LINK32) $(LFLAGS) $** $(LLIBS)

{$(JSD)}.c{$(OBJ)}.obj :
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
