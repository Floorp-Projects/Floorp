#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "NPL"); you may not use this file except in
# compliance with the NPL.  You may obtain a copy of the NPL at
# http://www.mozilla.org/NPL/
#
# Software distributed under the NPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
# for the specific language governing rights and limitations under the
# NPL.
#
# The Initial Developer of this code under the NPL is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
# Reserved.

PROJ = jsdshell
JSD = .
JSDJAVA = $(JSD)\JAVA
REF = $(JSD)\..
OBJ = Debug
RUN = Run

CFLAGS = /nologo /MDd /W3 /Gm /GX /Zi /Od\
         /I $(REF)\
         /I $(JSD)\
         /DDEBUG /DWIN32 /D_CONSOLE /DXP_PC /D_WINDOWS /D_WIN32\
         /DJSDEBUGGER\
!IF "$(JSDEBUGGER_JAVA_UI)" != ""
         /I $(JSD)\java\
         /I $(JSD)\java\jre\
         /I $(JSD)\java\jre\win32\
         /DJSDEBUGGER_JAVA_UI\
         /DJSD_STANDALONE_JAVA_VM\
!ENDIF 
         /DJSD_LOWLEVEL_SOURCE\
         /DEXPORT_JS_API\
         /DJSFILE\
         /c /Fp$(OBJ)\$(PROJ).pch /Fd$(OBJ)\$(PROJ).pdb /YX -Fo$@ $<

LFLAGS = /nologo /subsystem:console /incremental:no /machine:I386 /DEBUG\
         /pdb:$(OBJ)\$(PROJ).pdb -out:$(OBJ)\$(PROJ).exe

LLIBS = kernel32.lib advapi32.lib
# unused... user32.lib gdi32.lib winspool.lib comdlg32.lib shell32.lib                                    

CPP=cl.exe
LINK32=link.exe

all: $(OBJ) $(RUN) $(RUN)\$(PROJ).exe $(RUN)\$(PROJ).pdb

$(OBJ)\$(PROJ).exe:         \
        $(OBJ)\js.obj       \
        $(OBJ)\prassert.obj \
!IF "$(JSDEBUGGER_JAVA_UI)" != ""
        $(OBJ)\jsdjava.obj  \
        $(OBJ)\jsd_jntv.obj \
        $(OBJ)\jsd_jvm.obj  \
        $(OBJ)\jre.obj      \
        $(OBJ)\jre_md.obj   \
!ENDIF 
        $(OBJ)\jsd_high.obj \
        $(OBJ)\jsd_hook.obj \
        $(OBJ)\jsd_scpt.obj \
        $(OBJ)\jsd_stak.obj \
        $(OBJ)\jsd_text.obj \
        $(OBJ)\jsdebug.obj  \
        $(OBJ)\jsapi.obj    \
        $(OBJ)\jsarray.obj  \
        $(OBJ)\jsatom.obj   \
        $(OBJ)\jsbool.obj   \
        $(OBJ)\jscntxt.obj  \
        $(OBJ)\jsdate.obj   \
        $(OBJ)\jsdbgapi.obj \
        $(OBJ)\jsemit.obj   \
        $(OBJ)\jsfun.obj    \
        $(OBJ)\jsgc.obj     \
        $(OBJ)\jsinterp.obj \
        $(OBJ)\jslock.obj   \
        $(OBJ)\jsmath.obj   \
        $(OBJ)\jsnum.obj    \
        $(OBJ)\jsobj.obj    \
        $(OBJ)\jsopcode.obj \
        $(OBJ)\jsparse.obj  \
        $(OBJ)\jsregexp.obj \
        $(OBJ)\jsscan.obj   \
        $(OBJ)\jsscope.obj  \
        $(OBJ)\jsscript.obj \
        $(OBJ)\jsstr.obj    \
        $(OBJ)\jsxdrapi.obj \
        $(OBJ)\prarena.obj  \
        $(OBJ)\prdtoa.obj   \
        $(OBJ)\prhash.obj   \
        $(OBJ)\prlog2.obj   \
        $(OBJ)\prlong.obj   \
        $(OBJ)\prprintf.obj \
        $(OBJ)\prtime.obj
  $(LINK32) $(LFLAGS) $** $(LLIBS)

.c{$(OBJ)}.obj:
  $(CPP) $(CFLAGS)

{$(JSDJAVA)}.c{$(OBJ)}.obj :
  $(CPP) $(CFLAGS)

{$(JSD)}.c{$(OBJ)}.obj :
  $(CPP) $(CFLAGS)

{$(JSDJAVA)\jre}.c{$(OBJ)}.obj :
  $(CPP) $(CFLAGS)

{$(JSDJAVA)\jre\win32}.c{$(OBJ)}.obj :
  $(CPP) $(CFLAGS)

{$(REF)}.c{$(OBJ)}.obj :
  $(CPP) $(CFLAGS)

$(OBJ) :
    mkdir $(OBJ)

$(RUN) :
    mkdir $(RUN)

$(RUN)\$(PROJ).exe: $(OBJ)\$(PROJ).exe
    copy $(OBJ)\$(PROJ).exe $(RUN)

$(RUN)\$(PROJ).pdb: $(OBJ)\$(PROJ).pdb
    copy $(OBJ)\$(PROJ).pdb $(RUN)

clean:
    del $(OBJ)\*.pch
    del $(OBJ)\*.obj
    del $(OBJ)\*.exp
    del $(OBJ)\*.lib
    del $(OBJ)\*.idb
    del $(OBJ)\*.pdb
    del $(OBJ)\*.exe
    del $(RUN)\*.pdb
    del $(RUN)\*.exe
