
PROJ = nativejsengine
PACKAGE_DOT =   com.netscape.nativejsengine

NJSE = .
TESTS = $(NJSE)\tests
GEN = $(NJSE)\_jni
JSD = $(NJSE)\..
JS = $(JSD)\..\src
JSDJAVA = $(JSD)\java

JSPROJ      = js32
JSDPROJ     = jsd
JSDJAVAPROJ = jsdjava

EXPORT_BIN_BASE_DIR = $(NJSE)\..\..\jsdj\dist\bin
EXPORT_CLASSES_BASE_DIR = $(NJSE)\..\..\jsdj\dist\classes

!IF "$(BUILD_OPT)" != ""
OBJ = Release
CC_FLAGS = /DNDEBUG 
!ELSE
OBJ = Debug
CC_FLAGS = /DDEBUG 
LINK_FLAGS = /DEBUG
!ENDIF 

QUIET=@

EXPORT_BIN_DIR = $(EXPORT_BIN_BASE_DIR)\$(OBJ)

STD_CLASSPATH = -classpath $(EXPORT_CLASSES_BASE_DIR);$(CLASSPATH)

CFLAGS = /nologo /MDd /W3 /Gm /GX /Zi /Od\
         /DWIN32 /DXP_PC /D_WINDOWS /D_WIN32\
         /I $(JS)\
         /I $(JSD)\
         /I $(JSDJAVA)\
         /DJSDEBUGGER\
         /DJSD_THREADSAFE\
         $(CC_FLAGS)\
         /c /Fp$(OBJ)\$(PROJ).pch /Fd$(OBJ)\$(PROJ).pdb /YX -Fo$@ $<

LFLAGS = /nologo /subsystem:console /incremental:no /DLL /machine:I386 \
        $(LINK_FLAGS) /pdb:$(OBJ)\$(PROJ).pdb -out:$(OBJ)\$(PROJ).dll

LLIBS = kernel32.lib advapi32.lib \
        $(JS)\$(OBJ)\$(JSPROJ).lib \
        $(JSD)\$(OBJ)\$(JSDPROJ).lib \
        $(JSDJAVA)\$(OBJ)\$(JSDJAVAPROJ).lib

CPP=cl.exe
LINK32=link.exe

CLASSES_WITH_NATIVES = \
    $(PACKAGE_DOT).JSRuntime\
    $(PACKAGE_DOT).JSContext


all: $(GEN) $(OBJ) dlls mkjniheaders $(OBJ)\$(PROJ).dll export_binaries

$(OBJ)\$(PROJ).dll:         \
    $(OBJ)\nativejsengine.obj
  $(QUIET)$(LINK32) $(LFLAGS) $** $(LLIBS)

.c{$(OBJ)}.obj:
  $(QUIET)$(CPP) $(CFLAGS)

$(GEN) :
    @mkdir $(GEN)

$(OBJ) :
    @mkdir $(OBJ)

dlls :
    $(QUIET)cd ..\..\src
!IF "$(BUILD_OPT)" != ""
    $(QUIET)nmake -f js.mak CFG="js - Win32 Release"
!ELSE
    $(QUIET)nmake -f js.mak CFG="js - Win32 Debug"
!ENDIF 
    $(QUIET)cd ..\jsd\javawrap
    $(QUIET)cd ..
    $(QUIET)nmake -f jsd.mak JSD_THREADSAFE=1 $(OPT)
    $(QUIET)cd javawrap
    $(QUIET)cd ..\java
    $(QUIET)nmake -f jsdjava.mak $(OPT)
    $(QUIET)cd ..\javawrap


export_binaries : mk_export_dirs
    @echo exporting binaries
    $(QUIET)copy $(JS)\$(OBJ)\$(JSPROJ).dll $(EXPORT_BIN_DIR)               >NUL
    $(QUIET)copy $(JS)\$(OBJ)\$(JSPROJ).pdb $(EXPORT_BIN_DIR)               >NUL
    $(QUIET)copy $(JSD)\$(OBJ)\$(JSDPROJ).dll $(EXPORT_BIN_DIR)             >NUL
    $(QUIET)copy $(JSD)\$(OBJ)\$(JSDPROJ).pdb $(EXPORT_BIN_DIR)             >NUL
    $(QUIET)copy $(JSDJAVA)\$(OBJ)\$(JSDJAVAPROJ).dll $(EXPORT_BIN_DIR)     >NUL
    $(QUIET)copy $(JSDJAVA)\$(OBJ)\$(JSDJAVAPROJ).pdb $(EXPORT_BIN_DIR)     >NUL
    $(QUIET)copy $(OBJ)\$(PROJ).pdb $(EXPORT_BIN_DIR)                       >NUL
    $(QUIET)copy $(OBJ)\$(PROJ).dll $(EXPORT_BIN_DIR)                       >NUL

mkjniheaders :
    @echo generating JNI header
    $(QUIET)javah -jni -d "$(GEN)" $(STD_CLASSPATH) $(CLASSES_WITH_NATIVES)
    @touch *.c >NUL

mk_export_dirs:
    @if not exist $(JS)\..\jsdj\dist\NUL     @mkdir $(JS)\..\jsdj\dist
    @if not exist $(JS)\..\jsdj\dist\bin\NUL @mkdir $(JS)\..\jsdj\dist\bin
    @if not exist $(EXPORT_BIN_DIR)\NUL @mkdir $(EXPORT_BIN_DIR)

#mktest :
#    @echo compiling Java test file
#    @sj  $(JAVAFLAGS) $(TEST_CLASSPATH) $(TESTS)\Main.java
#    @echo copying js and jsd dlls
#    @copy $(JS)\$(OBJ)\$(JSPROJ).dll $(OBJ) >NUL
#    @copy $(JS)\$(OBJ)\$(JSPROJ).pdb $(OBJ) >NUL
#    @copy $(JSD)\$(OBJ)\$(JSDPROJ).dll $(OBJ) >NUL
#    @copy $(JSD)\$(OBJ)\$(JSDPROJ).pdb $(OBJ) >NUL
#    @copy $(TESTS)\*.js $(OBJ) >NUL

clean:
    @echo deleting old output
    $(QUIET)del $(OBJ)\*.pch >NUL
    $(QUIET)del $(OBJ)\*.obj >NUL
    $(QUIET)del $(OBJ)\*.exp >NUL
    $(QUIET)del $(OBJ)\*.lib >NUL
    $(QUIET)del $(OBJ)\*.idb >NUL
    $(QUIET)del $(OBJ)\*.pdb >NUL
    $(QUIET)del $(OBJ)\*.dll >NUL
    $(QUIET)del $(GEN)\*.h   >NUL


deep_clean: clean
    $(QUIET)cd ..\..\src
!IF "$(BUILD_OPT)" != ""
    $(QUIET)nmake -f js.mak CFG="js - Win32 Release" clean
!ELSE
    $(QUIET)nmake -f js.mak CFG="js - Win32 Debug" clean
!ENDIF 
    $(QUIET)cd ..\jsd\javawrap
    $(QUIET)cd ..
    $(QUIET)nmake -f jsd.mak clean
    $(QUIET)cd javawrap
    $(QUIET)cd ..\java
    $(QUIET)nmake -f jsdjava.mak clean
    $(QUIET)cd ..\javawrap
