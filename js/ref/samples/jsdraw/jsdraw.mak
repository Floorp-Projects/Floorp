
#
# jband - 11/08/97 - 
#

PROJ = jsdraw
JSD = ..\..\JSD
JSDJAVA = $(JSD)\JAVA
REF = ..\..
OBJ = Debug
RUN = Run

CFLAGS = /nologo /MDd /W3 /Gm /GX /Zi /Od\
         /I $(REF)\
         /I $(JSD)\
         /I $(JSD)\java\
         /I $(JSD)\java\jre\
         /I $(JSD)\java\jre\win32\
         /DDEBUG /DWIN32 /D_CONSOLE /DXP_PC /D_WINDOWS /D_WIN32\
         /DJSDEBUGGER\
         /DJSD_STANDALONE_JAVA_VM\
         /DJSFILE\
         /c /Fp$(OBJ)\$(PROJ).pch /Fd$(OBJ)\$(PROJ).pdb /YX -Fo$@ $<

LFLAGS = /nologo /subsystem:windows /entry:WinMainCRTStartup /incremental:no\
         /machine:I386 /DEBUG\
         /pdb:$(OBJ)\$(PROJ).pdb -out:$(OBJ)\$(PROJ).exe

LLIBS = kernel32.lib advapi32.lib\
        user32.lib gdi32.lib winspool.lib comdlg32.lib shell32.lib                                    

RCFLAGS = /r -DWIN32 -D_WIN32

CPP=cl.exe
LINK32=link.exe
RC=rc.exe

all: $(OBJ) $(RUN) $(RUN)\$(PROJ).exe $(RUN)\$(PROJ).pdb

$(OBJ)\$(PROJ).exe:         \
        $(OBJ)\jsdraw.obj   \
        $(OBJ)\maindlg.obj  \
        $(OBJ)\prompt.obj   \
        $(OBJ)\data.obj     \
        $(OBJ)\draw.obj     \
        $(OBJ)\jsdraw.res   \
        $(OBJ)\prassert.obj \
        $(OBJ)\jsdjava.obj  \
        $(OBJ)\jsd_jntv.obj \
        $(OBJ)\jsd_jvm.obj  \
        $(OBJ)\jre.obj      \
        $(OBJ)\jre_md.obj   \
        $(OBJ)\jsd_high.obj \
        $(OBJ)\jsd_hook.obj \
        $(OBJ)\jsd_scpt.obj \
        $(OBJ)\jsd_stak.obj \
        $(OBJ)\jsd_text.obj \
        $(OBJ)\jsd_lock.obj \
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
        $(OBJ)\prarena.obj  \
        $(OBJ)\prdtoa.obj   \
        $(OBJ)\prhash.obj   \
        $(OBJ)\prlog2.obj   \
        $(OBJ)\prlong.obj   \
        $(OBJ)\prprintf.obj \
        $(OBJ)\prtime.obj
  $(LINK32) $(LFLAGS) $** $(LLIBS)

$(OBJ)\$(PROJ).res: $(PROJ).rc resource.h

.rc{$(OBJ)}.res:
    $(RC) $(RCFLAGS) /fo$@  $<

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
    del $(OBJ)\*.res
    del $(OBJ)\*.idb
    del $(OBJ)\*.pdb
    del $(OBJ)\*.exe
    del $(RUN)\*.pdb
    del $(RUN)\*.exe




##
## jband - 11/08/97 - 
##
#
#TARGETOS = WINNT
#
#!include <win32.mak>
#
#all: jsdraw.exe
#
## Update the resources if necessary
#
#jsdraw.res: jsdraw.rc resource.h
#    $(rc) $(rcflags) $(rcvars) jsdraw.rc
#
#jsdraw.exe:         \
#        jsdraw.obj  \
#        maindlg.obj \
#        prompt.obj  \
#        data.obj    \
#        draw.obj    \
#        jsdraw.res
#  $(link) $(linkdebug) $(guiflags) -out:$*.exe $** ..\ref\Debug\js32.lib $(guilibs)
#
#.c.obj:
#  $(cc) $(cdebug) $(cflags) $(cvars) /I..\ref /YX $*.c
#
#clean:
#    del *.obj
#    del *.pch
#    del jsdraw.res
#    del jsdraw.exe
