#!gmake
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

# 

#   Mozilla makefile
#       Please use ns/client.mak for building.
#       See HowToBuild web page for instruction.


DEPTH = ..\..\..
include <$(DEPTH)\config\config.mak>

.SUFFIXES: .cpp .c .rc

!if !defined(MOZ_SRC)
MOZ_SRC=y:
!endif

!if !defined(MOZ_OUT)
MOZ_OUT=. 
!endif

!if !defined(MOZ_INT)
MOZ_INT=$(MOZ_OUT)
!endif

!if !defined(MOZ_BITS)
MOZ_BITS=32
!endif

!if !defined(MOZ_SEC)
MOZ_SEC=EXPORT
!endif

!if defined(MOZ_TIME)
TIMESTART=time /T
TIMESTOP=time /T
!else
TIMESTART=@rem
TIMESTOP=@rem
!endif

!if !defined(MOZ_PROCESS_NUMBER)
MOZ_PROCESS_NUMBER=0
!endif

!if defined(USERNAME) && !defined(MOZ_USERNAME)
MOZ_USERNAME=$(USERNAME)
!endif

!if defined(MOZ_USERNAME)
MOZ_USERDEBUG=/DDEBUG_$(MOZ_USERNAME)
!endif

SPELLCHK_DLL = sp$(MOZ_BITS)$(VERSION_NUMBER).dll
# Location of spell checker dictionary files
SPELLCHK_DATA = $(MOZ_SRC)\ns\modules\spellchk\data

!if !defined(MOZ_PURIFY)
MOZ_PURIFY=C:\Pure\Purify
!endif

# No security means no patcher required.
!if "$(NO_SECURITY)" == "1"
MOZ_NO_PATCHER=1
!endif

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 


CPP= \
!if "$(MOZ_BCPRO)" == ""
    cl.exe 
!else
    bcompile.exe
!endif
LINK= \
!if "$(MOZ_BITS)"=="32"
!if "$(MOZ_BCPRO)" == ""
    link.exe /nologo $(MOZ_LFLAGS)
!else
    bclink.exe /nologo $(MOZ_LFLAGS)
!endif
!else
    $(MOZ_TOOLS)\bin\optlinks.exe /nologo $(MOZ_LFLAGS)
!endif
MTL=mktyplib.exe /nologo
RSC= \
!if "$(MOZ_BITS)"=="32"
    rc.exe
!else
    rc
!endif
TXT2RC=txt2rc
BIN2RC=$(MOZ_SRC)\ns\config\bin2rc.exe

#
# Add different product values here, like dec alpha, mips etc, win16...
#
!if "$(MOZ_BITS)"=="32"
!if "$(MOZ_BCPRO)" == ""
PROD=x86
!else
PROD=BCx86
!endif
!else
PROD=16x86
!endif

#
# Some will differ.
# 
# Using x86 for MOZ_MEDIUM
#
!ifdef MOZ_NAV_BUILD_PREFIX
PROD=$(PROD:x86=Nav)
!endif

#
# Get compiler version info right
#
!if "$(PROD)"=="x86" || "$(PROD)"=="Nav"

# Intel 32 bit uses version 4
MSVC4=1

# Determine compiler version (Needed to decide which libraries to link with).
# Default to MSVC 4.0, set in your environment if different.
# For 4.2, you'll want 1020
# As per the compiler predefined macro, please.
!IF !DEFINED(_MSC_VER)
# Support old way of doing this in dogbert.
!IF "$(MOZ_VCVER)"=="41"
_MSC_VER=1000
!ELSE
_MSC_VER=1020
!ENDIF
!ENDIF

!endif

#
#       Should reflect non debug settings always,
#               regardless if CFLAGS_DEBUG is doing
#               so also.
#       This is so 16 bits can compile only portions desired
#               as debug (and still link).
#
CFLAGS_RELEASE=/DNDEBUG \
!IF "$(MOZ_BITS)"=="32"
    /MD /O1
!ELSE
    /O1
!ENDIF

!IF "$(MOZ_BITS)"=="32"
MOZ_DEBUG_FLAG=/Zi
!else
!IF !defined(MOZ_DEBUG_FLAG)
MOZ_DEBUG_FLAG=/Zd
!endif
!ENDIF

!if "$(MOZ_BITS)" == "16"
!if !defined(MOZ_STACK)
#   Set stack size for 16 bit product, in decimal.
#   How this number is calculated:
#       Link the .exe
#       Find the .map file
#       Find the line starting with "Type=Stack"
#       On the next line, take the first number, and do this math in hex:
#           FFFF - first number - 0410
#       That is the maximum stack value.
MOZ_STACK=33679
!endif
!endif


!if defined(MOZ_DEBUG)
VERSTR=Dbg
CFLAGS_DEBUG=$(MOZ_DEBUG_FLAG) /Bd /DDEBUG /D_DEBUG $(MOZ_USERDEBUG)\
!IF "$(MOZ_BITS)"=="32"
	/Gm /Gi \
!IF 0 #defined(MOZ_BATCH)
    /MDd /Od /Gy /Z7
!else
!if "$(MOZ_USERNAME)" == "WHITEBOX"
    /MDd /Od /Gy /FR /Yd /Fd"$(OUTDIR)\vcp$(MOZ_PROCESS_NUMBER).pdb"
!else
    /MDd /Od /Gy /Yd /Fd"$(OUTDIR)\vcp$(MOZ_PROCESS_NUMBER).pdb"
!endif
!endif
!ELSE
    /Odi
!ENDIF
RCFLAGS_DEBUG=/DDEBUG
LINKFLAGS_DEBUG= \
!if "$(MOZ_BITS)"=="32"
    /debug /incremental:yes comctl32.lib msvcrtd.lib winmm.lib
!else
    /STACK:$(MOZ_STACK) /ALIGN:128 /PACKC:61440 /SEG:1024 /NOD /PACKD /NOI /ONERROR:NOEXE /CO /MAP /DETAILEDMAP /CHECKEXE /RELOCATIONCHECK /W
!endif
!else
VERSTR=Rel
CFLAGS_DEBUG=$(CFLAGS_RELEASE)
RCFLAGS_DEBUG=/DNODEBUG
LINKFLAGS_DEBUG= \
!if "$(MOZ_BITS)"=="32"
    comctl32.lib msvcrt.lib winmm.lib
!else
    /STACK:$(MOZ_STACK) /ALIGN:128 /PACKC:61440 /SEG:1024 /NOD /PACKD /NOI /ONERROR:NOEXE
!endif
!endif

#
#       Edit these in order to control 16 bit
#               debug targets.
#
CFLAGS_DEFAULT=\
!if "$(MOZ_BITS)"=="32"
    $(CFLAGS_DEBUG)
!else
    $(CFLAGS_RELEASE) -DFORCE_PR_LOG
!endif


POLICY  = moz40p3


NEOFLAGS=/DqNeoThreads /DqNeoStandAlone /I$(MOZ_SRC)\ns\lib\libneo ^
    /I$(MOZ_SRC)\ns\lib\libneo\ibmpc ^
    /I$(MOZ_SRC)\ns\lib\libneo\ibmpc\alone 


#
# If you add a file in a new directory, you must add flags for that directory
#
CFLAGS_LIBMSG_C=        $(CFLAGS_DEFAULT) /Fp"$(OUTDIR)/msgc.pch" /YX"msg.h"
CFLAGS_LIBMIME_C=       $(CFLAGS_DEFAULT)
CFLAGS_LIBI18N_C=       $(CFLAGS_DEFAULT) /Fp"$(OUTDIR)/intlpriv.pch" /YX"intlpriv.h"
CFLAGS_LIBIMG_C=        $(CFLAGS_DEFAULT) /I$(MOZ_SRC)\ns\jpeg /Fp"$(OUTDIR)/xp.pch" /YX"xp.h"
CFLAGS_JTOOLS_C=        $(CFLAGS_DEFAULT)  
CFLAGS_LIBCNV_C=        $(CFLAGS_DEFAULT) /I$(MOZ_SRC)\ns\jpeg /Fp"$(OUTDIR)/xp.pch" /YX"xp.h"
CFLAGS_JPEG_C=          $(CFLAGS_DEFAULT) 
CFLAGS_LAYOUT_C=        $(CFLAGS_DEFAULT) /Fp"$(OUTDIR)/layoutc.pch" /YX"xp.h"
CFLAGS_LIBSTYLE_C=      $(CFLAGS_DEFAULT) /Fp"$(OUTDIR)/stylec.pch" /YX"xp.h"
CFLAGS_LIBJAR_C=        $(CFLAGS_DEFAULT) 
CFLAGS_LIBLAYER_C=      $(CFLAGS_DEFAULT) 
CFLAGS_LIBMISC_C=       $(CFLAGS_DEFAULT) 
CFLAGS_LIBNET_C=        $(CFLAGS_DEFAULT) /Fp"$(OUTDIR)/netc.pch" /YX"mkutils.h"  
CFLAGS_LIBNET_CPP=      $(CFLAGS_DEFAULT) /Fp"$(OUTDIR)/netcpp.pch" /YX"mkutils.h"
CFLAGS_LIBPARSE_C=      $(CFLAGS_DEFAULT) /Fp"$(OUTDIR)/pa_parse.pch" /YX"pa_parse.h"
CFLAGS_XP_C=            $(CFLAGS_DEFAULT) 
CFLAGS_LIBPICS_C=       $(CFLAGS_DEFAULT) 
CFLAGS_LIBPWCAC_C=       $(CFLAGS_DEFAULT) 
CFLAGS_XLATE_C=         $(CFLAGS_DEFAULT) 
CFLAGS_LIBDBM_C=        $(CFLAGS_DEFAULT) 
CFLAGS_PLUGIN_C=        $(CFLAGS_DEBUG)
CFLAGS_APPLET_C=        $(CFLAGS_DEFAULT) /Fp"$(OUTDIR)/lj.pch" /YX"lj.h"
CFLAGS_EDTPLUG_C=       $(CFLAGS_DEFAULT) /Fp"$(OUTDIR)/le.pch" /YX"le.h"
CFLAGS_LIBMOCHA_C=      $(CFLAGS_DEFAULT) /Fp"$(OUTDIR)/lm.pch" /YX"lm.h"
CFLAGS_LIBMSG_CPP=      $(CFLAGS_DEFAULT) $(NEOFLAGS) /Fp"$(OUTDIR)/msgcpp.pch" /YX"msg.h"
CFLAGS_LIBADDR_CPP=     $(CFLAGS_DEFAULT) $(NEOFLAGS) /Fp"$(OUTDIR)/addr.pch" /YX"neohdrsa.h"
CFLAGS_LIBADDR_C=       $(CFLAGS_DEFAULT) $(NEOFLAGS)
CFLAGS_LIBNEO_CPP=      $(CFLAGS_DEFAULT) $(NEOFLAGS) 
CFLAGS_LAYOUT_CPP=      $(CFLAGS_DEFAULT) /Fp"$(OUTDIR)/editor.pch" /YX"editor.h"
CFLAGS_PLUGIN_CPP=      $(CFLAGS_DEBUG) /I$(MOZ_SRC)\ns\cmd\winfe /Fp"$(OUTDIR)/stdafx.pch" /YX"stdafx.h"
CFLAGS_LIBPREF_C=                 $(CFLAGS_DEBUG)
CFLAGS_WINFE_C=                 $(CFLAGS_DEBUG)
!if "$(MOZ_BITS)"=="32"
!if "$(MOZ_BCPRO)" == ""
CFLAGS_WINFE_CPP=       $(CFLAGS_DEBUG) /I$(MOZ_SRC)\ns\jpeg /Fp"$(OUTDIR)/stdafx.pch" /YX"stdafx.h"
!else
CFLAGS_WINFE_CPP=       $(CFLAGS_DEBUG) /I$(MOZ_SRC)\ns\jpeg
!endif
!else
CFLAGS_WINFE_CPP=       $(CFLAGS_DEBUG)
!endif
!if "$(MOZ_BITS)"=="16"
CFLAGS_WINDOWS_C=		$(CFLAGS_DEFAULT) /I$(MOZ_SRC)\ns\dist\public\win16\private
!endif

OUTDIR=$(MOZ_OUT)\$(PROD)$(VERSTR)
GENDIR=.\_gen

# I changed $(DIST_PREFIX)954.0" to "WIN954.0" so that lite and medium builds will work.
!if ("$(MOZ_BITS)" == "16" )
BINREL_DIST = $(DIST)
!else
!ifndef MOZ_DEBUG
BINREL_DIST=$(XPDIST)\WIN954.0_OPT.OBJ
!else
BINREL_DIST=$(XPDIST)\WIN954.0_DBG.OBJD
!endif
!endif
	
LINK_LIBS= \
!if "$(MOZ_BITS)"=="32"
!ifndef NSPR20
    $(DIST)\lib\pr32$(VERSION_NUMBER).lib \
!else
    $(DIST)\lib\libnspr21.lib \
    $(DIST)\lib\libplds21.lib \
    $(DIST)\lib\libplc21.lib \
    $(DIST)\lib\libmsgc21.lib \
!endif
!ifdef MOZ_JAVA
    $(DIST)\lib\jrt32$(VERSION_NUMBER).lib \
!else
    $(DIST)\lib\libsjs32.lib \
    $(DIST)\lib\libnjs32.lib \
!endif
    $(DIST)\lib\js32$(VERSION_NUMBER).lib \
!ifdef MOZ_JAVA
    $(DIST)\lib\jsd32$(VERSION_NUMBER).lib \
!endif
    $(DIST)\lib\jsj32$(VERSION_NUMBER).lib \
!ifdef MOZ_JAVA
    $(DIST)\lib\nsn32.lib \
!endif
    $(DIST)\lib\xppref32.lib \
    $(DIST)\lib\libreg32.lib \
!ifdef MOZ_JAVA
    $(DIST)\lib\libapplet32.lib \
!endif
    $(DIST)\lib\hook.lib \
#!if defined(EDITOR)
!ifdef MOZ_JAVA
    $(DIST)\lib\edtplug.lib \
!endif
#!endif
!ifndef NO_SECURITY
    $(DIST)\lib\secnav32.lib \
    $(DIST)\lib\export.lib \
    $(BINREL_DIST)\lib\ssl.lib \
    $(BINREL_DIST)\lib\pkcs12.lib \
    $(BINREL_DIST)\lib\pkcs7.lib \
    $(BINREL_DIST)\lib\secmod.lib \
    $(BINREL_DIST)\lib\cert.lib \
    $(BINREL_DIST)\lib\key.lib \
    $(BINREL_DIST)\lib\crypto.lib \
    $(BINREL_DIST)\lib\secutil.lib \
    $(BINREL_DIST)\lib\hash.lib \
!endif
!ifdef NO_SECURITY
    $(DIST)\lib\secfreenav32.lib \
!endif
    $(DIST)\lib\htmldg32.lib \
!ifdef MOZ_JAVA
    $(DIST)\lib\libnsc32.lib \
!endif
    $(DIST)\lib\img32.lib \
!ifdef MOZ_JAVA
    $(DIST)\lib\jmc.lib \
!endif
    $(DIST)\lib\font.lib \
    $(DIST)\lib\rdf32.lib \
    $(OUTDIR)\appicon.res \
    $(DIST)\lib\winfont.lib \
!ifdef MOZ_LDAP
!ifdef MOZ_JAVA
    $(DIST)\lib\nsldap32.lib \
!endif
!endif
    $(DIST)\lib\unicvt32.lib \
!ifdef MOZ_JAVA
    $(DIST)\lib\softup32.lib \
!ifndef NO_SECURITY
    $(DIST)\lib\jsl32.lib \
!endif
!endif
!ifdef MOZ_LOC_INDEP
    $(DIST)\lib\li32.lib \
!endif
    $(DIST)\lib\prgrss32.lib \
    $(DIST)\lib\sched32.lib \
    $(DIST)\lib\prefuuid.lib \
    $(DIST)\lib\png.lib \
    $(DIST)\lib\xpstrdll.lib \
!ifdef MOZ_MAIL_NEWS
	$(DIST)\lib\mnrc32.lib \
!endif
    $(DIST)\lib\zip$(MOZ_BITS)$(VERSION_NUMBER).lib \
    $(DIST)\lib\jpeg$(MOZ_BITS)$(VERSION_NUMBER).lib \
    $(DIST)\lib\dbm$(MOZ_BITS).lib \
!endif
    $(DIST)\lib\xpcom$(MOZ_BITS).lib \
    $(NULL)

##      Specify MFC libs before other libs and before .obj files,
##              such that _CrtDumpMemoryLeaks will be called
##              after all other global objects are destroyed.
##      This greatly reduces the amount of memory dumping after
##              exiting a debug build, and thus has much more
##              accurate data.  See #pragma init_seg help.
##
LINK_FLAGS= \
!if "$(MOZ_BITS)"=="32"
!if defined(MOZ_DEBUG)
!if "$(_MSC_VER)"=="1020" || "$(_MSC_VER)"=="1100"
    mfc42d.lib \
    mfco42d.lib \
    mfcs42d.lib \
!else
    mfc40d.lib \
    mfco40d.lib \
    mfcs40d.lib \
!endif
!else
!if "$(_MSC_VER)"=="1020" || "$(_MSC_VER)"=="1100"
    mfc42.lib \
    mfcs42.lib \
!else
    mfc40.lib \
    mfcs40.lib \
!endif
!endif
    $(LINK_LIBS) \
    version.lib rpcrt4.lib \
    $(LINKFLAGS_DEBUG) \
    kernel32.lib shell32.lib user32.lib gdi32.lib oldnames.lib advapi32.lib \
    comdlg32.lib uuid.lib oleaut32.lib ole32.lib \
!if "$(_MSC_VER)"!="1100"
	uuid2.lib \
!endif
    /subsystem:windows \
    /pdb:"$(OUTDIR)/mozilla.pdb" /machine:I386 \
!if !defined(MOZ_DEBUG)
    /fixed \
!endif        
    /nodefaultlib /out:"$(OUTDIR)/mozilla.exe"
!else
#    $(DIST)\lib\jmc.lib \
    $(LINKFLAGS_DEBUG)
!endif


# To control the warning level from the command line, just put WARN=x
# on the NMAKE command line where x = the desired warning level
!ifdef WARN
WARNINGS=/W$(WARN)
!else
WARNINGS=/W3
!endif

CFLAGS_GENERAL=/c $(WARNINGS) /Fo"$(OUTDIR)/" \
!if "$(MOZ_BITS)"=="32"
    /GX
!else
    /Fd"$(OUTDIR)/" /G3 /AL /Gt3 /Gx- /GA \
!if defined(MOZ_DEBUG)
!if "$(MOZ_USERNAME)" == "WHITEBOX"
    /Od /FR
!else
    /Od
!endif
!else
    /Gs
!endif
!endif

RCFLAGS_GENERAL= \
!if "$(MOZ_BITS)"=="32"
    /l 0x409
!else
    /r
!endif

#EXPORTINC=$(MOZ_SRC)\ns\exportinc
EXPORTINC=$(MOZ_SRC)\ns\dist\public\win16

# if you add something to CINCLUDES, you must also add it to the exports target
# at the end of the file.

CINCLUDES= \
    /I$(MOZ_SRC)\ns\include \
!if "$(MOZ_BITS)" == "32"
    /I$(MOZ_SRC)\ns\lib\layout \
    /I$(MOZ_SRC)\ns\lib\libstyle \
    /I$(MOZ_SRC)\ns\lib\liblayer\include \
!ifndef NO_SECURITY
    /I$(MOZ_SRC)\ns\lib\libjar \
!endif
    /I$(MOZ_SRC)\ns\lib\libnet \
    /I$(MOZ_SRC)\ns\lib\libcnv \
    /I$(MOZ_SRC)\ns\lib\libi18n \
    /I$(MOZ_SRC)\ns\lib\libparse \
    /I$(MOZ_SRC)\ns\lib\plugin \
!ifdef MOZ_MAIL_NEWS
    /I$(MOZ_SRC)\ns\lib\libmsg \
    /I$(MOZ_SRC)\ns\lib\libaddr \
    /I$(MOZ_SRC)\ns\lib\libneo \
!endif
!else
    /I$(EXPORTINC)
!endif

RCINCLUDES=$(MOZ_SRC)\ns\cmd\winfe;$(MOZ_SRC)\ns\include

CDEPENDINCLUDES= \
    /I$(MOZ_SRC)\ns\cmd\winfe \
    /I$(MOZ_SRC)\ns\jpeg

# if you add something to CDISTINCLUDES, you must also add it to the exports target
# at the end of the file.

CDISTINCLUDES= \
!if "$(MOZ_BITS)" == "32"
    /I$(DIST)\include \
    /I$(XPDIST)\public\dbm \
    /I$(XPDIST)\public\java \
    /I$(XPDIST)\public\applet \
    /I$(XPDIST)\public\libreg \
    /I$(XPDIST)\public\hook \
    /I$(XPDIST)\public\pref \
    /I$(XPDIST)\public\libfont \
    /I$(XPDIST)\public\winfont \
    /I$(XPDIST)\public\js \
    /I$(XPDIST)\public\jsdebug \
    /I$(XPDIST)\public\security \
    /I$(XPDIST)\public\htmldlgs \
    /I$(XPDIST)\public\softupdt \
#!ifdef MOZ_LOC_INDEP
	/I$(XPDIST)\public\li \
#!endif MOZ_LOC_INDEP
	/I$(XPDIST)\public\progress \
    /I$(XPDIST)\public\schedulr \
    /I$(XPDIST)\public\xpcom \
#!ifdef EDITOR
!ifdef MOZ_JAVA
    /I$(XPDIST)\public\edtplug \
!endif
    /I$(XPDIST)\public\spellchk \
#!endif
#!ifdef MOZ_LDAP
    /I$(XPDIST)\public\ldap \
#!endif
    /I$(XPDIST)\public\rdf \
    /I$(DIST)\include \
    /I$(XPDIST)\public\img \
    /I$(XPDIST)\public\jtools \
!else
!endif
    /I$(XPDIST)\public \
    /I$(XPDIST)\public\coreincl \
    /I$(XPDIST)\public\util     
RCDISTINCLUDES=$(DIST)\include;$(XPDIST)\public\security

#Added MQUOTE
CDEFINES=/DXP_PC /Dx386 /D_WINDOWS /D_X86_ \
    /DMOCHA \
    /D_MBCS \
    /DEDIT_REMOTE /DLAYERS /DMQUOTE \
	/D_IMAGE_CONVERT \
	/D_IME_COMPOSITION \
!if "$(MOZ_BITS)" == "32"
!ifndef NSPR20
    /DWIN32 /DJAVA_WIN32 /DHW_THREADS /D_AFXDLL \
!else
    /DWIN32 /DJAVA_WIN32 /DNSPR20 /D_AFXDLL \
!endif
!if defined(MSVC4)
    /DMSVC4 \
!endif
!else
!if defined(NSPR20)
	/DNSPR20 \
!endif
!endif
!if defined(MOZ_JAVA)
    /DJAVA \
!endif
    /DMOZILLA_CLIENT


# MOZ_LITENESS_FLAGS deal with MOZ_LIGHT, MOZ_MEDIUM
CDEFINES=$(CDEFINES) $(MOZ_LITENESS_FLAGS)

# Don't add anything to RCDEFINES that needs to be there for Win16
# or the RC command line will be too long

RCDEFINES=/DRESOURCE_STR /D_WINDOWS \
!if defined(MOZ_JAVA)
     /DJAVA \
!endif
!if "$(MOZ_BITS)" == "32"
    /DXP_PC /Dx386 /D_X86_ \
    /DLAYERS /DMQUOTE /D_AFXDLL /D_MBCS \
    /DWIN32 /DJAVA_WIN32 /DHW_THREADS \
!if defined(MSVC4)
    /DMSVC4 \
!endif
	/D_IMAGE_CONVERT \
!endif
    /DMOZILLA_CLIENT

# MOZ_LITENESS_FLAGS deal with MOZ_LITE, MOZ_MEDIUM
RCDEFINES=$(RCDEFINES) $(MOZ_LITENESS_FLAGS)

CFILEFLAGS=$(CFLAGS_GENERAL) ^
    $(CDEFINES) ^
    $(CINCLUDES) ^
    $(CDISTINCLUDES) 


RCFILEFLAGS=$(RCFLAGS_GENERAL)\
    $(RCFLAGS_DEBUG)\
    $(RCDEFINES)

#
# if depend is defined, the default is to build depandancies
#

!IFDEF DEPEND

all: "$(OUTDIR)" $(MOZ_SRC)\ns\cmd\winfe\mkfiles32\makedep.exe $(OUTDIR)\mozilla.dep

$(OUTDIR)\mozilla.dep: $(MOZ_SRC)\ns\cmd\winfe\mkfiles32\mozilla.mak
    @rem <<$(PROD)$(VERSTR).dep
	$(CINCLUDES) $(CDISTINCLUDES) $(CDEPENDINCLUDES) -O $(OUTDIR)\mozilla.dep
!IF "$(MOZ_BITS)"=="16"
    -16
!ENDIF
<<
    $(MOZ_SRC)\ns\cmd\winfe\mkfiles32\makedep.exe @$(PROD)$(VERSTR).dep -F <<
	$(MOZ_SRC)\ns\lib\liblayer\src\cl_comp.c
	$(MOZ_SRC)\ns\lib\liblayer\src\cl_drwbl.c
	$(MOZ_SRC)\ns\lib\liblayer\src\cl_layer.c
	$(MOZ_SRC)\ns\lib\liblayer\src\cl_group.c
	$(MOZ_SRC)\ns\lib\liblayer\src\cl_util.c
	$(MOZ_SRC)\ns\lib\liblayer\src\xp_rect.c

	$(MOZ_SRC)\ns\lib\layout\bullet.c  
	$(MOZ_SRC)\ns\lib\layout\clipline.c
!ifdef EDITOR
	$(MOZ_SRC)\ns\lib\layout\editor.cpp   
	$(MOZ_SRC)\ns\lib\layout\edtbuf.cpp   
	$(MOZ_SRC)\ns\lib\layout\edtcmd.cpp   
	$(MOZ_SRC)\ns\lib\layout\edtele.cpp   
	$(MOZ_SRC)\ns\lib\layout\edtjava.cpp   
	$(MOZ_SRC)\ns\lib\layout\edtsave.cpp   
	$(MOZ_SRC)\ns\lib\layout\edtutil.cpp   
!endif
	$(MOZ_SRC)\ns\lib\layout\layedit.c 
	$(MOZ_SRC)\ns\lib\layout\fsfile.cpp   
	$(MOZ_SRC)\ns\lib\layout\streams.cpp   
	$(MOZ_SRC)\ns\lib\layout\layarena.c
	$(MOZ_SRC)\ns\lib\layout\layblock.c 
	$(MOZ_SRC)\ns\lib\layout\laycell.c 
	$(MOZ_SRC)\ns\lib\layout\laycols.c 
	$(MOZ_SRC)\ns\lib\layout\laydisp.c 
	$(MOZ_SRC)\ns\lib\layout\layembed.c
	$(MOZ_SRC)\ns\lib\layout\layfind.c 
	$(MOZ_SRC)\ns\lib\layout\layform.c 
	$(MOZ_SRC)\ns\lib\layout\layfree.c 
	$(MOZ_SRC)\ns\lib\layout\laygrid.c 
	$(MOZ_SRC)\ns\lib\layout\layhrule.c
	$(MOZ_SRC)\ns\lib\layout\layimage.c
	$(MOZ_SRC)\ns\lib\layout\layinfo.c 
!if defined(MOZ_JAVA)
	$(MOZ_SRC)\ns\lib\layout\layjava.c 
!endif
	$(MOZ_SRC)\ns\lib\layout\laylayer.c 
	$(MOZ_SRC)\ns\lib\layout\laylist.c 
	$(MOZ_SRC)\ns\lib\layout\laymap.c  
	$(MOZ_SRC)\ns\lib\layout\laymocha.c
	$(MOZ_SRC)\ns\lib\layout\layobj.c
	$(MOZ_SRC)\ns\lib\layout\layout.c  
	$(MOZ_SRC)\ns\lib\layout\layscrip.c
	$(MOZ_SRC)\ns\lib\layout\laystyle.c
	$(MOZ_SRC)\ns\lib\layout\laysel.c  
	$(MOZ_SRC)\ns\lib\layout\layspace.c  
	$(MOZ_SRC)\ns\lib\layout\laysub.c  
	$(MOZ_SRC)\ns\lib\layout\laytable.c
	$(MOZ_SRC)\ns\lib\layout\laytags.c 
	$(MOZ_SRC)\ns\lib\layout\laytext.c 
	$(MOZ_SRC)\ns\lib\layout\layutil.c 
	$(MOZ_SRC)\ns\lib\layout\ptinpoly.c
	$(MOZ_SRC)\ns\lib\layout\layrelay.c 
	$(MOZ_SRC)\ns\lib\layout\laytrav.c 

!ifdef MOZ_MAIL_NEWS
	$(MOZ_SRC)\ns\lib\libaddr\line64.c
	$(MOZ_SRC)\ns\lib\libaddr\vobject.c
	$(MOZ_SRC)\ns\lib\libaddr\vcc.c
	$(MOZ_SRC)\ns\lib\libaddr\ab.cpp  
	$(MOZ_SRC)\ns\lib\libaddr\abcntxt.cpp  
	$(MOZ_SRC)\ns\lib\libaddr\abentry.cpp  
	$(MOZ_SRC)\ns\lib\libaddr\abinfo.cpp  
	$(MOZ_SRC)\ns\lib\libaddr\ablist.cpp  
	$(MOZ_SRC)\ns\lib\libaddr\addbook.cpp  
	$(MOZ_SRC)\ns\lib\libaddr\abpane.cpp  
	$(MOZ_SRC)\ns\lib\libaddr\nickindx.cpp  
	$(MOZ_SRC)\ns\lib\libaddr\tyindex.cpp  
	$(MOZ_SRC)\ns\lib\libaddr\import.cpp
	$(MOZ_SRC)\ns\lib\libaddr\export.cpp
	$(MOZ_SRC)\ns\lib\libaddr\abundoac.cpp
	$(MOZ_SRC)\ns\lib\libaddr\abglue.cpp
	$(MOZ_SRC)\ns\lib\libaddr\abcinfo.cpp
	$(MOZ_SRC)\ns\lib\libaddr\abcpane.cpp
	$(MOZ_SRC)\ns\lib\libaddr\abpane2.cpp
	$(MOZ_SRC)\ns\lib\libaddr\abntfy.cpp
!endif

	$(MOZ_SRC)\ns\lib\libi18n\detectu2.c
	$(MOZ_SRC)\ns\lib\libi18n\metatag.c
	$(MOZ_SRC)\ns\lib\libi18n\autokr.c
	$(MOZ_SRC)\ns\lib\libi18n\autocvt.c
	$(MOZ_SRC)\ns\lib\libi18n\b52cns.c 
	$(MOZ_SRC)\ns\lib\libi18n\cns2b5.c 
	$(MOZ_SRC)\ns\lib\libi18n\cvchcode.c   
	$(MOZ_SRC)\ns\lib\libi18n\euc2jis.c
	$(MOZ_SRC)\ns\lib\libi18n\euc2sjis.c   
	$(MOZ_SRC)\ns\lib\libi18n\euckr2is.c   
	$(MOZ_SRC)\ns\lib\libi18n\fe_ccc.c 
	$(MOZ_SRC)\ns\lib\libi18n\doc_ccc.c 
	$(MOZ_SRC)\ns\lib\libi18n\intl_csi.c 
	$(MOZ_SRC)\ns\lib\libi18n\is2euckr.c   
	$(MOZ_SRC)\ns\lib\libi18n\intl_csi.c   
	$(MOZ_SRC)\ns\lib\libi18n\jis2oth.c
	$(MOZ_SRC)\ns\lib\libi18n\nscstr.c
	$(MOZ_SRC)\ns\lib\libi18n\sjis2euc.c   
	$(MOZ_SRC)\ns\lib\libi18n\sjis2jis.c   
	$(MOZ_SRC)\ns\lib\libi18n\ucs2.c   
	$(MOZ_SRC)\ns\lib\libi18n\ugen.c   
	$(MOZ_SRC)\ns\lib\libi18n\ugendata.c   
	$(MOZ_SRC)\ns\lib\libi18n\umap.c   
	$(MOZ_SRC)\ns\lib\libi18n\uscan.c   
!IF "$(MOZ_BITS)"=="16"
	$(MOZ_SRC)\ns\lib\libi18n\unicvt.c
!ENDIF
	$(MOZ_SRC)\ns\lib\libi18n\fontencd.c
	$(MOZ_SRC)\ns\lib\libi18n\csnamefn.c
	$(MOZ_SRC)\ns\lib\libi18n\csnametb.c
	$(MOZ_SRC)\ns\lib\libi18n\mime2fun.c
	$(MOZ_SRC)\ns\lib\libi18n\sbconvtb.c
	$(MOZ_SRC)\ns\lib\libi18n\acptlang.c
	$(MOZ_SRC)\ns\lib\libi18n\csstrlen.c
	$(MOZ_SRC)\ns\lib\libi18n\sblower.c
	$(MOZ_SRC)\ns\lib\libi18n\intlcomp.c
	$(MOZ_SRC)\ns\lib\libi18n\dblower.c
	$(MOZ_SRC)\ns\lib\libi18n\kinsokud.c
	$(MOZ_SRC)\ns\lib\libi18n\kinsokuf.c
	$(MOZ_SRC)\ns\lib\libi18n\net_junk.c 
	$(MOZ_SRC)\ns\lib\libi18n\katakana.c 
!ifndef NO_SECURITY
	$(MOZ_SRC)\ns\lib\libjar\zig.c
	$(MOZ_SRC)\ns\lib\libjar\zigsign.c
	$(MOZ_SRC)\ns\lib\libjar\zigver.c
	$(MOZ_SRC)\ns\lib\libjar\zig-ds.c
	$(MOZ_SRC)\ns\lib\libjar\zigevil.c
!endif
	$(MOZ_SRC)\ns\lib\libcnv\libcnv.c 
	$(MOZ_SRC)\ns\lib\libcnv\writejpg.c 
	$(MOZ_SRC)\ns\lib\libcnv\colorqnt.c 
	$(MOZ_SRC)\ns\lib\libcnv\readbmp.c 
	$(MOZ_SRC)\ns\lib\libcnv\libppm3.c 

!ifdef MOZ_MAIL_NEWS
	$(MOZ_SRC)\ns\lib\libmime\mimecont.c
	$(MOZ_SRC)\ns\lib\libmime\mimecryp.c
	$(MOZ_SRC)\ns\lib\libmime\mimeebod.c
	$(MOZ_SRC)\ns\lib\libmime\mimeenc.c
	$(MOZ_SRC)\ns\lib\libmime\mimeeobj.c
	$(MOZ_SRC)\ns\lib\libmime\mimehdrs.c
	$(MOZ_SRC)\ns\lib\libmime\mimei.c
	$(MOZ_SRC)\ns\lib\libmime\mimeiimg.c
	$(MOZ_SRC)\ns\lib\libmime\mimeleaf.c
	$(MOZ_SRC)\ns\lib\libmime\mimemalt.c
	$(MOZ_SRC)\ns\lib\libmime\mimemapl.c
	$(MOZ_SRC)\ns\lib\libmime\mimemdig.c
	$(MOZ_SRC)\ns\lib\libmime\mimemmix.c
	$(MOZ_SRC)\ns\lib\libmime\mimemoz.c
	$(MOZ_SRC)\ns\lib\libmime\mimempar.c
!ifndef NO_SECURITY
	$(MOZ_SRC)\ns\lib\libmime\mimempkc.c
!endif
	$(MOZ_SRC)\ns\lib\libmime\mimemrel.c
	$(MOZ_SRC)\ns\lib\libmime\mimemsg.c
	$(MOZ_SRC)\ns\lib\libmime\mimemsig.c
	$(MOZ_SRC)\ns\lib\libmime\mimemult.c
	$(MOZ_SRC)\ns\lib\libmime\mimeobj.c
	$(MOZ_SRC)\ns\lib\libmime\mimepbuf.c
!ifndef NO_SECURITY
	$(MOZ_SRC)\ns\lib\libmime\mimepkcs.c
!endif
	$(MOZ_SRC)\ns\lib\libmime\mimesun.c
	$(MOZ_SRC)\ns\lib\libmime\mimetenr.c
	$(MOZ_SRC)\ns\lib\libmime\mimetext.c
	$(MOZ_SRC)\ns\lib\libmime\mimethtm.c
	$(MOZ_SRC)\ns\lib\libmime\mimetpla.c
	$(MOZ_SRC)\ns\lib\libmime\mimetric.c
	$(MOZ_SRC)\ns\lib\libmime\mimeunty.c
	$(MOZ_SRC)\ns\lib\libmime\mimevcrd.c
	$(MOZ_SRC)\ns\lib\libmime\mimedrft.c
	$(MOZ_SRC)\ns\lib\libmisc\mime.c   
	$(MOZ_SRC)\ns\lib\libmisc\dirprefs.c
!endif
 
	$(MOZ_SRC)\ns\lib\libmisc\glhist.c 
	$(MOZ_SRC)\ns\lib\libmisc\hotlist.c
	$(MOZ_SRC)\ns\lib\libmisc\shist.c  
	$(MOZ_SRC)\ns\lib\libmisc\undo.c   

	$(MOZ_SRC)\ns\lib\libmocha\et_mocha.c
	$(MOZ_SRC)\ns\lib\libmocha\et_moz.c
	$(MOZ_SRC)\ns\lib\libmocha\lm_applt.c
	$(MOZ_SRC)\ns\lib\libmocha\lm_bars.c
	$(MOZ_SRC)\ns\lib\libmocha\lm_cmpnt.c
!ifndef NO_SECURITY
	$(MOZ_SRC)\ns\lib\libmocha\lm_crypt.c
!endif
	$(MOZ_SRC)\ns\lib\libmocha\lm_doc.c
	$(MOZ_SRC)\ns\lib\libmocha\lm_embed.c
	$(MOZ_SRC)\ns\lib\libmocha\lm_event.c
	$(MOZ_SRC)\ns\lib\libmocha\lm_form.c   
	$(MOZ_SRC)\ns\lib\libmocha\lm_hardw.c   
	$(MOZ_SRC)\ns\lib\libmocha\lm_hist.c   
	$(MOZ_SRC)\ns\lib\libmocha\lm_href.c   
	$(MOZ_SRC)\ns\lib\libmocha\lm_img.c
	$(MOZ_SRC)\ns\lib\libmocha\lm_init.c   
	$(MOZ_SRC)\ns\lib\libmocha\lm_input.c  
	$(MOZ_SRC)\ns\lib\libmocha\lm_layer.c  
	$(MOZ_SRC)\ns\lib\libmocha\lm_nav.c
	$(MOZ_SRC)\ns\lib\libmocha\lm_plgin.c  
	$(MOZ_SRC)\ns\lib\libmocha\lm_screen.c  
	$(MOZ_SRC)\ns\lib\libmocha\lm_supdt.c  
	$(MOZ_SRC)\ns\lib\libmocha\lm_taint.c  
	$(MOZ_SRC)\ns\lib\libmocha\lm_trggr.c  
	$(MOZ_SRC)\ns\lib\libmocha\lm_url.c
	$(MOZ_SRC)\ns\lib\libmocha\lm_win.c
!ifndef NO_SECURITY
	$(MOZ_SRC)\ns\lib\libmocha\lm_pk11.c
!endif
!if "$(MOZ_BITS)" == "32"
!ifdef MOZ_JAVA
	$(MOZ_SRC)\ns\lib\libmocha\lm_jsd.c
!endif
!endif

!ifdef MOZ_MAIL_NEWS
	$(MOZ_SRC)\ns\lib\libmsg\ad_strm.c 
	$(MOZ_SRC)\ns\lib\libmsg\msgppane.cpp 
	$(MOZ_SRC)\ns\lib\libmsg\addr.c
	$(MOZ_SRC)\ns\lib\libmsg\ap_decod.c
	$(MOZ_SRC)\ns\lib\libmsg\ap_encod.c
	$(MOZ_SRC)\ns\lib\libmsg\appledbl.c
	$(MOZ_SRC)\ns\lib\libmsg\bh_strm.c 
	$(MOZ_SRC)\ns\lib\libmsg\bytearr.cpp
	$(MOZ_SRC)\ns\lib\libmsg\chngntfy.cpp
!ifndef NO_SECURITY
	$(MOZ_SRC)\ns\lib\libmsg\composec.c
!endif
	$(MOZ_SRC)\ns\lib\libmsg\dwordarr.cpp
	$(MOZ_SRC)\ns\lib\libmsg\eneoidar.cpp
	$(MOZ_SRC)\ns\lib\libmsg\filters.cpp
	$(MOZ_SRC)\ns\lib\libmsg\grec.cpp
	$(MOZ_SRC)\ns\lib\libmsg\grpinfo.cpp
	$(MOZ_SRC)\ns\lib\libmsg\hashtbl.cpp
	$(MOZ_SRC)\ns\lib\libmsg\hosttbl.cpp
	$(MOZ_SRC)\ns\lib\libmsg\idarray.cpp
	$(MOZ_SRC)\ns\lib\libmsg\imaphost.cpp
	$(MOZ_SRC)\ns\lib\libmsg\jsmsg.cpp
	$(MOZ_SRC)\ns\lib\libmsg\listngst.cpp
	$(MOZ_SRC)\ns\lib\libmsg\m_binhex.c
	$(MOZ_SRC)\ns\lib\libmsg\maildb.cpp
	$(MOZ_SRC)\ns\lib\libmsg\mailhdr.cpp
	$(MOZ_SRC)\ns\lib\libmsg\mhtmlstm.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgbg.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgbgcln.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgbiff.c
	$(MOZ_SRC)\ns\lib\libmsg\msgcpane.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgccach.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgcflds.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgcmfld.cpp 
	$(MOZ_SRC)\ns\lib\libmsg\msgdb.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgdbini.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgdbvw.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgdoc.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgdwnof.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgdlqml.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgfcach.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgfinfo.cpp
	$(MOZ_SRC)\ns\lib\libmsg\imapoff.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgimap.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgfpane.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgglue.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msghdr.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msglpane.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msglsrch.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgmast.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgmapi.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgmpane.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgmsrch.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgnsrch.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgoffnw.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgpane.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgppane.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgprefs.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgpurge.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgrulet.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgsec.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgsend.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgsendp.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgspane.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgtpane.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgundmg.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgundac.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgurlq.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgutils.c
	$(MOZ_SRC)\ns\lib\libmsg\msgzap.cpp
	$(MOZ_SRC)\ns\lib\libmsg\msgmdn.cpp
	$(MOZ_SRC)\ns\lib\libmsg\newsdb.cpp
	$(MOZ_SRC)\ns\lib\libmsg\newshdr.cpp 
	$(MOZ_SRC)\ns\lib\libmsg\newshost.cpp 
	$(MOZ_SRC)\ns\lib\libmsg\newspane.cpp 
	$(MOZ_SRC)\ns\lib\libmsg\newsset.cpp 
	$(MOZ_SRC)\ns\lib\libmsg\nwsartst.cpp 
	$(MOZ_SRC)\ns\lib\libmsg\prsembst.cpp 
	$(MOZ_SRC)\ns\lib\libmsg\ptrarray.cpp
	$(MOZ_SRC)\ns\lib\libmsg\search.cpp
	$(MOZ_SRC)\ns\lib\libmsg\subline.cpp
	$(MOZ_SRC)\ns\lib\libmsg\subpane.cpp
	$(MOZ_SRC)\ns\lib\libmsg\thrdbvw.cpp
	$(MOZ_SRC)\ns\lib\libmsg\thrhead.cpp
	$(MOZ_SRC)\ns\lib\libmsg\thrlstst.cpp
	$(MOZ_SRC)\ns\lib\libmsg\thrnewvw.cpp
	$(MOZ_SRC)\ns\lib\libmsg\mozdb.cpp

	$(MOZ_SRC)\ns\lib\libneo\enstring.cpp
	$(MOZ_SRC)\ns\lib\libneo\enswizz.cpp
	$(MOZ_SRC)\ns\lib\libneo\nappl.cpp
	$(MOZ_SRC)\ns\lib\libneo\nappsa.cpp
	$(MOZ_SRC)\ns\lib\libneo\narray.cpp
	$(MOZ_SRC)\ns\lib\libneo\nblob.cpp
	$(MOZ_SRC)\ns\lib\libneo\nclass.cpp
	$(MOZ_SRC)\ns\lib\libneo\ncstream.cpp
	$(MOZ_SRC)\ns\lib\libneo\ndata.cpp
	$(MOZ_SRC)\ns\lib\libneo\ndblndx.cpp
	$(MOZ_SRC)\ns\lib\libneo\ndoc.cpp
	$(MOZ_SRC)\ns\lib\libneo\nfltndx.cpp
	$(MOZ_SRC)\ns\lib\libneo\nformat.cpp
	$(MOZ_SRC)\ns\lib\libneo\nfree.cpp
	$(MOZ_SRC)\ns\lib\libneo\nfstream.cpp
	$(MOZ_SRC)\ns\lib\libneo\nidindex.cpp
	$(MOZ_SRC)\ns\lib\libneo\nidlist.cpp
	$(MOZ_SRC)\ns\lib\libneo\nindexit.cpp
	$(MOZ_SRC)\ns\lib\libneo\ninode.cpp
	$(MOZ_SRC)\ns\lib\libneo\nioblock.cpp
	$(MOZ_SRC)\ns\lib\libneo\niter.cpp
	$(MOZ_SRC)\ns\lib\libneo\nlaundry.cpp
	$(MOZ_SRC)\ns\lib\libneo\nlongndx.cpp
	$(MOZ_SRC)\ns\lib\libneo\nmeta.cpp
	$(MOZ_SRC)\ns\lib\libneo\nmrswsem.cpp
	$(MOZ_SRC)\ns\lib\libneo\nmsem.cpp
	$(MOZ_SRC)\ns\lib\libneo\nnode.cpp
	$(MOZ_SRC)\ns\lib\libneo\nnstrndx.cpp
	$(MOZ_SRC)\ns\lib\libneo\noffsprn.cpp
	$(MOZ_SRC)\ns\lib\libneo\npartmgr.cpp
	$(MOZ_SRC)\ns\lib\libneo\npersist.cpp
	$(MOZ_SRC)\ns\lib\libneo\npliter.cpp
	$(MOZ_SRC)\ns\lib\libneo\nquery.cpp
	$(MOZ_SRC)\ns\lib\libneo\nselect.cpp
	$(MOZ_SRC)\ns\lib\libneo\nsselect.cpp
	$(MOZ_SRC)\ns\lib\libneo\nstream.cpp
	$(MOZ_SRC)\ns\lib\libneo\nstrndx.cpp
	$(MOZ_SRC)\ns\lib\libneo\nsub.cpp
	$(MOZ_SRC)\ns\lib\libneo\nthread.cpp
	$(MOZ_SRC)\ns\lib\libneo\ntrans.cpp
	$(MOZ_SRC)\ns\lib\libneo\nulngndx.cpp
	$(MOZ_SRC)\ns\lib\libneo\nutils.cpp
	$(MOZ_SRC)\ns\lib\libneo\nwselect.cpp
	$(MOZ_SRC)\ns\lib\libneo\semnspr.cpp
	$(MOZ_SRC)\ns\lib\libneo\thrnspr.cpp
	$(MOZ_SRC)\ns\lib\libnet\mkabook.cpp
!endif

	$(MOZ_SRC)\ns\lib\libnet\cvactive.c
	$(MOZ_SRC)\ns\lib\libnet\cvcolor.c 
	$(MOZ_SRC)\ns\lib\libnet\cvdisk.c  
	$(MOZ_SRC)\ns\lib\libnet\cvproxy.c 
	$(MOZ_SRC)\ns\lib\libnet\cvunzip.c 
	$(MOZ_SRC)\ns\lib\libnet\cvchunk.c 
	$(MOZ_SRC)\ns\lib\libnet\extcache.c
	$(MOZ_SRC)\ns\lib\libnet\mkaccess.c
	$(MOZ_SRC)\ns\lib\libnet\mkautocf.c
	$(MOZ_SRC)\ns\lib\libnet\mkcache.c 
	$(MOZ_SRC)\ns\lib\libnet\mkconect.c
	$(MOZ_SRC)\ns\lib\libnet\mkdaturl.c
	$(MOZ_SRC)\ns\lib\libnet\mkextcac.c
	$(MOZ_SRC)\ns\lib\libnet\mkfile.c  
	$(MOZ_SRC)\ns\lib\libnet\mkformat.c
	$(MOZ_SRC)\ns\lib\libnet\mkfsort.c 
	$(MOZ_SRC)\ns\lib\libnet\mkftp.c   
	$(MOZ_SRC)\ns\lib\libnet\mkgeturl.c
	$(MOZ_SRC)\ns\lib\libnet\mkgopher.c
	$(MOZ_SRC)\ns\lib\libnet\mkhelp.c  
	$(MOZ_SRC)\ns\lib\libnet\mkhttp.c  
	$(MOZ_SRC)\ns\lib\libnet\mkinit.c  
	$(MOZ_SRC)\ns\lib\libnet\mktrace.c
	$(MOZ_SRC)\ns\lib\libnet\cvmime.c 
	$(MOZ_SRC)\ns\lib\libnet\mkpadpac.c
	$(MOZ_SRC)\ns\lib\libnet\jscookie.c 
	$(MOZ_SRC)\ns\lib\libnet\prefetch.c 
	$(MOZ_SRC)\ns\lib\libnet\mkjscfg.c
	$(MOZ_SRC)\ns\lib\libnet\cvsimple.c
!ifdef MOZ_MAIL_NEWS
	$(MOZ_SRC)\ns\lib\libnet\mkcertld.c
	$(MOZ_SRC)\ns\lib\libnet\imap4url.c
	$(MOZ_SRC)\ns\lib\libnet\imapearl.cpp
	$(MOZ_SRC)\ns\lib\libnet\imaphier.cpp
	$(MOZ_SRC)\ns\lib\libnet\imappars.cpp
	$(MOZ_SRC)\ns\lib\libnet\imapbody.cpp
	$(MOZ_SRC)\ns\lib\libnet\mkimap4.cpp 
	$(MOZ_SRC)\ns\lib\libnet\mkldap.cpp  
	$(MOZ_SRC)\ns\lib\libnet\mkmailbx.c
	$(MOZ_SRC)\ns\lib\libnet\mknews.c  
	$(MOZ_SRC)\ns\lib\libnet\mknewsgr.c
	$(MOZ_SRC)\ns\lib\libnet\mkpop3.c  
	$(MOZ_SRC)\ns\lib\libnet\mksmtp.c  
!endif
!if defined(MOZ_JAVA)
	$(MOZ_SRC)\ns\lib\libnet\mkmarimb.cpp
!endif
	$(MOZ_SRC)\ns\lib\libnet\mkmessag.c
	$(MOZ_SRC)\ns\lib\libnet\mkmemcac.c
	$(MOZ_SRC)\ns\lib\libnet\mkmocha.c 
	$(MOZ_SRC)\ns\lib\libnet\mkparse.c 
	$(MOZ_SRC)\ns\lib\libnet\mkremote.c
	$(MOZ_SRC)\ns\lib\libnet\mkselect.c
	$(MOZ_SRC)\ns\lib\libnet\mksockrw.c
	$(MOZ_SRC)\ns\lib\libnet\mksort.c  
	$(MOZ_SRC)\ns\lib\libnet\mkstream.c
	$(MOZ_SRC)\ns\lib\libnet\mkutils.c 
	$(MOZ_SRC)\ns\lib\libnet\jsautocf.c
	$(MOZ_SRC)\ns\lib\libnet\txview.c 

	$(MOZ_SRC)\ns\lib\libparse\pa_amp.c
	$(MOZ_SRC)\ns\lib\libparse\pa_hash.c   
	$(MOZ_SRC)\ns\lib\libparse\pa_hook.c   
	$(MOZ_SRC)\ns\lib\libparse\pa_mdl.c
	$(MOZ_SRC)\ns\lib\libparse\pa_parse.c  

	$(MOZ_SRC)\ns\lib\libstyle\libstyle.c
	$(MOZ_SRC)\ns\lib\libstyle\csslex.c
	$(MOZ_SRC)\ns\lib\libstyle\csstab.c
	$(MOZ_SRC)\ns\lib\libstyle\csstojs.c
	$(MOZ_SRC)\ns\lib\libstyle\jssrules.c
	$(MOZ_SRC)\ns\lib\libstyle\stystack.c
	$(MOZ_SRC)\ns\lib\libstyle\stystruc.c

	$(MOZ_SRC)\ns\modules\libutil\src\obs.c
!if "$(MOZ_BITS)"=="16"
	$(MOZ_SRC)\ns\modules\libimg\src\color.c
	$(MOZ_SRC)\ns\modules\libimg\src\colormap.c
	$(MOZ_SRC)\ns\modules\libimg\src\dither.c
	$(MOZ_SRC)\ns\modules\libimg\src\dummy_nc.c
	$(MOZ_SRC)\ns\modules\libimg\src\external.c
	$(MOZ_SRC)\ns\modules\libimg\src\gif.c
	$(MOZ_SRC)\ns\modules\libimg\src\if.c
	$(MOZ_SRC)\ns\modules\libimg\src\ilclient.c
	$(MOZ_SRC)\ns\modules\libimg\src\il_util.c
	$(MOZ_SRC)\ns\modules\libimg\src\jpeg.c
	$(MOZ_SRC)\ns\modules\libimg\src\MIMGCB.c
	$(MOZ_SRC)\ns\modules\libimg\src\scale.c
	$(MOZ_SRC)\ns\modules\libimg\src\xbm.c
	$(MOZ_SRC)\ns\modules\libimg\src\ipng.c
	$(MOZ_SRC)\ns\modules\libimg\src\png_png.c
!if defined(MOZ_JAVA)
	$(MOZ_SRC)\ns\sun-java\jtools\src\jmc.c
!endif
!endif

	$(MOZ_SRC)\ns\lib\plugin\npassoc.c 
	$(MOZ_SRC)\ns\lib\plugin\npglue.cpp
	$(MOZ_SRC)\ns\lib\plugin\npwplat.cpp 
	$(MOZ_SRC)\ns\lib\plugin\nsplugin.cpp 

	$(MOZ_SRC)\ns\lib\xlate\isotab.c   
	$(MOZ_SRC)\ns\lib\xlate\stubs.c
	$(MOZ_SRC)\ns\lib\xlate\tblprint.c 
	$(MOZ_SRC)\ns\lib\xlate\text.c 

	$(MOZ_SRC)\ns\lib\xp\allxpstr.c
	$(MOZ_SRC)\ns\lib\xp\xp_alloc.c
	$(MOZ_SRC)\ns\lib\xp\xp_cntxt.c
	$(MOZ_SRC)\ns\lib\xp\xp_core.c 
	$(MOZ_SRC)\ns\lib\xp\xp_error.c
	$(MOZ_SRC)\ns\lib\xp\xp_file.c 
	$(MOZ_SRC)\ns\lib\xp\xp_hash.c
	$(MOZ_SRC)\ns\lib\xp\xp_md5.c  
	$(MOZ_SRC)\ns\lib\xp\xp_mesg.c 
	$(MOZ_SRC)\ns\lib\xp\xp_ncent.c
	$(MOZ_SRC)\ns\lib\xp\xp_reg.c  
	$(MOZ_SRC)\ns\lib\xp\xp_rgb.c  
	$(MOZ_SRC)\ns\lib\xp\xp_sec.c  
	$(MOZ_SRC)\ns\lib\xp\xp_str.c  
	$(MOZ_SRC)\ns\lib\xp\xp_thrmo.c
	$(MOZ_SRC)\ns\lib\xp\xp_time.c 
	$(MOZ_SRC)\ns\lib\xp\xp_trace.c
	$(MOZ_SRC)\ns\lib\xp\xp_wrap.c 
	$(MOZ_SRC)\ns\lib\xp\xpassert.c
	$(MOZ_SRC)\ns\lib\xp\xp_list.c
	$(MOZ_SRC)\ns\lib\xp\xplocale.c

	$(MOZ_SRC)\ns\lib\libpwcac\pwcacapi.c

	$(MOZ_SRC)\ns\lib\libpics\cslabel.c
	$(MOZ_SRC)\ns\lib\libpics\csparse.c
	$(MOZ_SRC)\ns\lib\libpics\htchunk.c
	$(MOZ_SRC)\ns\lib\libpics\htstring.c
	$(MOZ_SRC)\ns\lib\libpics\htlist.c
	$(MOZ_SRC)\ns\lib\libpics\lablpars.c
	$(MOZ_SRC)\ns\lib\libpics\picsapi.c
!if "$(MOZ_BITS)" == "16"
	$(MOZ_SRC)\ns\nspr20\pr\src\md\windows\w16stdio.c
!endif
!ifndef MOZ_MAIL_NEWS
	$(MOZ_SRC)\ns\cmd\winfe\compmapi.cpp
!endif
!ifdef MOZ_MAIL_NEWS
	$(MOZ_SRC)\ns\cmd\winfe\addrfrm.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\addrdlg.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\abmldlg.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\advopdlg.cpp
	$(MOZ_SRC)\ns\cmd\winfe\compbar.cpp
	$(MOZ_SRC)\ns\cmd\winfe\compfe.cpp
	$(MOZ_SRC)\ns\cmd\winfe\compfile.cpp
	$(MOZ_SRC)\ns\cmd\winfe\compfrm.cpp
	$(MOZ_SRC)\ns\cmd\winfe\compfrm2.cpp
	$(MOZ_SRC)\ns\cmd\winfe\compmisc.cpp
!endif
!ifdef EDITOR
	$(MOZ_SRC)\ns\cmd\winfe\edframe.cpp
	$(MOZ_SRC)\ns\cmd\winfe\edprops.cpp
	$(MOZ_SRC)\ns\cmd\winfe\edtable.cpp
	$(MOZ_SRC)\ns\cmd\winfe\edview.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\edview2.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\eddialog.cpp
	$(MOZ_SRC)\ns\cmd\winfe\edlayout.cpp   
!endif
!ifdef MOZ_MAIL_NEWS
	$(MOZ_SRC)\ns\cmd\winfe\filter.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\edhdrdlg.cpp
	$(MOZ_SRC)\ns\cmd\winfe\mailfrm.cpp
	$(MOZ_SRC)\ns\cmd\winfe\mailfrm2.cpp
	$(MOZ_SRC)\ns\cmd\winfe\mailmisc.cpp
	$(MOZ_SRC)\ns\cmd\winfe\mailpriv.cpp
	$(MOZ_SRC)\ns\cmd\winfe\mailqf.cpp
	$(MOZ_SRC)\ns\cmd\winfe\mapihook.cpp
	$(MOZ_SRC)\ns\cmd\winfe\mapismem.cpp
	$(MOZ_SRC)\ns\cmd\winfe\mapimail.cpp
	$(MOZ_SRC)\ns\cmd\winfe\nsstrseq.cpp
	$(MOZ_SRC)\ns\cmd\winfe\mnprefs.cpp  
	$(MOZ_SRC)\ns\cmd\winfe\mnwizard.cpp  
	$(MOZ_SRC)\ns\cmd\winfe\msgfrm.cpp
	$(MOZ_SRC)\ns\cmd\winfe\msgtmpl.cpp
	$(MOZ_SRC)\ns\cmd\winfe\msgview.cpp
	$(MOZ_SRC)\ns\cmd\winfe\namcomp.cpp
	$(MOZ_SRC)\ns\cmd\winfe\numedit.cpp
	$(MOZ_SRC)\ns\cmd\winfe\srchfrm.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\subnews.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\taskbar.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\thrdfrm.cpp
!endif
!ifdef EDITOR
	$(MOZ_SRC)\ns\cmd\winfe\edtrccln.cpp
	$(MOZ_SRC)\ns\cmd\winfe\edtclass.cpp
	$(MOZ_SRC)\ns\cmd\winfe\spellcli.cpp
!endif
!ifdef MOZ_MAIL_NEWS
	$(MOZ_SRC)\ns\cmd\winfe\dlgdwnld.cpp
	$(MOZ_SRC)\ns\cmd\winfe\dlghtmmq.cpp
	$(MOZ_SRC)\ns\cmd\winfe\dlghtmrp.cpp
	$(MOZ_SRC)\ns\cmd\winfe\dlgseldg.cpp
   $(MOZ_SRC)\ns\cmd\winfe\nsadrlst.cpp
   $(MOZ_SRC)\ns\cmd\winfe\nsadrnam.cpp
   $(MOZ_SRC)\ns\cmd\winfe\nsadrtyp.cpp
	$(MOZ_SRC)\ns\cmd\winfe\fldrfrm.cpp        
   $(MOZ_SRC)\ns\cmd\winfe\dspppage.cpp
	$(MOZ_SRC)\ns\cmd\winfe\srchdlg.cpp
	$(MOZ_SRC)\ns\cmd\winfe\srchobj.cpp
	$(MOZ_SRC)\ns\cmd\winfe\mnrccln.cpp
!endif
	$(MOZ_SRC)\ns\cmd\winfe\setupwiz.cpp  
	$(MOZ_SRC)\ns\cmd\winfe\ngdwtrst.cpp
	$(MOZ_SRC)\ns\cmd\winfe\animbar.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\animbar2.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\apiapi.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\askmedlg.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\authdll.cpp
	$(MOZ_SRC)\ns\cmd\winfe\button.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\cfe.cpp
	$(MOZ_SRC)\ns\cmd\winfe\cmdparse.cpp
	$(MOZ_SRC)\ns\cmd\winfe\cntritem.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\confhook.cpp
	$(MOZ_SRC)\ns\cmd\winfe\csttlbr2.cpp
	$(MOZ_SRC)\ns\cmd\winfe\custom.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\cuvfm.cpp
	$(MOZ_SRC)\ns\cmd\winfe\cuvfs.cpp
	$(MOZ_SRC)\ns\cmd\winfe\cvffc.cpp
	$(MOZ_SRC)\ns\cmd\winfe\cxabstra.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\cxdc.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\cxdc1.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\cxicon.cpp
	$(MOZ_SRC)\ns\cmd\winfe\cxinit.cpp
	$(MOZ_SRC)\ns\cmd\winfe\cxmeta.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\cxnet1.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\cxpane.cpp
	$(MOZ_SRC)\ns\cmd\winfe\cxprint.cpp
	$(MOZ_SRC)\ns\cmd\winfe\cxprndlg.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\cxsave.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\cxstubs.cpp
	$(MOZ_SRC)\ns\cmd\winfe\cxwin.cpp
	$(MOZ_SRC)\ns\cmd\winfe\cxwin1.cpp
	$(MOZ_SRC)\ns\cmd\winfe\dateedit.cpp
	$(MOZ_SRC)\ns\cmd\winfe\dde.cpp
	$(MOZ_SRC)\ns\cmd\winfe\ddecmd.cpp
	$(MOZ_SRC)\ns\cmd\winfe\ddectc.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\dialog.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\display.cpp
	$(MOZ_SRC)\ns\cmd\winfe\dragbar.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\drawable.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\dropmenu.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\edcombtb.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\extgen.cpp
	$(MOZ_SRC)\ns\cmd\winfe\extview.cpp
	$(MOZ_SRC)\ns\cmd\winfe\feembed.cpp
	$(MOZ_SRC)\ns\cmd\winfe\fegrid.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\fegui.cpp  
	$(MOZ_SRC)\ns\cmd\winfe\feimage.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\feimages.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\feorphan.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\feorphn2.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\femess.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\fenet.cpp  
	$(MOZ_SRC)\ns\cmd\winfe\feselect.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\feutil.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\findrepl.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\fmabstra.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\fmbutton.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\fmfile.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\fmradio.cpp
	$(MOZ_SRC)\ns\cmd\winfe\fmrdonly.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\fmselmul.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\fmselone.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\fmtext.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\fmtxarea.cpp
	$(MOZ_SRC)\ns\cmd\winfe\frameglu.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\framinit.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\genchrom.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\gendoc.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\genedit.cpp
	$(MOZ_SRC)\ns\cmd\winfe\genframe.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\genfram2.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\prefs.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\genview.cpp
	$(MOZ_SRC)\ns\cmd\winfe\gridedge.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\helpers.cpp
	$(MOZ_SRC)\ns\cmd\winfe\hiddenfr.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\histbld.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\imagemap.cpp
!if "$(MOZ_BITS)"  == "32"
	$(MOZ_SRC)\ns\cmd\winfe\intelli.cpp
!endif
	$(MOZ_SRC)\ns\cmd\winfe\intlwin.cpp
	$(MOZ_SRC)\ns\cmd\winfe\ipframe.cpp
	$(MOZ_SRC)\ns\cmd\winfe\lastacti.cpp
	$(MOZ_SRC)\ns\cmd\winfe\logindg.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\mainfrm.cpp
	$(MOZ_SRC)\ns\cmd\winfe\medit.cpp  
	$(MOZ_SRC)\ns\cmd\winfe\mozock.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\mucwiz.cpp  
	$(MOZ_SRC)\ns\cmd\winfe\mucproc.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\navbar.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\navcntr.cpp  
	$(MOZ_SRC)\ns\cmd\winfe\navcontv.cpp  
	$(MOZ_SRC)\ns\cmd\winfe\navfram.cpp  
	$(MOZ_SRC)\ns\cmd\winfe\navigate.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\ncapiurl.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\nethelp.cpp  
	$(MOZ_SRC)\ns\cmd\winfe\mozilla.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\nsapp.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\netsdoc.cpp
	$(MOZ_SRC)\ns\cmd\winfe\nsfont.cpp
	$(MOZ_SRC)\ns\cmd\winfe\netsprnt.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\netsvw.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\nsshell.cpp
	$(MOZ_SRC)\ns\cmd\winfe\odctrl.cpp
	$(MOZ_SRC)\ns\cmd\winfe\olectc.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\olehelp.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\oleprot1.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\oleregis.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\olestart.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\oleshut.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\oleview.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\oleview1.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\outliner.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\ownedlst.cpp  
	$(MOZ_SRC)\ns\cmd\winfe\pain.cpp
	$(MOZ_SRC)\ns\cmd\winfe\plginvw.cpp
	$(MOZ_SRC)\ns\cmd\winfe\popup.cpp  
	$(MOZ_SRC)\ns\cmd\winfe\prefinfo.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\presentm.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\printpag.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\profile.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\qahook.cpp  
	$(MOZ_SRC)\ns\cmd\winfe\quickfil.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\rdfliner.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\region.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\regproto.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\shcut.cpp  
	$(MOZ_SRC)\ns\cmd\winfe\shcutdlg.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\slavewnd.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\splash.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\spiwrap.c 
	$(MOZ_SRC)\ns\cmd\winfe\srvritem.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\statbar.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\stshfont.cpp 
!ifdef MOZ_LOC_INDEP
	$(MOZ_SRC)\ns\cmd\winfe\stshli.cpp 
!endif
	$(MOZ_SRC)\ns\cmd\winfe\stshplug.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\styles.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\sysinfo.cpp
	$(MOZ_SRC)\ns\cmd\winfe\template.cpp
!if "$(MOZ_USERNAME)" == "WHITEBOX"
    $(MOZ_SRC)\ns\cmd\winfe\qadelmsg.cpp
    $(MOZ_SRC)\ns\cmd\winfe\qaoutput.cpp
	$(MOZ_SRC)\ns\cmd\winfe\qatrace.cpp
	$(MOZ_SRC)\ns\cmd\winfe\qaui.cpp
    $(MOZ_SRC)\ns\cmd\winfe\testcase.cpp
    $(MOZ_SRC)\ns\cmd\winfe\testcasemanager.cpp
    $(MOZ_SRC)\ns\cmd\winfe\tclist.cpp
    $(MOZ_SRC)\ns\cmd\winfe\testcasedlg.cpp
!endif 
	$(MOZ_SRC)\ns\cmd\winfe\timer.cpp  
	$(MOZ_SRC)\ns\cmd\winfe\tip.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\tlbutton.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\toolbar2.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\tooltip.cpp  
	$(MOZ_SRC)\ns\cmd\winfe\urlbar.cpp 
	$(MOZ_SRC)\ns\cmd\winfe\urlecho.cpp
	$(MOZ_SRC)\ns\cmd\winfe\usertlbr.cpp
	$(MOZ_SRC)\ns\cmd\winfe\viewerse.cpp    
	$(MOZ_SRC)\ns\cmd\winfe\winclose.cpp   
	$(MOZ_SRC)\ns\cmd\winfe\winpref.c
!ifdef MOZ_LOC_INDEP
	$(MOZ_SRC)\ns\cmd\winfe\winli.cpp
!endif
	$(MOZ_SRC)\ns\cmd\winfe\resdll\resdll.c
!if "$(MOZ_BITS)"=="32"
	$(MOZ_SRC)\ns\cmd\winfe\talk.cpp
!endif
	$(MOZ_SRC)\ns\cmd\winfe\nsguids.cpp
!if "$(MOZ_BITS)" == "16"
	$(MOZ_SRC)\ns\cmd\winfe\except.cpp
!endif
	$(MOZ_SRC)\ns\cmd\winfe\xpstrsw.cpp
	$(MOZ_SRC)\ns\cmd\winfe\widgetry.cpp
	$(MOZ_SRC)\ns\cmd\winfe\woohoo.cpp
<<

$(MOZ_SRC)\ns\cmd\winfe\mkfiles32\makedep.exe: $(MOZ_SRC)\ns\cmd\winfe\mkfiles32\makedep.cpp
!if "$(MOZ_BITS)"=="32"
    @cl -MT -Fo"$(OUTDIR)/" -Fe"$(MOZ_SRC)\ns\cmd\winfe\mkfiles32\makedep.exe" $(MOZ_SRC)\ns\cmd\winfe\mkfiles32\makedep.cpp
!else
    @echo Can't build makedep under 16 bits, must be built.
	!error
!endif

!ELSE

ALL : $(OUTDIR)\mozilla.dep "$(OUTDIR)" prebuild $(OUTDIR)\resdll.dll $(OUTDIR)\appicon.res $(OUTDIR)\mozilla.exe $(OUTDIR)\mozilla.tlb install rebase \
!if !defined(MOZ_NO_PATCHER)
# Allow building without patcher. You get intl security, but faster build time
"$(OUTDIR)\netsc_us.exe" "$(OUTDIR)\netsc_fr.exe"  
!else
!endif

# Allow makefile to work without dependencies generated.
!if exist("$(OUTDIR)\mozilla.dep")
!include "$(OUTDIR)\mozilla.dep"
!endif

PURIFY : "$(OUTDIR)" "$(OUTDIR)\PurifyCache" "$(OUTDIR)\mozilla.exe" pure

!ENDIF

# 
# utility Stuff.
#
$(OUTDIR) :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

$(GENDIR) :
    if not exist "$(GENDIR)/$(NULL)" mkdir "$(GENDIR)"


$(OUTDIR)\PurifyCache :
	if not exist "$(OUTDIR)\PurifyCache\$(NULL)" mkdir "$(OUTDIR)\PurifyCache"

#
#	RDF Images need binary conversion
#
NavCenterImages: $(GENDIR) $(GENDIR)\personal.rc $(GENDIR)\history.rc \
	$(GENDIR)\channels.rc $(GENDIR)\sitemap.rc $(GENDIR)\search.rc \
	$(GENDIR)\guide.rc $(GENDIR)\file.rc $(GENDIR)\ldap.rc

$(GENDIR)\personal.rc: $(MOZ_SRC)\ns\modules\rdf\images\personal.gif
	$(BIN2RC) $(MOZ_SRC)\ns\modules\rdf\images\personal.gif image/gif > $(GENDIR)\personal.rc

$(GENDIR)\history.rc: $(MOZ_SRC)\ns\modules\rdf\images\history.gif
	$(BIN2RC) $(MOZ_SRC)\ns\modules\rdf\images\history.gif image/gif > $(GENDIR)\history.rc

$(GENDIR)\channels.rc: $(MOZ_SRC)\ns\modules\rdf\images\channels.gif
	$(BIN2RC) $(MOZ_SRC)\ns\modules\rdf\images\channels.gif image/gif > $(GENDIR)\channels.rc

$(GENDIR)\sitemap.rc: $(MOZ_SRC)\ns\modules\rdf\images\sitemap.gif
	$(BIN2RC) $(MOZ_SRC)\ns\modules\rdf\images\sitemap.gif image/gif > $(GENDIR)\sitemap.rc

$(GENDIR)\search.rc: $(MOZ_SRC)\ns\modules\rdf\images\search.gif
	$(BIN2RC) $(MOZ_SRC)\ns\modules\rdf\images\search.gif image/gif > $(GENDIR)\search.rc

$(GENDIR)\guide.rc: $(MOZ_SRC)\ns\modules\rdf\images\guide.gif
	$(BIN2RC) $(MOZ_SRC)\ns\modules\rdf\images\guide.gif image/gif > $(GENDIR)\guide.rc

$(GENDIR)\file.rc: $(MOZ_SRC)\ns\modules\rdf\images\file.gif
	$(BIN2RC) $(MOZ_SRC)\ns\modules\rdf\images\file.gif image/gif > $(GENDIR)\file.rc

$(GENDIR)\ldap.rc: $(MOZ_SRC)\ns\modules\rdf\images\ldap.gif
	$(BIN2RC) $(MOZ_SRC)\ns\modules\rdf\images\ldap.gif image/gif > $(GENDIR)\ldap.rc

#
#	Misc images in the about pages.
#
!ifdef FEATURE_ABOUT_BRANDED_IMAGES

AboutImages: $(GENDIR) \
!ifdef MOZ_JAVA
	$(GENDIR)\javalogo.rc \
!endif
!ifdef FORTEZZA
	$(GENDIR)\litronic.rc \
!endif
	$(GENDIR)\biglogo.rc \
	$(GENDIR)\rsalogo.rc \
	$(GENDIR)\qt_logo.rc \
	$(GENDIR)\visilogo.rc \
	$(GENDIR)\coslogo.rc \
	$(GENDIR)\insologo.rc \
	$(GENDIR)\mmlogo.rc \
	$(GENDIR)\mclogo.rc \
	$(GENDIR)\ncclogo.rc \
	$(GENDIR)\odilogo.rc \
	$(GENDIR)\symlogo.rc \
	$(GENDIR)\tdlogo.rc

!ifdef MOZ_JAVA
$(GENDIR)\javalogo.rc: $(MOZ_SRC)\ns\lib\xp\javalogo.gif
	$(BIN2RC) $(MOZ_SRC)\ns\lib\xp\javalogo.gif image/gif > $(GENDIR)\javalogo.rc
!endif

!ifdef FORTEZZA
$(GENDIR)\litronic.rc: $(MOZ_SRC)\ns\lib\xp\litronic.gif
	$(BIN2RC) $(MOZ_SRC)\ns\lib\xp\litronic.gif image/gif > $(GENDIR)\litronic.rc
!endif

$(GENDIR)\biglogo.rc: $(MOZ_SRC)\ns\lib\xp\biglogo.gif
	$(BIN2RC) $(MOZ_SRC)\ns\lib\xp\biglogo.gif image/gif > $(GENDIR)\biglogo.rc

$(GENDIR)\rsalogo.rc: $(MOZ_SRC)\ns\lib\xp\rsalogo.gif
	$(BIN2RC) $(MOZ_SRC)\ns\lib\xp\rsalogo.gif image/gif > $(GENDIR)\rsalogo.rc

$(GENDIR)\qt_logo.rc: $(MOZ_SRC)\ns\lib\xp\qt_logo.gif
	$(BIN2RC) $(MOZ_SRC)\ns\lib\xp\qt_logo.gif image/gif > $(GENDIR)\qt_logo.rc

$(GENDIR)\visilogo.rc: $(MOZ_SRC)\ns\lib\xp\visilogo.gif
	$(BIN2RC) $(MOZ_SRC)\ns\lib\xp\visilogo.gif image/gif > $(GENDIR)\visilogo.rc

$(GENDIR)\coslogo.rc: $(MOZ_SRC)\ns\lib\xp\coslogo.jpg
	$(BIN2RC) $(MOZ_SRC)\ns\lib\xp\coslogo.jpg image/jpeg > $(GENDIR)\coslogo.rc

$(GENDIR)\insologo.rc: $(MOZ_SRC)\ns\lib\xp\insologo.gif
	$(BIN2RC) $(MOZ_SRC)\ns\lib\xp\insologo.gif image/gif > $(GENDIR)\insologo.rc

$(GENDIR)\mclogo.rc: $(MOZ_SRC)\ns\lib\xp\mclogo.gif
	$(BIN2RC) $(MOZ_SRC)\ns\lib\xp\mclogo.gif image/gif > $(GENDIR)\mclogo.rc

$(GENDIR)\ncclogo.rc: $(MOZ_SRC)\ns\lib\xp\ncclogo.gif
	$(BIN2RC) $(MOZ_SRC)\ns\lib\xp\ncclogo.gif image/gif > $(GENDIR)\ncclogo.rc

$(GENDIR)\odilogo.rc: $(MOZ_SRC)\ns\lib\xp\odilogo.gif
	$(BIN2RC) $(MOZ_SRC)\ns\lib\xp\odilogo.gif image/gif > $(GENDIR)\odilogo.rc

$(GENDIR)\symlogo.rc: $(MOZ_SRC)\ns\lib\xp\symlogo.gif
	$(BIN2RC) $(MOZ_SRC)\ns\lib\xp\symlogo.gif image/gif > $(GENDIR)\symlogo.rc

$(GENDIR)\tdlogo.rc: $(MOZ_SRC)\ns\lib\xp\tdlogo.gif
	$(BIN2RC) $(MOZ_SRC)\ns\lib\xp\tdlogo.gif image/gif > $(GENDIR)\tdlogo.rc
!else

AboutImages: $(GENDIR) \
	$(GENDIR)\flamer.rc
	
$(GENDIR)\flamer.rc: $(MOZ_SRC)\ns\lib\xp\flamer.gif
	$(BIN2RC) $(MOZ_SRC)\ns\lib\xp\flamer.gif image/gif > $(GENDIR)\flamer.rc
!endif


#
# Creation of resource files needed for building
#
#
prebuild: $(GENDIR) $(GENDIR)\initpref.rc $(GENDIR)\allpref.rc \
	$(GENDIR)\allpref2.rc $(GENDIR)\allpref3.rc $(GENDIR)\allpref4.rc\
	$(GENDIR)\winpref.rc $(GENDIR)\config.rc NavCenterImages \
	AboutImages
	
$(GENDIR)\initpref.rc: $(MOZ_SRC)\ns\modules\libpref\src\initpref.js
	$(TXT2RC) init_prefs $(MOZ_SRC)\ns\modules\libpref\src\initpref.js \
		$(GENDIR)\initpref.rc

$(GENDIR)\allpref.rc: $(MOZ_SRC)\ns\modules\libpref\src\init\all.js
	$(TXT2RC) all_prefs $(MOZ_SRC)\ns\modules\libpref\src\init\all.js \
		$(GENDIR)\allpref.rc

$(GENDIR)\allpref2.rc: $(MOZ_SRC)\ns\modules\libpref\src\init\mailnews.js
	$(TXT2RC) mailnews_prefs $(MOZ_SRC)\ns\modules\libpref\src\init\mailnews.js \
		$(GENDIR)\allpref2.rc

$(GENDIR)\allpref3.rc: $(MOZ_SRC)\ns\modules\libpref\src\init\editor.js
	$(TXT2RC) editor_prefs $(MOZ_SRC)\ns\modules\libpref\src\init\editor.js \
		$(GENDIR)\allpref3.rc

$(GENDIR)\allpref4.rc: $(MOZ_SRC)\ns\modules\libpref\src\init\security.js
	$(TXT2RC) security_prefs $(MOZ_SRC)\ns\modules\libpref\src\init\security.js \
		$(GENDIR)\allpref4.rc

$(GENDIR)\winpref.rc: $(MOZ_SRC)\ns\modules\libpref\src\win\winpref.js
	$(TXT2RC) win_prefs $(MOZ_SRC)\ns\modules\libpref\src\win\winpref.js \
	    $(GENDIR)\winpref.rc

# May need a new one for MOZ_MEDIUM.
!ifndef MOZ_COMMUNICATOR_CONFIG_JS
$(GENDIR)\config.rc:  $(MOZ_SRC)\ns\modules\libpref\src\init\configr.js
	$(TXT2RC) config_prefs $(MOZ_SRC)\ns\modules\libpref\src\init\configr.js \
		$(GENDIR)\config.rc
!else
$(GENDIR)\config.rc:  $(MOZ_SRC)\ns\modules\libpref\src\init\config.js
	$(TXT2RC) config_prefs $(MOZ_SRC)\ns\modules\libpref\src\init\config.js \
		$(GENDIR)\config.rc
!endif

#
# Installation of the executable directory, support dlls and java
#
install:    \
!IF EXIST($(DIST)\bin\jpeg$(MOZ_BITS)$(VERSION_NUMBER).dll)
	    $(OUTDIR)\jpeg$(MOZ_BITS)$(VERSION_NUMBER).dll    \
!ENDIF
!IF EXIST($(DIST)\bin\zip$(MOZ_BITS)$(VERSION_NUMBER).dll)
	    $(OUTDIR)\zip$(MOZ_BITS)$(VERSION_NUMBER).dll    \
!ENDIF
!IF EXIST($(DIST)\bin\prefui$(MOZ_BITS).dll)
	    $(OUTDIR)\prefui$(MOZ_BITS).dll    \
!ENDIF
!IF EXIST($(DIST)\bin\nsdlg$(MOZ_BITS).dll)
	    $(OUTDIR)\nsdlg$(MOZ_BITS).dll    \
!ENDIF
!ifdef EDITOR
!IF EXIST($(DIST)\bin\editor$(MOZ_BITS).dll)
	    $(OUTDIR)\editor$(MOZ_BITS).dll    \
!ENDIF
!endif
!IF EXIST($(DIST)\bin\brpref$(MOZ_BITS).dll)
	    $(OUTDIR)\brpref$(MOZ_BITS).dll    \
!ENDIF
!ifdef EDITOR
!IF EXIST($(DIST)\bin\edpref$(MOZ_BITS).dll)
	    $(OUTDIR)\edpref$(MOZ_BITS).dll    \
!ENDIF
!IF EXIST($(DIST)\bin\mnpref$(MOZ_BITS).dll)
	    $(OUTDIR)\mnpref$(MOZ_BITS).dll    \
!ENDIF
!IF EXIST($(DIST)\bin\lipref$(MOZ_BITS).dll)
	    $(OUTDIR)\lipref$(MOZ_BITS).dll    \
!ENDIF
!IF EXIST($(DIST)\bin\mnrc$(MOZ_BITS).dll)
	    $(OUTDIR)\mnrc$(MOZ_BITS).dll    \
!ENDIF
!endif
!IF EXIST($(DIST)\bin\xpstrdll.dll)
	    $(OUTDIR)\xpstrdll.dll    \
!ENDIF
!IF "$(MOZ_BITS)"=="32"
!ifndef NSPR20
!IF EXIST($(DIST)\bin\pr32$(VERSION_NUMBER).dll)
	    $(OUTDIR)\pr32$(VERSION_NUMBER).dll    \
!ENDIF
!else
!IF EXIST($(DIST)\bin\libnspr21.dll)
	    $(OUTDIR)\libnspr21.dll    \
!ENDIF
!IF EXIST($(DIST)\bin\libplds21.dll)
	    $(OUTDIR)\libplds21.dll    \
!ENDIF
!IF EXIST($(DIST)\bin\libplc21.dll)
	    $(OUTDIR)\libplc21.dll    \
!ENDIF
!IF EXIST($(DIST)\bin\libmsgc21.dll)
	    $(OUTDIR)\libmsgc21.dll    \
!ENDIF
!endif
!IF EXIST($(DIST)\bin\js32$(VERSION_NUMBER).dll)
	    $(OUTDIR)\js32$(VERSION_NUMBER).dll    \
!ENDIF
!IF EXIST($(DIST)\bin\jsd32$(VERSION_NUMBER).dll)
	    $(OUTDIR)\jsd32$(VERSION_NUMBER).dll    \
!ENDIF
!IF EXIST($(DIST)\bin\jsj32$(VERSION_NUMBER).dll)
	    $(OUTDIR)\jsj32$(VERSION_NUMBER).dll    \
!ENDIF
!IF EXIST($(DIST)\bin\xppref32.dll)
	    $(OUTDIR)\xppref32.dll    \
!ENDIF
!IF EXIST($(DIST)\bin\sched32.dll)
	    $(OUTDIR)\sched32.dll    \
!ENDIF
!IF EXIST($(DIST)\bin\jrt32$(VERSION_NUMBER).dll)
	    $(OUTDIR)\jrt32$(VERSION_NUMBER).dll    \
!ENDIF
!IF EXIST($(DIST)\bin\uni3200.dll)
	    $(OUTDIR)\uni3200.dll    \
!ENDIF
!IF EXIST($(DIST)\bin\awt32$(VERSION_NUMBER).dll)
 	    $(OUTDIR)\java\bin\awt32$(VERSION_NUMBER).dll   \
!ENDIF
!if defined(MOZ_TRACKGDI)
!IF EXIST($(DIST)\bin\trackgdi.dll)
	    $(OUTDIR)\trackgdi.dll   \
!ENDIF
!endif
!IF EXIST($(DIST)\bin\jbn32$(VERSION_NUMBER).dll)
	    $(OUTDIR)\java\bin\jbn32$(VERSION_NUMBER).dll    \
!ENDIF
!IF EXIST($(DIST)\bin\jdb32$(VERSION_NUMBER).dll)
	    $(OUTDIR)\java\bin\jdb32$(VERSION_NUMBER).dll    \
!ENDIF
!IF EXIST($(DIST)\bin\mm32$(VERSION_NUMBER).dll)
	    $(OUTDIR)\java\bin\mm32$(VERSION_NUMBER).dll    \
!ENDIF
!IF EXIST($(DIST)\bin\jit32$(VERSION_NUMBER).dll)
	    $(OUTDIR)\java\bin\jit32$(VERSION_NUMBER).dll    \
!ENDIF
!IF EXIST($(DIST)\bin\jpw32$(VERSION_NUMBER).dll)
	    $(OUTDIR)\java\bin\jpw32$(VERSION_NUMBER).dll    \
!ENDIF
!IF EXIST($(DIST)\bin\con32$(VERSION_NUMBER).dll)
	    $(OUTDIR)\java\bin\con32$(VERSION_NUMBER).dll    \
!ENDIF
!IF EXIST($(DIST)\bin\zpw32$(VERSION_NUMBER).dll)
	    $(OUTDIR)\java\bin\zpw32$(VERSION_NUMBER).dll    \
!ENDIF
!IF EXIST($(MOZ_SRC)\ns\cmd\winfe\nstdfp32.dll)
	    $(OUTDIR)\dynfonts\nstdfp32.dll    \
!ENDIF
!ifdef MOZ_LDAP
!IF EXIST($(DIST)\bin\nsldap32.dll)
	    $(OUTDIR)\nsldap32.dll    \
!ENDIF
!endif
!IF EXIST($(DIST)\bin\unicvt32.dll)
	   $(OUTDIR)\unicvt32.dll    \
!ENDIF
!ifdef EDITOR
!IF EXIST($(DIST)\bin\editor32.dll)
	    $(OUTDIR)\editor32.dll   \
!ENDIF
!endif
!ELSE
!IFDEF MOZ_JAVA
!IF EXIST($(DIST)\bin\jrt16$(VERSION_NUMBER).dll)
	    $(OUTDIR)\jrt16$(VERSION_NUMBER).dll    \
!ENDIF
!IF EXIST($(DIST)\bin\awt16$(VERSION_NUMBER).dll)
	    $(OUTDIR)\java\bin\awt16$(VERSION_NUMBER).dll   \
!ENDIF
!IF EXIST($(DIST)\bin\jpw16$(VERSION_NUMBER).dll)
	    $(OUTDIR)\java\bin\jpw16$(VERSION_NUMBER).dll    \
!ENDIF
!IF EXIST($(DIST)\bin\con16$(VERSION_NUMBER).dll)
	    $(OUTDIR)\java\bin\con16$(VERSION_NUMBER).dll    \
!ENDIF
!IF EXIST($(DIST)\bin\zpw16$(VERSION_NUMBER).dll)
	    $(OUTDIR)\java\bin\zpw16$(VERSION_NUMBER).dll    \
!ENDIF
#!IF EXIST($(DIST)\bin\jbn16$(VERSION_NUMBER).dll)
#           $(OUTDIR)\java\bin\jbn16$(VERSION_NUMBER).dll   \
#!ENDIF
#!IF EXIST($(DIST)\bin\jdb16$(VERSION_NUMBER).dll)
#           $(OUTDIR)\java\bin\jdb16$(VERSION_NUMBER).dll   \
#!ENDIF
!ENDIF
!ifndef NSPR20
!IF EXIST($(DIST)\bin\pr16$(VERSION_NUMBER).dll)
	    $(OUTDIR)\pr16$(VERSION_NUMBER).dll    \
!ENDIF
!else
!IF EXIST($(DIST)\lib\nspr21.dll)
	    $(OUTDIR)\nspr21.dll    \
!ENDIF
!IF EXIST($(DIST)\lib\plds21.dll)
	    $(OUTDIR)\plds21.dll    \
!ENDIF
!IF EXIST($(DIST)\lib\plc21.dll)
	    $(OUTDIR)\plc21.dll    \
!ENDIF
!IF EXIST($(DIST)\lib\msgc21.dll)
	    $(OUTDIR)\msgc21.dll    \
!ENDIF
!endif
!IF EXIST($(DIST)\bin\js16$(VERSION_NUMBER).dll)
	    $(OUTDIR)\js16$(VERSION_NUMBER).dll    \
!ENDIF
!IF EXIST($(DIST)\bin\jsj16$(VERSION_NUMBER).dll)
	    $(OUTDIR)\jsj16$(VERSION_NUMBER).dll    \
!ENDIF
!IF EXIST($(DIST)\bin\xppref16.dll)
	    $(OUTDIR)\xppref16.dll    \
!ENDIF
!IF EXIST($(DIST)\bin\sched16.dll)
	    $(OUTDIR)\sched16.dll    \
!ENDIF
!IF EXIST($(DIST)\bin\nsinit.exe)
	    $(OUTDIR)\nsinit.exe    \
!ENDIF
!IF EXIST($(DIST)\bin\uni1600.dll)
	    $(OUTDIR)\uni1600.dll    \
!ENDIF
!IF EXIST($(MOZ_SRC)\ns\cmd\winfe\nstdfp16.dll)
	    $(OUTDIR)\dynfonts\nstdfp16.dll    \
!ENDIF
!ifdef MOZ_LDAP
!IF EXIST($(DIST)\bin\nsldap.dll)
	    $(OUTDIR)\nsldap.dll    \
!ENDIF
!endif
!ENDIF
!if defined(MOZ_JAVA)
	    $(OUTDIR)\java\classes\$(JAR_NAME) \
!endif
	    $(OUTDIR)\netscape.cfg  \
!if defined(DEATH_TO_POLICY_FILES)
	    $(OUTDIR)\$(POLICY) \
!endif
!ifdef EDITOR
!IF EXIST($(DIST)\bin\$(SPELLCHK_DLL))
	   $(OUTDIR)\spellchk\$(SPELLCHK_DLL)    \
!ENDIF
!endif
!ifdef EDITOR
!IF EXIST($(SPELLCHK_DATA)\pen4s324.dat)
	   $(OUTDIR)\spellchk\pen4s324.dat    \
!ENDIF
!endif
!ifdef EDITOR
!IF EXIST($(SPELLCHK_DATA)\netscape.dic)
	   $(OUTDIR)\spellchk\netscape.dic    \
!ENDIF
!endif
!ifdef EDITOR
!IF EXIST($(SPELLCHK_DATA)\psp4s333.dat)
	   $(OUTDIR)\spellchk\psp4s333.dat    \
!ENDIF
!endif
!ifdef EDITOR
!IF EXIST($(SPELLCHK_DATA)\pgr2s321.dat)
	   $(OUTDIR)\spellchk\pgr2s321.dat    \
!ENDIF
!endif
!ifdef EDITOR
!IF EXIST($(SPELLCHK_DATA)\pfr2s331.dat)
	   $(OUTDIR)\spellchk\pfr2s331.dat    \
!ENDIF
!endif
!ifdef EDITOR
!IF EXIST($(SPELLCHK_DATA)\pit2s340.dat)
	   $(OUTDIR)\spellchk\pit2s340.dat    \
!ENDIF
!endif
!ifdef EDITOR
!IF EXIST($(SPELLCHK_DATA)\ppo4s331.dat)
	   $(OUTDIR)\spellchk\ppo4s331.dat    \
!ENDIF
!endif
!ifdef EDITOR
!IF EXIST($(SPELLCHK_DATA)\pca4s323.dat)
	   $(OUTDIR)\spellchk\pca4s323.dat    \
!ENDIF
!endif
!ifdef EDITOR
!IF EXIST($(SPELLCHK_DATA)\pdu2s341.dat)
	   $(OUTDIR)\spellchk\pdu2s341.dat    \
!ENDIF
!endif
!ifdef EDITOR
!IF EXIST($(SPELLCHK_DATA)\pfn2s311.dat)
	   $(OUTDIR)\spellchk\pfn2s311.dat    \
!ENDIF
!endif

REBASE=rebase.exe
!if [for %i in (. %PATH%) do @if exist %i\$(REBASE) echo %i\$(REBASE) > rebase.yes]
!endif
!if exist(rebase.yes)
!if [for %i in ($(OUTDIR)\*.dll) do @echo %i >> rebase.lst]
!endif
!if [for %i in ($(OUTDIR)\java\bin\*.dll) do @echo %i >> rebase.lst]
!endif
!if [for %i in ($(OUTDIR)\spellchk\*.dll) do @echo %i >> rebase.lst]
!endif
!endif

rebase:
!if exist(rebase.lst)
	$(REBASE) -b 60000000 -R . -G rebase.lst
	del rebase.lst
!endif
!if exist(rebase.yes)
	del rebase.yes
!endif


$(OUTDIR)\java\bin\jpw32$(VERSION_NUMBER).dll:   $(DIST)\bin\jpw32$(VERSION_NUMBER).dll
    @IF NOT EXIST "$(OUTDIR)\java/$(NULL)" mkdir "$(OUTDIR)\java"
    @IF NOT EXIST "$(OUTDIR)\java\bin/$(NULL)" mkdir "$(OUTDIR)\java\bin"
    @IF EXIST $(DIST)\bin\jpw32$(VERSION_NUMBER).dll copy $(DIST)\bin\jpw32$(VERSION_NUMBER).dll $(OUTDIR)\java\bin\jpw32$(VERSION_NUMBER).dll

$(OUTDIR)\java\bin\con32$(VERSION_NUMBER).dll:   $(DIST)\bin\con32$(VERSION_NUMBER).dll
    @IF NOT EXIST "$(OUTDIR)\java/$(NULL)" mkdir "$(OUTDIR)\java"
    @IF NOT EXIST "$(OUTDIR)\java\bin/$(NULL)" mkdir "$(OUTDIR)\java\bin"
    @IF EXIST $(DIST)\bin\con32$(VERSION_NUMBER).dll copy $(DIST)\bin\con32$(VERSION_NUMBER).dll $(OUTDIR)\java\bin\con32$(VERSION_NUMBER).dll

$(OUTDIR)\java\bin\zpw32$(VERSION_NUMBER).dll:   $(DIST)\bin\zpw32$(VERSION_NUMBER).dll
    @IF NOT EXIST "$(OUTDIR)\java/$(NULL)" mkdir "$(OUTDIR)\java"
    @IF NOT EXIST "$(OUTDIR)\java\bin/$(NULL)" mkdir "$(OUTDIR)\java\bin"
    @IF EXIST $(DIST)\bin\zpw32$(VERSION_NUMBER).dll copy $(DIST)\bin\zpw32$(VERSION_NUMBER).dll $(OUTDIR)\java\bin\zpw32$(VERSION_NUMBER).dll


!IF "$(MOZ_BITS)"=="32"
!ifndef NSPR20
$(OUTDIR)\pr32$(VERSION_NUMBER).dll:   $(DIST)\bin\pr32$(VERSION_NUMBER).dll
    @IF EXIST $(DIST)\bin\pr32$(VERSION_NUMBER).dll copy $(DIST)\bin\pr32$(VERSION_NUMBER).dll $(OUTDIR)\pr32$(VERSION_NUMBER).dll
!else
$(OUTDIR)\libnspr21.dll:   $(DIST)\bin\libnspr21.dll
    @IF EXIST $(DIST)\bin\libnspr21.dll copy $(DIST)\bin\libnspr21.dll $(OUTDIR)\libnspr21.dll
$(OUTDIR)\libplds21.dll:   $(DIST)\bin\libplds21.dll
    @IF EXIST $(DIST)\bin\libplds21.dll copy $(DIST)\bin\libplds21.dll $(OUTDIR)\libplds21.dll
$(OUTDIR)\libplc21.dll:   $(DIST)\bin\libplc21.dll
    @IF EXIST $(DIST)\bin\libplc21.dll copy $(DIST)\bin\libplc21.dll $(OUTDIR)\libplc21.dll
$(OUTDIR)\libmsgc21.dll:   $(DIST)\bin\libmsgc21.dll
    @IF EXIST $(DIST)\bin\libmsgc21.dll copy $(DIST)\bin\libmsgc21.dll $(OUTDIR)\libmsgc21.dll
!endif

$(OUTDIR)\js32$(VERSION_NUMBER).dll:   $(DIST)\bin\js32$(VERSION_NUMBER).dll
    @IF EXIST $(DIST)\bin\js32$(VERSION_NUMBER).dll copy $(DIST)\bin\js32$(VERSION_NUMBER).dll $(OUTDIR)\js32$(VERSION_NUMBER).dll

$(OUTDIR)\jsd32$(VERSION_NUMBER).dll:   $(DIST)\bin\jsd32$(VERSION_NUMBER).dll
    @IF EXIST $(DIST)\bin\jsd32$(VERSION_NUMBER).dll copy $(DIST)\bin\jsd32$(VERSION_NUMBER).dll $(OUTDIR)\jsd32$(VERSION_NUMBER).dll

$(OUTDIR)\jsj32$(VERSION_NUMBER).dll:   $(DIST)\bin\jsj32$(VERSION_NUMBER).dll
    @IF EXIST $(DIST)\bin\jsj32$(VERSION_NUMBER).dll copy $(DIST)\bin\jsj32$(VERSION_NUMBER).dll $(OUTDIR)\jsj32$(VERSION_NUMBER).dll

$(OUTDIR)\xppref32.dll:   $(DIST)\bin\xppref32.dll
    @IF EXIST $(DIST)\bin\xppref32.dll copy $(DIST)\bin\xppref32.dll $(OUTDIR)\xppref32.dll

$(OUTDIR)\sched32.dll:   $(DIST)\bin\sched32.dll
    @IF EXIST $(DIST)\bin\sched32.dll copy $(DIST)\bin\sched32.dll $(OUTDIR)\sched32.dll

$(OUTDIR)\uni3200.dll:   $(DIST)\bin\uni3200.dll
    @IF EXIST $(DIST)\bin\uni3200.dll copy $(DIST)\bin\uni3200.dll $(OUTDIR)\uni3200.dll

$(OUTDIR)\jrt32$(VERSION_NUMBER).dll:   $(DIST)\bin\jrt32$(VERSION_NUMBER).dll
    @IF EXIST $(DIST)\bin\jrt32$(VERSION_NUMBER).dll copy $(DIST)\bin\jrt32$(VERSION_NUMBER).dll $(OUTDIR)\jrt32$(VERSION_NUMBER).dll

$(OUTDIR)\unicvt32.dll:   $(DIST)\bin\unicvt32.dll
    @IF EXIST $(DIST)\bin\unicvt32.dll copy $(DIST)\bin\unicvt32.dll $(OUTDIR)\unicvt32.dll

$(OUTDIR)\dynfonts\nstdfp32.dll:   $(MOZ_SRC)\ns\cmd\winfe\nstdfp32.dll
    @IF NOT EXIST "$(OUTDIR)\dynfonts/$(NULL)" mkdir "$(OUTDIR)\dynfonts"
    @IF EXIST $(MOZ_SRC)\ns\cmd\winfe\nstdfp32.dll copy $(MOZ_SRC)\ns\cmd\winfe\nstdfp32.dll $(OUTDIR)\dynfonts\nstdfp32.dll

!if !defined(MOZ_NO_LDAP)
$(OUTDIR)\nsldap32.dll:   $(DIST)\bin\nsldap32.dll
    @IF EXIST $(DIST)\bin\nsldap32.dll copy $(DIST)\bin\nsldap32.dll $(OUTDIR)\nsldap32.dll
!endif

$(OUTDIR)\java\bin\awt32$(VERSION_NUMBER).dll:   $(DIST)\bin\awt32$(VERSION_NUMBER).dll
    @IF NOT EXIST "$(OUTDIR)\java/$(NULL)" mkdir "$(OUTDIR)\java"
    @IF NOT EXIST "$(OUTDIR)\java\bin/$(NULL)" mkdir "$(OUTDIR)\java\bin"
    @IF EXIST $(DIST)\bin\awt32$(VERSION_NUMBER).dll copy $(DIST)\bin\awt32$(VERSION_NUMBER).dll $(OUTDIR)\java\bin\awt32$(VERSION_NUMBER).dll

!if defined(MOZ_TRACKGDI)
$(OUTDIR)\trackgdi.dll:   $(DIST)\bin\trackgdi.dll
    @IF EXIST $(DIST)\bin\trackgdi.dll copy $(DIST)\bin\trackgdi.dll $(OUTDIR)\trackgdi.dll
!endif

$(OUTDIR)\java\bin\jbn32$(VERSION_NUMBER).dll:   $(DIST)\bin\jbn32$(VERSION_NUMBER).dll
    @IF NOT EXIST "$(OUTDIR)\java/$(NULL)" mkdir "$(OUTDIR)\java"
    @IF NOT EXIST "$(OUTDIR)\java\bin/$(NULL)" mkdir "$(OUTDIR)\java\bin"
    @IF EXIST $(DIST)\bin\jbn32$(VERSION_NUMBER).dll copy $(DIST)\bin\jbn32$(VERSION_NUMBER).dll $(OUTDIR)\java\bin\jbn32$(VERSION_NUMBER).dll

$(OUTDIR)\java\bin\jdb32$(VERSION_NUMBER).dll:   $(DIST)\bin\jdb32$(VERSION_NUMBER).dll
    @IF NOT EXIST "$(OUTDIR)\java/$(NULL)" mkdir "$(OUTDIR)\java"
    @IF NOT EXIST "$(OUTDIR)\java\bin/$(NULL)" mkdir "$(OUTDIR)\java\bin"
    @IF EXIST $(DIST)\bin\jdb32$(VERSION_NUMBER).dll copy $(DIST)\bin\jdb32$(VERSION_NUMBER).dll $(OUTDIR)\java\bin\jdb32$(VERSION_NUMBER).dll

$(OUTDIR)\java\bin\mm32$(VERSION_NUMBER).dll:   $(DIST)\bin\mm32$(VERSION_NUMBER).dll
    @IF NOT EXIST "$(OUTDIR)\java/$(NULL)" mkdir "$(OUTDIR)\java"
    @IF NOT EXIST "$(OUTDIR)\java\bin/$(NULL)" mkdir "$(OUTDIR)\java\bin"
    @IF EXIST $(DIST)\bin\mm32$(VERSION_NUMBER).dll copy $(DIST)\bin\mm32$(VERSION_NUMBER).dll $(OUTDIR)\java\bin\mm32$(VERSION_NUMBER).dll

$(OUTDIR)\java\bin\jit32$(VERSION_NUMBER).dll:   $(DIST)\bin\jit32$(VERSION_NUMBER).dll
    @IF NOT EXIST "$(OUTDIR)\java/$(NULL)" mkdir "$(OUTDIR)\java"
    @IF NOT EXIST "$(OUTDIR)\java\bin/$(NULL)" mkdir "$(OUTDIR)\java\bin"
    @IF EXIST $(DIST)\bin\jit32$(VERSION_NUMBER).dll copy $(DIST)\bin\jit32$(VERSION_NUMBER).dll $(OUTDIR)\java\bin\jit32$(VERSION_NUMBER).dll

!ELSE
!ifndef NSPR20
$(OUTDIR)\pr16$(VERSION_NUMBER).dll:   $(DIST)\bin\pr16$(VERSION_NUMBER).dll
    @IF EXIST $(DIST)\bin\pr16$(VERSION_NUMBER).dll copy $(DIST)\bin\pr16$(VERSION_NUMBER).dll $(OUTDIR)\pr16$(VERSION_NUMBER).dll
!else
$(OUTDIR)\nspr21.dll:   $(DIST)\lib\nspr21.dll
    @IF EXIST $(DIST)\bin\nspr21.dll copy $(DIST)\bin\nspr21.dll $(OUTDIR)\nspr21.dll
$(OUTDIR)\plds21.dll:   $(DIST)\lib\plds21.dll
    @IF EXIST $(DIST)\bin\plds21.dll copy $(DIST)\bin\plds21.dll $(OUTDIR)\plds21.dll
$(OUTDIR)\plc21.dll:   $(DIST)\lib\plc21.dll
    @IF EXIST $(DIST)\bin\plc21.dll copy $(DIST)\bin\plc21.dll $(OUTDIR)\plc21.dll
$(OUTDIR)\msgc21.dll:   $(DIST)\lib\msgc21.dll
    @IF EXIST $(DIST)\bin\msgc21.dll copy $(DIST)\bin\msgc21.dll $(OUTDIR)\msgc21.dll
!endif

$(OUTDIR)\js16$(VERSION_NUMBER).dll:   $(DIST)\bin\js16$(VERSION_NUMBER).dll
    @IF EXIST $(DIST)\bin\js16$(VERSION_NUMBER).dll copy $(DIST)\bin\js16$(VERSION_NUMBER).dll $(OUTDIR)\js16$(VERSION_NUMBER).dll

$(OUTDIR)\jsj16$(VERSION_NUMBER).dll:   $(DIST)\bin\jsj16$(VERSION_NUMBER).dll
    @IF EXIST $(DIST)\bin\jsj16$(VERSION_NUMBER).dll copy $(DIST)\bin\jsj16$(VERSION_NUMBER).dll $(OUTDIR)\jsj16$(VERSION_NUMBER).dll

$(OUTDIR)\xppref16.dll:   $(DIST)\bin\xppref16.dll
    @IF EXIST $(DIST)\bin\xppref16.dll copy $(DIST)\bin\xppref16.dll $(OUTDIR)\xppref16.dll

$(OUTDIR)\sched16.dll:   $(DIST)\bin\sched16.dll
    @IF EXIST $(DIST)\bin\sched16.dll copy $(DIST)\bin\sched16.dll $(OUTDIR)\sched16.dll

$(OUTDIR)\uni1600.dll:   $(DIST)\bin\uni1600.dll
    @IF EXIST $(DIST)\bin\uni1600.dll copy $(DIST)\bin\uni1600.dll $(OUTDIR)\uni1600.dll

$(OUTDIR)\dynfonts\nstdfp16.dll:   $(MOZ_SRC)\ns\cmd\winfe\nstdfp16.dll
    @IF NOT EXIST "$(OUTDIR)\dynfonts/$(NULL)" mkdir "$(OUTDIR)\dynfonts"
    @IF EXIST $(MOZ_SRC)\ns\cmd\winfe\nstdfp16.dll copy $(MOZ_SRC)\ns\cmd\winfe\nstdfp16.dll $(OUTDIR)\dynfonts\nstdfp16.dll

!if !defined(MOZ_NO_LDAP)
$(OUTDIR)\nsldap.dll:   $(DIST)\bin\nsldap.dll
    @IF EXIST $(DIST)\bin\nsldap.dll copy $(DIST)\bin\nsldap.dll $(OUTDIR)\nsldap.dll
!endif

$(OUTDIR)\jrt16$(VERSION_NUMBER).dll:   $(DIST)\bin\jrt16$(VERSION_NUMBER).dll
    @IF EXIST $(DIST)\bin\jrt16$(VERSION_NUMBER).dll copy $(DIST)\bin\jrt16$(VERSION_NUMBER).dll $(OUTDIR)\jrt16$(VERSION_NUMBER).dll

$(OUTDIR)\java\bin\awt16$(VERSION_NUMBER).dll:   $(DIST)\bin\awt16$(VERSION_NUMBER).dll
    @IF NOT EXIST "$(OUTDIR)\java/$(NULL)" mkdir "$(OUTDIR)\java"
    @IF NOT EXIST "$(OUTDIR)\java\bin/$(NULL)" mkdir "$(OUTDIR)\java\bin"
    @IF EXIST $(DIST)\bin\awt16$(VERSION_NUMBER).dll copy $(DIST)\bin\awt16$(VERSION_NUMBER).dll $(OUTDIR)\java\bin\awt16$(VERSION_NUMBER).dll

$(OUTDIR)\java\bin\jpw16$(VERSION_NUMBER).dll:   $(DIST)\bin\jpw16$(VERSION_NUMBER).dll
    @IF NOT EXIST "$(OUTDIR)\java/$(NULL)" mkdir "$(OUTDIR)\java"
    @IF NOT EXIST "$(OUTDIR)\java\bin/$(NULL)" mkdir "$(OUTDIR)\java\bin"
    @IF EXIST $(DIST)\bin\jpw16$(VERSION_NUMBER).dll copy $(DIST)\bin\jpw16$(VERSION_NUMBER).dll $(OUTDIR)\java\bin\jpw16$(VERSION_NUMBER).dll

$(OUTDIR)\java\bin\con16$(VERSION_NUMBER).dll:   $(DIST)\bin\con16$(VERSION_NUMBER).dll
    @IF NOT EXIST "$(OUTDIR)\java/$(NULL)" mkdir "$(OUTDIR)\java"
    @IF NOT EXIST "$(OUTDIR)\java\bin/$(NULL)" mkdir "$(OUTDIR)\java\bin"
    @IF EXIST $(DIST)\bin\con16$(VERSION_NUMBER).dll copy $(DIST)\bin\con16$(VERSION_NUMBER).dll $(OUTDIR)\java\bin\con16$(VERSION_NUMBER).dll

$(OUTDIR)\java\bin\zpw16$(VERSION_NUMBER).dll:   $(DIST)\bin\zpw16$(VERSION_NUMBER).dll
    @IF NOT EXIST "$(OUTDIR)\java/$(NULL)" mkdir "$(OUTDIR)\java"
    @IF NOT EXIST "$(OUTDIR)\java\bin/$(NULL)" mkdir "$(OUTDIR)\java\bin"
    @IF EXIST $(DIST)\bin\zpw16$(VERSION_NUMBER).dll copy $(DIST)\bin\zpw16$(VERSION_NUMBER).dll $(OUTDIR)\java\bin\zpw16$(VERSION_NUMBER).dll

$(OUTDIR)\nsinit.exe: $(DIST)\bin\nsinit.exe
    @IF EXIST $(DIST)\bin\nsinit.exe copy $(DIST)\bin\nsinit.exe $(OUTDIR)\nsinit.exe

!ENDIF


# XXX this will copy them all, we really only want the ones that changed
$(OUTDIR)\java\classes\$(JAR_NAME): $(JAVA_DESTPATH)\$(JAR_NAME)
    @IF NOT EXIST "$(OUTDIR)\java/$(NULL)" mkdir "$(OUTDIR)\java"
    @IF NOT EXIST "$(OUTDIR)\java\classes/$(NULL)" mkdir "$(OUTDIR)\java\classes"
!ifdef MOZ_JAVA
!ifdef MOZ_COPY_ALL_JARS
    @copy $(JAVA_DESTPATH)\*.jar "$(OUTDIR)\java\classes\"
!else
    @copy $(JAVA_DESTPATH)\java*.jar "$(OUTDIR)\java\classes\"
    @copy $(JAVA_DESTPATH)\ifc*.jar "$(OUTDIR)\java\classes\"
    @copy $(JAVA_DESTPATH)\jsj*.jar "$(OUTDIR)\java\classes\"
!endif
!endif

$(OUTDIR)\netscape.cfg:   $(DIST)\bin\netscape.cfg
    @IF EXIST $(DIST)\bin\netscape.cfg copy $(DIST)\bin\netscape.cfg $(OUTDIR)\netscape.cfg

$(OUTDIR)\editor$(MOZ_BITS).dll:   $(DIST)\bin\editor$(MOZ_BITS).dll
    @IF EXIST $(DIST)\bin\$(@F) copy $(DIST)\bin\$(@F) $@

$(OUTDIR)\nsdlg$(MOZ_BITS).dll:   $(DIST)\bin\nsdlg$(MOZ_BITS).dll
    @IF EXIST $(DIST)\bin\$(@F) copy $(DIST)\bin\$(@F) $@

$(OUTDIR)\zip$(MOZ_BITS)$(VERSION_NUMBER).dll:   $(DIST)\bin\zip$(MOZ_BITS)$(VERSION_NUMBER).dll
    @IF EXIST $(DIST)\bin\$(@F) copy $(DIST)\bin\$(@F) $@

$(OUTDIR)\jpeg$(MOZ_BITS)$(VERSION_NUMBER).dll:   $(DIST)\bin\jpeg$(MOZ_BITS)$(VERSION_NUMBER).dll
    @IF EXIST $(DIST)\bin\$(@F) copy $(DIST)\bin\$(@F) $@

$(OUTDIR)\prefui$(MOZ_BITS).dll:   $(DIST)\bin\prefui$(MOZ_BITS).dll
    @IF EXIST $(DIST)\bin\$(@F) copy $(DIST)\bin\$(@F) $@

$(OUTDIR)\brpref$(MOZ_BITS).dll:   $(DIST)\bin\brpref$(MOZ_BITS).dll
    @IF EXIST $(DIST)\bin\$(@F) copy $(DIST)\bin\$(@F) $@

$(OUTDIR)\lipref$(MOZ_BITS).dll:   $(DIST)\bin\lipref$(MOZ_BITS).dll
    @IF EXIST $(DIST)\bin\$(@F) copy $(DIST)\bin\$(@F) $@

$(OUTDIR)\edpref$(MOZ_BITS).dll:   $(DIST)\bin\edpref$(MOZ_BITS).dll
    @IF EXIST $(DIST)\bin\$(@F) copy $(DIST)\bin\$(@F) $@

$(OUTDIR)\mnpref$(MOZ_BITS).dll:   $(DIST)\bin\mnpref$(MOZ_BITS).dll
    @IF EXIST $(DIST)\bin\$(@F) copy $(DIST)\bin\$(@F) $@

$(OUTDIR)\mnrc$(MOZ_BITS).dll:   $(DIST)\bin\mnrc$(MOZ_BITS).dll
    @IF EXIST $(DIST)\bin\$(@F) copy $(DIST)\bin\$(@F) $@

$(OUTDIR)\xpstrdll.dll:   $(DIST)\bin\xpstrdll.dll
    @IF EXIST $(DIST)\bin\$(@F) copy $(DIST)\bin\$(@F) $@

$(OUTDIR)\spellchk\$(SPELLCHK_DLL):   $(DIST)\bin\$(SPELLCHK_DLL) 
    @IF NOT EXIST "$(OUTDIR)\spellchk/$(NULL)" mkdir "$(OUTDIR)\spellchk"
    @IF EXIST $(DIST)\bin\$(SPELLCHK_DLL) copy $(DIST)\bin\$(SPELLCHK_DLL) $(OUTDIR)\spellchk\$(SPELLCHK_DLL)

# spell checker English dictionary 
$(OUTDIR)\spellchk\pen4s324.dat:   $(SPELLCHK_DATA)\pen4s324.dat 
    @IF NOT EXIST "$(OUTDIR)\spellchk/$(NULL)" mkdir "$(OUTDIR)\spellchk"
    @IF EXIST $(SPELLCHK_DATA)\pen4s324.dat copy $(SPELLCHK_DATA)\pen4s324.dat $(OUTDIR)\spellchk\pen4s324.dat

#spell checker built-in dictionary extension
$(OUTDIR)\spellchk\netscape.dic:   $(SPELLCHK_DATA)\netscape.dic 
    @IF NOT EXIST "$(OUTDIR)\spellchk/$(NULL)" mkdir "$(OUTDIR)\spellchk"
    @IF EXIST $(SPELLCHK_DATA)\netscape.dic copy $(SPELLCHK_DATA)\netscape.dic $(OUTDIR)\spellchk\netscape.dic

# spell checker Spanish dictionary 
$(OUTDIR)\spellchk\psp4s333.dat:   $(SPELLCHK_DATA)\psp4s333.dat 
    @IF NOT EXIST "$(OUTDIR)\spellchk/$(NULL)" mkdir "$(OUTDIR)\spellchk"
    @IF EXIST $(SPELLCHK_DATA)\psp4s333.dat copy $(SPELLCHK_DATA)\psp4s333.dat $(OUTDIR)\spellchk\psp4s333.dat

# spell checker German dictionary 
$(OUTDIR)\spellchk\pgr2s321.dat:   $(SPELLCHK_DATA)\pgr2s321.dat 
    @IF NOT EXIST "$(OUTDIR)\spellchk/$(NULL)" mkdir "$(OUTDIR)\spellchk"
    @IF EXIST $(SPELLCHK_DATA)\pgr2s321.dat copy $(SPELLCHK_DATA)\pgr2s321.dat $(OUTDIR)\spellchk\pgr2s321.dat

# spell checker French dictionary 
$(OUTDIR)\spellchk\pfr2s331.dat:   $(SPELLCHK_DATA)\pfr2s331.dat 
    @IF NOT EXIST "$(OUTDIR)\spellchk/$(NULL)" mkdir "$(OUTDIR)\spellchk"
    @IF EXIST $(SPELLCHK_DATA)\pfr2s331.dat copy $(SPELLCHK_DATA)\pfr2s331.dat $(OUTDIR)\spellchk\pfr2s331.dat

# spell checker Italian dictionary 
$(OUTDIR)\spellchk\pit2s340.dat:   $(SPELLCHK_DATA)\pit2s340.dat 
    @IF NOT EXIST "$(OUTDIR)\spellchk/$(NULL)" mkdir "$(OUTDIR)\spellchk"
    @IF EXIST $(SPELLCHK_DATA)\pit2s340.dat copy $(SPELLCHK_DATA)\pit2s340.dat $(OUTDIR)\spellchk\pit2s340.dat

# spell checker Brazilian dictionary 
$(OUTDIR)\spellchk\ppo4s331.dat:   $(SPELLCHK_DATA)\ppo4s331.dat 
    @IF NOT EXIST "$(OUTDIR)\spellchk/$(NULL)" mkdir "$(OUTDIR)\spellchk"
    @IF EXIST $(SPELLCHK_DATA)\ppo4s331.dat copy $(SPELLCHK_DATA)\ppo4s331.dat $(OUTDIR)\spellchk\ppo4s331.dat

# spell checker Catalan dictionary 
$(OUTDIR)\spellchk\pca4s323.dat:   $(SPELLCHK_DATA)\pca4s323.dat 
    @IF NOT EXIST "$(OUTDIR)\spellchk/$(NULL)" mkdir "$(OUTDIR)\spellchk"
    @IF EXIST $(SPELLCHK_DATA)\pca4s323.dat copy $(SPELLCHK_DATA)\pca4s323.dat $(OUTDIR)\spellchk\pca4s323.dat

# spell checker Dutch dictionary 
$(OUTDIR)\spellchk\pdu2s341.dat:   $(SPELLCHK_DATA)\pdu2s341.dat 
    @IF NOT EXIST "$(OUTDIR)\spellchk/$(NULL)" mkdir "$(OUTDIR)\spellchk"
    @IF EXIST $(SPELLCHK_DATA)\pdu2s341.dat copy $(SPELLCHK_DATA)\pdu2s341.dat $(OUTDIR)\spellchk\pdu2s341.dat

# spell checker Finnish dictionary 
$(OUTDIR)\spellchk\pfn2s311.dat:   $(SPELLCHK_DATA)\pfn2s311.dat 
    @IF NOT EXIST "$(OUTDIR)\spellchk/$(NULL)" mkdir "$(OUTDIR)\spellchk"
    @IF EXIST $(SPELLCHK_DATA)\pfn2s311.dat copy $(SPELLCHK_DATA)\pfn2s311.dat $(OUTDIR)\spellchk\pfn2s311.dat


BATCH_BUILD_1:          \
	BATCH_LIBI18N_C         \
	BATCH_LIBLAYER_C                \
	BATCH_LIBMSG_CPP                \
	BATCH_PLUGIN_CPP                \
	BATCH_LIBNET_CPP                \
	BATCH_LIBDBM_C          \
	BATCH_LAYOUT_CPP                \
	BATCH_WINFE_CPP         \
	BATCH_LIBJAR_C          \
	BATCH_XLATE_C           
    echo Done > $(TMP)\bb1.sem
    echo BATCH BUILD 1 Successful and complete


BATCH_BUILD_2:             \
	BATCH_LIBADDR_C         \
	BATCH_LIBMIME_C         \
	BATCH_LIBNET_C          \
	BATCH_LIBPARSE_C                \
	BATCH_LIBMSG_C          \
	BATCH_LIBMISC_C         \
	BATCH_LIBSTYLE_C                \
	BATCH_XP_C              \
	BATCH_PICS_C              \
	BATCH_WINFE_C           \
	BATCH_PLUGIN_C          \
	BATCH_LIBADDR_CPP               \
	BATCH_LAYOUT_C          \
	BATCH_JPEG_C            \
	BATCH_LIBNEO_CPP                \
	BATCH_LIBMOCHA_C                
    echo Done >$(TMP)\bb2.sem
    echo BATCH BUILD 2 Successful and complete

SPAWN_BATCH1:
!if 0
    -mkdir $(OUTDIR)\bb1
    -mkdir $(OUTDIR)\bb1\$(PROD)$(VERSTR)
    -del $(TMP)\bb1.sem
    copy $(OUTDIR)\mozilla.dep $(OUTDIR)\bb1\$(PROD)$(VERSTR)
    set MOZ_OUT=$(OUTDIR)\bb1
    start /b /HIGH nmake -f mozilla.mak BATCH_BUILD_1 MOZ_OUT=$(OUTDIR)\bb1
!endif
    -del $(TMP)\bb1.sem
    start /b /HIGH nmake -f mozilla.mak BATCH_BUILD_1 MOZ_PROCESS_NUMBER=1


WAIT_OBJECTS:
    echo Waiting for Batch Build 1 to complete
    $(MOZ_SRC)\ns\cmd\winfe\mkfiles32\waitfor $(TMP)\bb1.sem
!if 0
    move $(OUTDIR)\bb1\$(PROD)$(VERSTR)\*.obj $(OUTDIR)
!endif


!ifdef MOZ_BATCH
BUILD_SOURCE: SPAWN_BATCH1 BATCH_BUILD_2 WAIT_OBJECTS
!else
BUILD_SOURCE: $(OBJ_FILES)
!endif


"$(OUTDIR)\mozilla.exe" : "$(OUTDIR)" BUILD_SOURCE $(OUTDIR)\mozilla.res $(LINK_LIBS)
	$(LINK) @<<"$(OUTDIR)\link.cl"
!if "$(MOZ_BITS)"=="32"
    $(LINK_FLAGS) $(LINK_OBJS)
!else
    $(LINK_FLAGS)
    $(LINK_OBJS)
    $(OUTDIR)\mozilla.exe
    $(OUTDIR)\mozilla.map
    c:\msvc\lib\+
    c:\msvc\mfc\lib\+
!if !defined(MOZ_USE_MS_MALLOC)
    $(DIST)\lib\mem16.lib +
!endif
!if defined(MOZ_DEBUG)
    lafxcwd.lib +
!else
    lafxcw.lib +
!endif
    oldnames.lib +
    libw.lib +
    llibcew.lib +
    compobj.lib +
    storage.lib +
    ole2.lib +
    ole2disp.lib +
    ole2nls.lib +
    mfcoleui.lib +
    commdlg.lib +
    ddeml.lib +
    olecli.lib +
    olesvr.lib +
	mmsystem.lib +
    shell.lib +
    ver.lib +
!ifdef MOZ_LDAP
    $(DIST)\lib\nsldap.lib +
!endif
!if defined(MOZ_JAVA)
    $(DIST)\lib\jrt16$(VERSION_NUMBER).lib +
    $(DIST)\lib\libapp~1.lib +
    $(DIST)\lib\jsj16$(VERSION_NUMBER).lib +
    $(DIST)\lib\libnsc16.lib +
    $(DIST)\lib\nsn16.lib +
    $(DIST)\lib\li16.lib +
	$(DIST)\lib\prgrss16.lib +
!ifdef EDITOR
!ifdef MOZ_JAVA
    $(DIST)\lib\edtplug.lib +
!endif
!endif
    $(DIST)\lib\softup16.lib +
!else
    $(DIST)\lib\libreg16.lib +
    $(DIST)\lib\libsjs16.lib +
    $(DIST)\lib\libnjs16.lib +
!endif
!ifdef MOZ_JAVA
!ifndef NO_SECURITY
    $(DIST)\lib\jsl16.lib +
!endif
!endif
!if defined(NSPR20)
    $(DIST)\lib\nspr21.lib +
    $(DIST)\lib\plds21.lib +
    $(DIST)\lib\plc21.lib +
    $(DIST)\lib\msgc21.lib +
!else
    $(DIST)\lib\pr16$(VERSION_NUMBER).lib +
!endif
    $(DIST)\lib\js16$(VERSION_NUMBER).lib +
    $(DIST)\lib\jsj16$(VERSION_NUMBER).lib +
    $(DIST)\lib\xppref16.lib +
    $(DIST)\lib\secnav16.lib +
    $(DIST)\lib\export.lib +
    $(DIST)\lib\ssl.lib +
    $(DIST)\lib\pkcs12.lib +
    $(DIST)\lib\pkcs7.lib +
    $(DIST)\lib\secmod.lib +
    $(DIST)\lib\cert.lib +
    $(DIST)\lib\key.lib +
    $(DIST)\lib\crypto.lib +
    $(DIST)\lib\secutil.lib +
    $(DIST)\lib\hash.lib +
    $(DIST)\lib\font.lib +
    $(DIST)\lib\winfont.lib +
    $(DIST)\lib\prefuuid.lib +
    $(DIST)\lib\htmldg16.lib +
    $(DIST)\lib\hook.lib +
    $(DIST)\lib\png.lib +
	$(DIST)\lib\sched16.lib +
    $(DIST)\lib\rdf16.lib +
    $(DIST)\lib\xpstrdll.lib +
!ifdef MOZ_MAIL_NEWS
	$(DIST)\lib\mnrc16.lib +
!endif
    $(DIST)\lib\zip$(MOZ_BITS)$(VERSION_NUMBER).lib +
    $(DIST)\lib\jpeg$(MOZ_BITS)$(VERSION_NUMBER).lib +
    $(DIST)\lib\dbm$(MOZ_BITS).lib +
    $(BINREL_DIST)\lib\watcomfx.lib
    $(MOZ_SRC)\ns\cmd\winfe\mozilla.def;
!endif

<<KEEP
!if "$(MOZ_BITS)"=="16"
    $(RSC) /K $(OUTDIR)\appicon.res $(OUTDIR)\mozilla.exe
!endif

PATCHER = $(DIST)\bin\patcher.exe
# cheat: use 32-bit patcher in win16 builds
#                               (you can't do this hack with gmake!)
!if "$(MOZ_BITS)"=="16"
PATCHER	= $(PATCHER:16=32)
!endif

$(OUTDIR)\netsc_us.exe : "$(OUTDIR)" $(PATCHER) $(XPDIST)\xpdist\domestic.txt $(OUTDIR)\mozilla.exe
	$(PATCHER) @<<
    $(XPDIST)\xpdist\domestic.txt
    $(OUTDIR)\mozilla.exe 
    $(OUTDIR)\netsc_us.exe
<<

$(OUTDIR)\netsc_fr.exe : "$(OUTDIR)" $(PATCHER) $(XPDIST)\xpdist\france.txt $(OUTDIR)\mozilla.exe
	$(PATCHER) @<<
    $(XPDIST)\xpdist\france.txt
    $(OUTDIR)\mozilla.exe 
    $(OUTDIR)\netsc_fr.exe
<<

RES_FILES =\
	$(MOZ_SRC)\ns\cmd\winfe\mozilla.rc\
	$(MOZ_SRC)\ns\cmd\winfe\editor.rc\
	$(MOZ_SRC)\ns\cmd\winfe\edres2.h\
	$(GENDIR)\allpref.rc\
	$(GENDIR)\allpref2.rc\
	$(GENDIR)\allpref3.rc\
	$(GENDIR)\allpref4.rc\
	$(GENDIR)\initpref.rc\
	$(GENDIR)\winpref.rc\
	$(GENDIR)\config.rc\
	$(MOZ_SRC)\ns\cmd\winfe\res\convtbls.rc\
	$(MOZ_SRC)\ns\cmd\winfe\res\editor.rc2\
	$(MOZ_SRC)\ns\cmd\winfe\res\license.rc\
	$(MOZ_SRC)\ns\cmd\winfe\res\mail.rc\
	$(MOZ_SRC)\ns\cmd\winfe\res\mozilla.rc2\
	$(MOZ_SRC)\ns\cmd\winfe\res\mozilla.rc3\
	\
	$(MOZ_SRC)\ns\cmd\winfe\res\address.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\addrnew.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\arrow1.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\arrow2.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\BITMAP3.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\bkfdopen.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\bkmkfld2.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\bm_qf.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\BMKITEM.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\BMP00001.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\BMP00002.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\bookmark.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\collect.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\column.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\compbar.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\comptabs.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\DOWND.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\DOWNF.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\DOWNU.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\DOWNX.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\EDAL_A_U.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\EDAL_B_U.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\EDAL_C_U.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\EDAL_L_U.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\EDAL_R_U.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\EDAL_T_U.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\EDALCB_U.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\edalignc.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\edalignl.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\edalignr.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\edanch.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\edanchm.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\edcombo.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\edform.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\edformm.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\edhrule.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\edimage.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\edlink.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\edtable.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\edtag.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\edtage.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\edtagem.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\edtagm.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\edtarg.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\EDTARGET.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\FILE.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\GAUDIO.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\GBINARY.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\GFIND.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\GFOLDER.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\GGENERIC.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\GIMAGE.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\GMOVIE.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\GOPHER_F.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\GTELNET.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\GTEXT.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\IBAD.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\IMAGE_BA.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\IMAGE_MA.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\IREPLACE.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\IUNKNOWN.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\mailcol.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\mailingl.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\mailthrd.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\msgback.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\N.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\newsart.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\newsthrd.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\outliner.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\outlmail.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\person.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\PICTURES.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\PICTURES.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\smidelay.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\smmask.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\SREPLACE.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\SSECURE.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\submenu.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\submenu2.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\taskbarl.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\taskbars.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\tb_dock.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\UPD.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\UPF.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\UPU.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\UPX.bmp\
	$(MOZ_SRC)\ns\cmd\winfe\res\vflippy.bmp\
	\
	$(MOZ_SRC)\ns\cmd\winfe\res\ADRESSWD.ico\
	$(MOZ_SRC)\ns\cmd\winfe\res\BOOKMKWD.ico\
	$(MOZ_SRC)\ns\cmd\winfe\res\COMPOSWD.ico\
	$(MOZ_SRC)\ns\cmd\winfe\res\COMPOSWD.ico\
	$(MOZ_SRC)\ns\cmd\winfe\res\idelay.ico\
	$(MOZ_SRC)\ns\cmd\winfe\res\IDR_DOC.ico\
	$(MOZ_SRC)\ns\cmd\winfe\res\IDR_DOC.ico\
	$(MOZ_SRC)\ns\cmd\winfe\res\mail.ico\
	$(MOZ_SRC)\ns\cmd\winfe\res\MAILWD.ico\
	$(MOZ_SRC)\ns\cmd\winfe\res\MAILWD.ico\
	$(MOZ_SRC)\ns\cmd\winfe\res\MAILWD.ico\
	$(MOZ_SRC)\ns\cmd\winfe\res\NEWSWD.ico\
	$(MOZ_SRC)\ns\cmd\winfe\res\NEWSWD.ico\
	$(MOZ_SRC)\ns\cmd\winfe\res\NEWSWD.ico\
	$(MOZ_SRC)\ns\cmd\winfe\res\SILVER.ico\
	$(MOZ_SRC)\ns\cmd\winfe\res\SRCHWD.ico\
	\
	$(MOZ_SRC)\ns\cmd\winfe\res\actembed.cur\
	$(MOZ_SRC)\ns\cmd\winfe\res\edhtmlcp.cur\
	$(MOZ_SRC)\ns\cmd\winfe\res\edhtmlmv.cur\
	$(MOZ_SRC)\ns\cmd\winfe\res\edimgcp.cur\
	$(MOZ_SRC)\ns\cmd\winfe\res\edimgmv.cur\
	$(MOZ_SRC)\ns\cmd\winfe\res\edlinkcp.cur\
	$(MOZ_SRC)\ns\cmd\winfe\res\edlinkmv.cur\
	$(MOZ_SRC)\ns\cmd\winfe\res\edtbeaml.cur\
	$(MOZ_SRC)\ns\cmd\winfe\res\edtbeams.cur\
	$(MOZ_SRC)\ns\cmd\winfe\res\edtextcp.cur\
	$(MOZ_SRC)\ns\cmd\winfe\res\edtextmv.cur\


$(OUTDIR)\mozilla.res : $(RES_FILES) "$(OUTDIR)"
    @SET SAVEINCLUDE=%%INCLUDE%%
    @SET INCLUDE=$(RCINCLUDES);$(RCDISTINCLUDES);%%SAVEINCLUDE%%
    $(RSC) /Fo$(PROD)$(VERSTR).res $(RCFILEFLAGS) $(RCFLAGS) $(MOZ_SRC)\ns\cmd\winfe\mozilla.rc
    @IF EXIST $(PROD)$(VERSTR).res copy $(PROD)$(VERSTR).res $(OUTDIR)\mozilla.res 
    @IF EXIST $(PROD)$(VERSTR).res del $(PROD)$(VERSTR).res
    @SET INCLUDE=%%SAVEINCLUDE%%
    @SET SAVEINCLUDE=

$(OUTDIR)\appicon.res : $(MOZ_SRC)\ns\cmd\winfe\res\silver.ico "$(OUTDIR)"
    @SET SAVEINCLUDE=%%INCLUDE%%
    @SET INCLUDE=$(MOZ_SRC)\ns\cmd\winfe\res;%%SAVEINCLUDE%%
    $(RSC) /Fo$(PROD)$(VERSTR).res $(RCFILEFLAGS) $(RCFLAGS) $(MOZ_SRC)\ns\cmd\winfe\res\appicon.rc
    @IF EXIST $(PROD)$(VERSTR).res copy $(PROD)$(VERSTR).res $(OUTDIR)\appicon.res 
    @IF EXIST $(PROD)$(VERSTR).res del $(PROD)$(VERSTR).res
    @SET INCLUDE=%%SAVEINCLUDE%%
    @SET SAVEINCLUDE=

$(OUTDIR)\resdll.dll : $(OUTDIR)\mozilla.res $(OUTDIR)\resdll.obj
!if "$(MOZ_BITS)"=="32"
	$(LINK) /SUBSYSTEM:windows /DLL /INCREMENTAL:no /PDB:$(OUTDIR)/"resdll.pdb" /MACHINE:I386 /OUT:$(OUTDIR)/"resdll.dll" /IMPLIB:$(OUTDIR)/"resdll.lib" $(OUTDIR)\resdll.obj $(OUTDIR)\mozilla.res
!ELSE
		echo >NUL @<<resdll.CRF
$(LINK_FLAGS)
$(OUTDIR)\RESDLL.OBJ
$(OUTDIR)\resdll.DLL
nul
C:\MSVC\LIB\+
C:\MSVC\MFC\LIB\+
!if !defined(MOZ_DEBUG)
lafxdwd oldnames libw commdlg shell olecli olesvr ldllcew
!else
lafxdw oldnames libw commdlg shell olecli olesvr ldllcew
!endif
..\resdll\resdll.def
$(OUTDIR)\mozilla.res;
<<
	$(MOZ_TOOLS)\bin\optlinks @resdll.CRF
	implib /nowep $(OUTDIR)\resdll.LIB $(OUTDIR)\resdll.DLL

!ENDIF

ODL= \
!IF "$(MOZ_BITS)"=="32"
    $(MOZ_SRC)\ns\cmd\winfe\mozilla.odl
!ELSE
    $(MOZ_SRC)\ns\cmd\winfe\nscape16.odl
!ENDIF

PRECOMPILED_TLB= \
!IF "$(MOZ_BITS)"=="32"
    $(MOZ_SRC)\ns\cmd\winfe\mozilla.tlb
!ELSE
    $(MOZ_SRC)\ns\cmd\winfe\nscape16.tlb
!ENDIF

CPF=
!if "$(OS)" == "Windows_NT" && "$(OSVER)" == "5.0"
CPF=$(CPF) /Y
!endif

#   Only perform this step if after any possibility of automation is gone
#       and only then when the file is specifically out of date.
!if exist($(OUTDIR)\mozilla.tlb)
$(PRECOMPILED_TLB) : $(ODL)
    $(MTL) /tlb $(OUTDIR)\mozilla.tlb $(ODL)
    -copy $(OUTDIR)\mozilla.tlb $(PRECOMPILED_TLB) $(CPF)
!endif

$(OUTDIR)\mozilla.tlb : $(PRECOMPILED_TLB)
    -copy $(PRECOMPILED_TLB) $(OUTDIR)\mozilla.tlb $(CPF)


#nuke all the output directories
clobber_all:
    -rd /s /q $(MOZ_OUT)\x86Dbg
    -rd /s /q $(MOZ_OUT)\x86Rel
    -rd /s /q $(MOZ_OUT)\16x86Dbg
    -rd /s /q $(MOZ_OUT)\16x86Rel
    -rd /s /q $(MOZ_OUT)\NavDbg
    -rd /s /q $(MOZ_OUT)\NavRel
    -rd /s /q $(MOZ_OUT)\16NavDbg
    -rd /s /q $(MOZ_OUT)\16NavRel
    -rd /s /q _gen

dist:
    @set SAVE_SRC=%%MOZ_SRC%%
    @set MOZ_SRC=$(MOZ_SRC)
    @set SAVE_MSVC4=%%MSVC4%%
    @set MSVC4=$(MSVC4)
    @$(MOZ_SRC)
    @cd \ns
    nmake /f makefile.win export
    @set MOZ_SRC=%%SAVE_SRC%%
    @set SAVE_SRC=
    @set MSVC4=%%SAVE_MSVC4%%
    @set SAVE_MSVC4=

cleandist:
    @set SAVE_SRC=%%MOZ_SRC%%
    @set MOZ_SRC=$(MOZ_SRC)
    @set SAVE_MSVC4=%%MSVC4%%
    @set MSVC4=$(MSVC4)
    @$(MOZ_SRC)
    @cd \ns
    nmake /f makefile.win clobber
    rm -fr $(DIST)
    @set MOZ_SRC=%%SAVE_SRC%%
    @set SAVE_SRC=
    @set MSVC4=%%SAVE_MSVC4%%
    @set SAVE_MSVC4=

XCF=/S /I /D
!if "$(OS)" == "Windows_NT" && "$(OSVER)" == "5.0"
XCF=$(XCF) /Y
!endif

exports:
    -xcopy $(MOZ_SRC)\ns\lib\layout\*.h $(EXPORTINC) $(XCF)
    -xcopy $(MOZ_SRC)\ns\lib\libstyle\*.h $(EXPORTINC) $(XCF)
    -xcopy $(MOZ_SRC)\ns\lib\liblayer\include\*.h $(EXPORTINC) $(XCF)
    -xcopy $(MOZ_SRC)\ns\lib\libi18n\*.h $(EXPORTINC) $(XCF)
    -xcopy $(MOZ_SRC)\ns\lib\libjar\*.h $(EXPORTINC) $(XCF)
    -xcopy $(MOZ_SRC)\ns\lib\libparse\*.h $(EXPORTINC) $(XCF)
    -xcopy $(MOZ_SRC)\ns\lib\libnet\*.h $(EXPORTINC) $(XCF)
!ifdef MOZ_MAIL_NEWS
    -xcopy $(MOZ_SRC)\ns\lib\libaddr\*.h $(EXPORTINC) $(XCF)
    -xcopy $(MOZ_SRC)\ns\lib\libmsg\*.h $(EXPORTINC) $(XCF)
    -xcopy $(MOZ_SRC)\ns\lib\libneo\*.h $(EXPORTINC) $(XCF)
!endif
!ifdef MOZ_LDAP
    -xcopy $(MOZ_SRC)\ns\netsite\ldap\include\*.h $(EXPORTINC) $(XCF)
    -xcopy $(XPDIST)\public\ldap\*.h $(EXPORTINC) $(XCF)
!endif
!ifdef EDITOR
!ifdef MOZ_JAVA
    -xcopy $(MOZ_SRC)\ns\modules\edtplug\include\*.h $(EXPORTINC) $(XCF)
!endif
!endif
    -xcopy $(MOZ_SRC)\ns\lib\plugin\*.h $(EXPORTINC) $(XCF)
!if defined(MOZ_JAVA)
    -xcopy $(MOZ_SRC)\ns\modules\applet\include\*.h $(EXPORTINC) $(XCF)
    -xcopy $(MOZ_SRC)\ns\modules\libreg\include\*.h $(EXPORTINC) $(XCF)
!endif
    -xcopy $(MOZ_SRC)\ns\modules\libutil\public\xp_obs.h $(EXPORTINC) $(XCF)
    -xcopy $(MOZ_SRC)\ns\modules\libimg\public\*.h $(EXPORTINC) $(XCF)
    -xcopy $(MOZ_SRC)\ns\modules\libpref\public\*.h $(EXPORTINC) $(XCF)
    -xcopy $(MOZ_SRC)\ns\modules\coreincl\*.h $(EXPORTINC) $(XCF)
!if defined(MOZ_JAVA)
    -xcopy $(MOZ_SRC)\ns\sun-java\jtools\include\*.h $(EXPORTINC) $(XCF)
    -xcopy $(MOZ_SRC)\ns\sun-java\include\*.h $(EXPORTINC) $(XCF)
    -xcopy $(MOZ_SRC)\ns\sun-java\md-include\*.h $(EXPORTINC) $(XCF)
!endif
    -xcopy $(DIST)\include\*.h $(EXPORTINC) $(XCF)
    -xcopy $(XPDIST)\public\dbm\*.h $(EXPORTINC) $(XCF)
    -xcopy $(XPDIST)\public\js\*.h $(EXPORTINC) $(XCF)
!if "$(MOZ_BITS)" == "32"
    -xcopy $(XPDIST)\public\jsdebug\*.h $(EXPORTINC) $(XCF)
!endif
    -xcopy $(XPDIST)\public\security\*.h $(EXPORTINC) $(XCF)
!if defined(MOZ_JAVA)
    -xcopy $(XPDIST)\public\applet\*.h $(EXPORTINC) $(XCF)
    -xcopy $(XPDIST)\public\libreg\*.h $(EXPORTINC) $(XCF)
!endif
    -xcopy $(XPDIST)\public\hook\*.h $(EXPORTINC) $(XCF)
    -xcopy $(XPDIST)\public\pref\*.h $(EXPORTINC) $(XCF)
!if defined(MOZ_JAVA)
    -xcopy $(XPDIST)\public\edtplug\*.h $(EXPORTINC) $(XCF)
!endif
    -xcopy $(XPDIST)\public\htmldlgs\*.h $(EXPORTINC) $(XCF)
    -xcopy $(XPDIST)\public\softupdt\*.h $(EXPORTINC) $(XCF)
    -xcopy $(XPDIST)\public\li\*.h $(EXPORTINC) $(XCF)
!if defined(MOZ_JAVA)
    -xcopy $(XPDIST)\public\progress\*.h $(EXPORTINC) $(XCF)
!endif
    -xcopy $(XPDIST)\public\schedulr\*.h $(EXPORTINC) $(XCF)
    -xcopy $(XPDIST)\public\libfont\*.h $(EXPORTINC) $(XCF)
    -xcopy $(MOZ_SRC)\ns\dist\public\winfont\*.h $(EXPORTINC) $(XCF)
    -xcopy $(MOZ_SRC)\ns\dist\public\spellchk\*.h $(EXPORTINC) $(XCF)
    -xcopy $(MOZ_SRC)\ns\jpeg\*.h $(EXPORTINC) $(XCF)
    -xcopy $(MOZ_SRC)\ns\lib\libcnv\*.h $(EXPORTINC) $(XCF)

pure:
	$(MOZ_PURIFY)\purify /Run=no /ErrorCallStackLength=20 /AllocCallStackLength=20 \
	/CacheDir="$(MOZ_SRC)\ns\cmd\winfe\mkfiles32\$(PROD)$(VERSTR)\PurifyCache" \
	/Out "$(MOZ_SRC)\ns\cmd\winfe\mkfiles32\$(PROD)$(VERSTR)\mozilla.exe"

# for debugging this makefile
symbols:
	@echo "DIST    = $(DIST)"
	@echo "XPDIST  = $(XPDIST)"
	@echo "OUTDIR  = $(OUTDIR)"
	@echo "OBJDIR  = $(OBJDIR)"
	@echo "MOZ_SRC = $(MOZ_SRC)"
	@echo "PATCHER = $(PATCHER)"
!if "$(MOZ_USERNAME)" == "WHITEBOX"
	@echo "MOZ_USERNAME = $(MOZ_USERNAME)"  
	@echo "MOZ_USERDEBUG = $(MOZ_USERDEBUG)"
!endif
	

ns.zip:
	cd $(OUTDIR)
	zip -9rpu ns.zip		\
!if defined(MOZ_JAVA)
		mozilla.exe		\
		java/bin/awt3240.dll	\
		java/bin/jbn3240.dll	\
		java/bin/jdb3240.dll	\
		java/bin/jpw3240.dll	\
		java/bin/mm3240.dll	\
		java/classes/ifc11.jar	\
		java/classes/iiop10.jar	\
		java/classes/jae40.jar	\
		java/classes/java40.jar	\
		java/classes/jio40.jar	\
		java/classes/jsj10.jar	\
		java/classes/jsd10.jar	\
		java/classes/ldap10.jar	\
		java/classes/scd10.jar	\
		jrt3240.dll		\
		jsd3240.dll		\
!endif
		brpref32.dll		\
		lipref32.dll		\
		unicvt32.dll		\
		uni3200.dll		\
		resdll.dll		\
		prefui32.dll		\
		pr3240.dll		\
		nsldap32.dll		\
		nsdlg32.dll		\
		mnpref32.dll		\
		mnrc32.dll		\
		xpstrdll.dll		\
		js3240.dll		\
		jpeg3240.dll		\
		edpref32.dll		\
		editor32.dll		\
		xppref32.dll		\
		sched32.dll			\
		netscape.cfg		\
		moz40p3	
