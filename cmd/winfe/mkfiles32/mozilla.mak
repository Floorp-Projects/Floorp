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

!if "$(WINOS)" == "WIN95"
QUIET=
!else
QUIET=@
!endif

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
SPELLCHK_DATA = $(DEPTH)\modules\spellchk\data

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
BIN2RC=$(DEPTH)\config\bin2rc.exe

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


NEOFLAGS=/DqNeoThreads /DqNeoStandAlone /I$(DEPTH)\lib\libneo ^
    /I$(DEPTH)\lib\libneo\ibmpc ^
    /I$(DEPTH)\lib\libneo\ibmpc\alone 


#
# If you add a file in a new directory, you must add flags for that directory
#
CFLAGS_LIBMSG_C=        $(CFLAGS_DEFAULT) /Fp"$(OUTDIR)/msgc.pch" /YX"msg.h"
CFLAGS_LIBMIME_C=       $(CFLAGS_DEFAULT)
CFLAGS_LIBI18N_C=       $(CFLAGS_DEFAULT) /Fp"$(OUTDIR)/intlpriv.pch" /YX"intlpriv.h"
CFLAGS_LIBIMG_C=        $(CFLAGS_DEFAULT) /I$(DEPTH)\jpeg /Fp"$(OUTDIR)/xp.pch" /YX"xp.h"
CFLAGS_JTOOLS_C=        $(CFLAGS_DEFAULT)  
CFLAGS_LIBCNV_C=        $(CFLAGS_DEFAULT) /I$(DEPTH)\jpeg /Fp"$(OUTDIR)/xp.pch" /YX"xp.h"
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
CFLAGS_PLUGIN_CPP=      $(CFLAGS_DEBUG) /I$(DEPTH)\cmd\winfe /Fp"$(OUTDIR)/stdafx.pch" /YX"stdafx.h"
CFLAGS_LIBPREF_C=                 $(CFLAGS_DEBUG)
CFLAGS_WINFE_C=                 $(CFLAGS_DEBUG)
!if "$(MOZ_BITS)"=="32"
!if "$(MOZ_BCPRO)" == ""
CFLAGS_WINFE_CPP=       $(CFLAGS_DEBUG) /I$(DEPTH)\jpeg /Fp"$(OUTDIR)/stdafx.pch" /YX"stdafx.h"
!else
CFLAGS_WINFE_CPP=       $(CFLAGS_DEBUG) /I$(DEPTH)\jpeg
!endif
!else
CFLAGS_WINFE_CPP=       $(CFLAGS_DEBUG)
!endif
!if "$(MOZ_BITS)"=="16"
CFLAGS_WINDOWS_C=		$(CFLAGS_DEFAULT) /I$(DEPTH)\dist\public\win16\private
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
    $(DIST)\lib\jar.lib \
    $(DIST)\lib\secmocha.lib \
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
    $(DIST)\lib\xml.lib \
    $(OUTDIR)\appicon.res \
    $(DIST)\lib\winfont.lib \
    $(DIST)\lib\abouturl.lib \
    $(DIST)\lib\dataurl.lib \
    $(DIST)\lib\fileurl.lib \
    $(DIST)\lib\ftpurl.lib \
    $(DIST)\lib\gophurl.lib \
    $(DIST)\lib\httpurl.lib \
    $(DIST)\lib\jsurl.lib \
    $(DIST)\lib\marimurl.lib \
    $(DIST)\lib\remoturl.lib \
    $(DIST)\lib\netcache.lib \
    $(DIST)\lib\netcnvts.lib \
    $(DIST)\lib\network.lib \
    $(DIST)\lib\cnetinit.lib \
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
!if "$(WINOS)" == "WIN95"
    $(DIST)\lib\xpcom$(MOZ_BITS).lib
!else
    $(DIST)\lib\xpcom$(MOZ_BITS).lib \
    $(NULL)
!endif

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

#EXPORTINC=$(DEPTH)\exportinc
EXPORTINC=$(DEPTH)\dist\public\win16

# if you add something to CINCLUDES, you must also add it to the exports target
# at the end of the file.

CINCLUDES= \
    /I$(DEPTH)\include \
!if "$(MOZ_BITS)" == "32"
    /I$(DEPTH)\lib\layout \
    /I$(DEPTH)\lib\libstyle \
    /I$(DEPTH)\lib\liblayer\include \
    /I$(DEPTH)\lib\libcnv \
    /I$(DEPTH)\lib\libi18n \
    /I$(DEPTH)\lib\libparse \
    /I$(DEPTH)\lib\plugin \
!ifdef MOZ_MAIL_NEWS
    /I$(DEPTH)\lib\libmsg \
    /I$(DEPTH)\lib\libaddr \
    /I$(DEPTH)\lib\libneo \
!endif
!else
    /I$(EXPORTINC)
!endif

RCINCLUDES=$(DEPTH)\cmd\winfe;$(DEPTH)\include

CDEPENDINCLUDES= \
    /I$(DEPTH)\cmd\winfe \
    /I$(DEPTH)\jpeg

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
    /I$(XPDIST)\public\zlib \
    /I$(XPDIST)\public\httpurl \
    /I$(XPDIST)\public\netcache \
    /I$(XPDIST)\public\network \
    /I$(XPDIST)\public\util \
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
    /I$(XPDIST)\public\xml \
    /I$(DIST)\include \
    /I$(XPDIST)\public\img \
    /I$(XPDIST)\public\jtools \
!else
!endif
    /I$(XPDIST)\public \
    /I$(XPDIST)\public\coreincl \
!ifndef NO_SECURITY
    /I$(XPDIST)\public\jar \
!endif
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

all: "$(OUTDIR)" $(DEPTH)\cmd\winfe\mkfiles32\makedep.exe $(OUTDIR)\mozilla.dep

$(OUTDIR)\mozilla.dep: $(DEPTH)\cmd\winfe\mkfiles32\mozilla.mak
    @rem <<$(PROD)$(VERSTR).dep
	$(CINCLUDES) $(CDISTINCLUDES) $(CDEPENDINCLUDES) -O $(OUTDIR)\mozilla.dep
!IF "$(MOZ_BITS)"=="16"
    -16
!ENDIF
<<
    $(DEPTH)\cmd\winfe\mkfiles32\makedep.exe @$(PROD)$(VERSTR).dep -F <<
	$(DEPTH)\lib\liblayer\src\cl_comp.c
	$(DEPTH)\lib\liblayer\src\cl_drwbl.c
	$(DEPTH)\lib\liblayer\src\cl_layer.c
	$(DEPTH)\lib\liblayer\src\cl_group.c
	$(DEPTH)\lib\liblayer\src\cl_util.c
	$(DEPTH)\lib\liblayer\src\xp_rect.c

	$(DEPTH)\lib\layout\bullet.c  
	$(DEPTH)\lib\layout\clipline.c
!ifdef EDITOR
	$(DEPTH)\lib\layout\editor.cpp   
	$(DEPTH)\lib\layout\edtbuf.cpp   
	$(DEPTH)\lib\layout\edtcmd.cpp   
	$(DEPTH)\lib\layout\edtele.cpp   
	$(DEPTH)\lib\layout\edtjava.cpp   
	$(DEPTH)\lib\layout\edtsave.cpp   
	$(DEPTH)\lib\layout\edtutil.cpp   
!endif
	$(DEPTH)\lib\layout\layedit.c 
	$(DEPTH)\lib\layout\fsfile.cpp   
	$(DEPTH)\lib\layout\streams.cpp   
	$(DEPTH)\lib\layout\layarena.c
	$(DEPTH)\lib\layout\layblock.c 
	$(DEPTH)\lib\layout\laycell.c 
	$(DEPTH)\lib\layout\laycols.c 
	$(DEPTH)\lib\layout\laydisp.c 
	$(DEPTH)\lib\layout\layembed.c
	$(DEPTH)\lib\layout\layfind.c 
	$(DEPTH)\lib\layout\layform.c 
	$(DEPTH)\lib\layout\layfree.c 
	$(DEPTH)\lib\layout\laygrid.c 
	$(DEPTH)\lib\layout\layhrule.c
	$(DEPTH)\lib\layout\layimage.c
	$(DEPTH)\lib\layout\layinfo.c 
!if defined(MOZ_JAVA)
	$(DEPTH)\lib\layout\layjava.c 
!endif
	$(DEPTH)\lib\layout\laylayer.c 
	$(DEPTH)\lib\layout\laylist.c 
	$(DEPTH)\lib\layout\laymap.c  
	$(DEPTH)\lib\layout\laymocha.c
	$(DEPTH)\lib\layout\layobj.c
	$(DEPTH)\lib\layout\layout.c  
	$(DEPTH)\lib\layout\layprobe.c  
	$(DEPTH)\lib\layout\layscrip.c
	$(DEPTH)\lib\layout\laystyle.c
	$(DEPTH)\lib\layout\laysel.c  
	$(DEPTH)\lib\layout\layspace.c  
	$(DEPTH)\lib\layout\laysub.c  
	$(DEPTH)\lib\layout\laytable.c
	$(DEPTH)\lib\layout\laytags.c 
	$(DEPTH)\lib\layout\laytext.c 
	$(DEPTH)\lib\layout\layutil.c 
	$(DEPTH)\lib\layout\ptinpoly.c
	$(DEPTH)\lib\layout\layrelay.c 
	$(DEPTH)\lib\layout\laytrav.c 

!ifdef MOZ_MAIL_NEWS
	$(DEPTH)\lib\libaddr\line64.c
	$(DEPTH)\lib\libaddr\vobject.c
	$(DEPTH)\lib\libaddr\vcc.c
	$(DEPTH)\lib\libaddr\ab.cpp  
	$(DEPTH)\lib\libaddr\abcntxt.cpp  
	$(DEPTH)\lib\libaddr\abentry.cpp  
	$(DEPTH)\lib\libaddr\abinfo.cpp  
	$(DEPTH)\lib\libaddr\ablist.cpp  
	$(DEPTH)\lib\libaddr\addbook.cpp  
	$(DEPTH)\lib\libaddr\abpane.cpp  
	$(DEPTH)\lib\libaddr\nickindx.cpp  
	$(DEPTH)\lib\libaddr\tyindex.cpp  
	$(DEPTH)\lib\libaddr\import.cpp
	$(DEPTH)\lib\libaddr\export.cpp
	$(DEPTH)\lib\libaddr\abundoac.cpp
	$(DEPTH)\lib\libaddr\abglue.cpp
	$(DEPTH)\lib\libaddr\abcinfo.cpp
	$(DEPTH)\lib\libaddr\abcpane.cpp
	$(DEPTH)\lib\libaddr\abpane2.cpp
	$(DEPTH)\lib\libaddr\abntfy.cpp
!endif

	$(DEPTH)\lib\libi18n\detectu2.c
	$(DEPTH)\lib\libi18n\metatag.c
	$(DEPTH)\lib\libi18n\autokr.c
	$(DEPTH)\lib\libi18n\autocvt.c
	$(DEPTH)\lib\libi18n\b52cns.c 
	$(DEPTH)\lib\libi18n\cns2b5.c 
	$(DEPTH)\lib\libi18n\cvchcode.c   
	$(DEPTH)\lib\libi18n\euc2jis.c
	$(DEPTH)\lib\libi18n\euc2sjis.c   
	$(DEPTH)\lib\libi18n\euckr2is.c   
	$(DEPTH)\lib\libi18n\fe_ccc.c 
	$(DEPTH)\lib\libi18n\doc_ccc.c 
	$(DEPTH)\lib\libi18n\intl_csi.c 
	$(DEPTH)\lib\libi18n\is2euckr.c   
	$(DEPTH)\lib\libi18n\intl_csi.c   
	$(DEPTH)\lib\libi18n\jis2oth.c
	$(DEPTH)\lib\libi18n\nscstr.c
	$(DEPTH)\lib\libi18n\sjis2euc.c   
	$(DEPTH)\lib\libi18n\sjis2jis.c   
	$(DEPTH)\lib\libi18n\ucs2.c   
	$(DEPTH)\lib\libi18n\ugen.c   
	$(DEPTH)\lib\libi18n\ugendata.c   
	$(DEPTH)\lib\libi18n\umap.c   
	$(DEPTH)\lib\libi18n\uscan.c   
!IF "$(MOZ_BITS)"=="16"
	$(DEPTH)\lib\libi18n\unicvt.c
!ENDIF
	$(DEPTH)\lib\libi18n\fontencd.c
	$(DEPTH)\lib\libi18n\csnamefn.c
	$(DEPTH)\lib\libi18n\csnametb.c
	$(DEPTH)\lib\libi18n\mime2fun.c
	$(DEPTH)\lib\libi18n\sbconvtb.c
	$(DEPTH)\lib\libi18n\acptlang.c
	$(DEPTH)\lib\libi18n\csstrlen.c
	$(DEPTH)\lib\libi18n\sblower.c
	$(DEPTH)\lib\libi18n\intlcomp.c
	$(DEPTH)\lib\libi18n\dblower.c
	$(DEPTH)\lib\libi18n\kinsokud.c
	$(DEPTH)\lib\libi18n\kinsokuf.c
	$(DEPTH)\lib\libi18n\net_junk.c 
	$(DEPTH)\lib\libi18n\katakana.c 
	$(DEPTH)\lib\libcnv\libcnv.c 
	$(DEPTH)\lib\libcnv\writejpg.c 
	$(DEPTH)\lib\libcnv\colorqnt.c 
	$(DEPTH)\lib\libcnv\readbmp.c 
	$(DEPTH)\lib\libcnv\libppm3.c 

!ifdef MOZ_MAIL_NEWS
	$(DEPTH)\lib\libmime\mimecont.c
	$(DEPTH)\lib\libmime\mimecryp.c
	$(DEPTH)\lib\libmime\mimeebod.c
	$(DEPTH)\lib\libmime\mimeenc.c
	$(DEPTH)\lib\libmime\mimeeobj.c
	$(DEPTH)\lib\libmime\mimehdrs.c
	$(DEPTH)\lib\libmime\mimei.c
	$(DEPTH)\lib\libmime\mimeiimg.c
	$(DEPTH)\lib\libmime\mimeleaf.c
	$(DEPTH)\lib\libmime\mimemalt.c
	$(DEPTH)\lib\libmime\mimemapl.c
	$(DEPTH)\lib\libmime\mimemdig.c
	$(DEPTH)\lib\libmime\mimemmix.c
	$(DEPTH)\lib\libmime\mimemoz.c
	$(DEPTH)\lib\libmime\mimempar.c
!ifndef NO_SECURITY
	$(DEPTH)\lib\libmime\mimempkc.c
!endif
	$(DEPTH)\lib\libmime\mimemrel.c
	$(DEPTH)\lib\libmime\mimemsg.c
	$(DEPTH)\lib\libmime\mimemsig.c
	$(DEPTH)\lib\libmime\mimemult.c
	$(DEPTH)\lib\libmime\mimeobj.c
	$(DEPTH)\lib\libmime\mimepbuf.c
!ifndef NO_SECURITY
	$(DEPTH)\lib\libmime\mimepkcs.c
!endif
	$(DEPTH)\lib\libmime\mimesun.c
	$(DEPTH)\lib\libmime\mimetenr.c
	$(DEPTH)\lib\libmime\mimetext.c
	$(DEPTH)\lib\libmime\mimethtm.c
	$(DEPTH)\lib\libmime\mimetpla.c
	$(DEPTH)\lib\libmime\mimetric.c
	$(DEPTH)\lib\libmime\mimeunty.c
	$(DEPTH)\lib\libmime\mimevcrd.c
	$(DEPTH)\lib\libmime\mimedrft.c
	$(DEPTH)\lib\libmisc\mime.c   
	$(DEPTH)\lib\libmisc\dirprefs.c
!endif
 
	$(DEPTH)\lib\libmisc\glhist.c 
	$(DEPTH)\lib\libmisc\hotlist.c
	$(DEPTH)\lib\libmisc\shist.c  
	$(DEPTH)\lib\libmisc\undo.c   

	$(DEPTH)\lib\libmocha\et_mocha.c
	$(DEPTH)\lib\libmocha\et_moz.c
	$(DEPTH)\lib\libmocha\lm_applt.c
	$(DEPTH)\lib\libmocha\lm_bars.c
	$(DEPTH)\lib\libmocha\lm_cmpnt.c
	$(DEPTH)\lib\libmocha\lm_doc.c
	$(DEPTH)\lib\libmocha\lm_embed.c
	$(DEPTH)\lib\libmocha\lm_event.c
	$(DEPTH)\lib\libmocha\lm_form.c   
	$(DEPTH)\lib\libmocha\lm_hardw.c   
	$(DEPTH)\lib\libmocha\lm_hist.c   
	$(DEPTH)\lib\libmocha\lm_href.c   
	$(DEPTH)\lib\libmocha\lm_img.c
	$(DEPTH)\lib\libmocha\lm_init.c   
	$(DEPTH)\lib\libmocha\lm_input.c  
	$(DEPTH)\lib\libmocha\lm_layer.c  
	$(DEPTH)\lib\libmocha\lm_nav.c
	$(DEPTH)\lib\libmocha\lm_plgin.c  
	$(DEPTH)\lib\libmocha\lm_screen.c  
	$(DEPTH)\lib\libmocha\lm_supdt.c  
	$(DEPTH)\lib\libmocha\lm_taint.c  
	$(DEPTH)\lib\libmocha\lm_trggr.c  
	$(DEPTH)\lib\libmocha\lm_url.c
	$(DEPTH)\lib\libmocha\lm_win.c
!if "$(MOZ_BITS)" == "32"
!ifdef MOZ_JAVA
	$(DEPTH)\lib\libmocha\lm_jsd.c
!endif
!endif

!ifdef MOZ_MAIL_NEWS
	$(DEPTH)\lib\libmsg\ad_strm.c 
	$(DEPTH)\lib\libmsg\msgppane.cpp 
	$(DEPTH)\lib\libmsg\addr.c
	$(DEPTH)\lib\libmsg\ap_decod.c
	$(DEPTH)\lib\libmsg\ap_encod.c
	$(DEPTH)\lib\libmsg\appledbl.c
	$(DEPTH)\lib\libmsg\bh_strm.c 
	$(DEPTH)\lib\libmsg\bytearr.cpp
	$(DEPTH)\lib\libmsg\chngntfy.cpp
!ifndef NO_SECURITY
	$(DEPTH)\lib\libmsg\composec.c
!endif
	$(DEPTH)\lib\libmsg\dwordarr.cpp
	$(DEPTH)\lib\libmsg\eneoidar.cpp
	$(DEPTH)\lib\libmsg\filters.cpp
	$(DEPTH)\lib\libmsg\grec.cpp
	$(DEPTH)\lib\libmsg\grpinfo.cpp
	$(DEPTH)\lib\libmsg\hashtbl.cpp
	$(DEPTH)\lib\libmsg\hosttbl.cpp
	$(DEPTH)\lib\libmsg\idarray.cpp
	$(DEPTH)\lib\libmsg\imaphost.cpp
	$(DEPTH)\lib\libmsg\jsmsg.cpp
	$(DEPTH)\lib\libmsg\listngst.cpp
	$(DEPTH)\lib\libmsg\m_binhex.c
	$(DEPTH)\lib\libmsg\maildb.cpp
	$(DEPTH)\lib\libmsg\mailhdr.cpp
	$(DEPTH)\lib\libmsg\mhtmlstm.cpp
	$(DEPTH)\lib\libmsg\msgbg.cpp
	$(DEPTH)\lib\libmsg\msgbgcln.cpp
	$(DEPTH)\lib\libmsg\msgbiff.c
	$(DEPTH)\lib\libmsg\msgcpane.cpp
	$(DEPTH)\lib\libmsg\msgccach.cpp
	$(DEPTH)\lib\libmsg\msgcflds.cpp
	$(DEPTH)\lib\libmsg\msgcmfld.cpp 
	$(DEPTH)\lib\libmsg\msgdb.cpp
	$(DEPTH)\lib\libmsg\msgdbini.cpp
	$(DEPTH)\lib\libmsg\msgdbvw.cpp
	$(DEPTH)\lib\libmsg\msgdoc.cpp
	$(DEPTH)\lib\libmsg\msgdwnof.cpp
	$(DEPTH)\lib\libmsg\msgdlqml.cpp
	$(DEPTH)\lib\libmsg\msgfcach.cpp
	$(DEPTH)\lib\libmsg\msgfinfo.cpp
	$(DEPTH)\lib\libmsg\imapoff.cpp
	$(DEPTH)\lib\libmsg\msgimap.cpp
	$(DEPTH)\lib\libmsg\msgfpane.cpp
	$(DEPTH)\lib\libmsg\msgglue.cpp
	$(DEPTH)\lib\libmsg\msghdr.cpp
	$(DEPTH)\lib\libmsg\msglpane.cpp
	$(DEPTH)\lib\libmsg\msglsrch.cpp
	$(DEPTH)\lib\libmsg\msgmast.cpp
	$(DEPTH)\lib\libmsg\msgmapi.cpp
	$(DEPTH)\lib\libmsg\msgmpane.cpp
	$(DEPTH)\lib\libmsg\msgmsrch.cpp
	$(DEPTH)\lib\libmsg\msgnsrch.cpp
	$(DEPTH)\lib\libmsg\msgoffnw.cpp
	$(DEPTH)\lib\libmsg\msgpane.cpp
	$(DEPTH)\lib\libmsg\msgppane.cpp
	$(DEPTH)\lib\libmsg\msgprefs.cpp
	$(DEPTH)\lib\libmsg\msgpurge.cpp
	$(DEPTH)\lib\libmsg\msgrulet.cpp
	$(DEPTH)\lib\libmsg\msgsec.cpp
	$(DEPTH)\lib\libmsg\msgsend.cpp
	$(DEPTH)\lib\libmsg\msgsendp.cpp
	$(DEPTH)\lib\libmsg\msgspane.cpp
	$(DEPTH)\lib\libmsg\msgtpane.cpp
	$(DEPTH)\lib\libmsg\msgundmg.cpp
	$(DEPTH)\lib\libmsg\msgundac.cpp
	$(DEPTH)\lib\libmsg\msgurlq.cpp
	$(DEPTH)\lib\libmsg\msgutils.c
	$(DEPTH)\lib\libmsg\msgzap.cpp
	$(DEPTH)\lib\libmsg\msgmdn.cpp
	$(DEPTH)\lib\libmsg\newsdb.cpp
	$(DEPTH)\lib\libmsg\newshdr.cpp 
	$(DEPTH)\lib\libmsg\newshost.cpp 
	$(DEPTH)\lib\libmsg\newspane.cpp 
	$(DEPTH)\lib\libmsg\newsset.cpp 
	$(DEPTH)\lib\libmsg\nwsartst.cpp 
	$(DEPTH)\lib\libmsg\prsembst.cpp 
	$(DEPTH)\lib\libmsg\ptrarray.cpp
	$(DEPTH)\lib\libmsg\search.cpp
	$(DEPTH)\lib\libmsg\subline.cpp
	$(DEPTH)\lib\libmsg\subpane.cpp
	$(DEPTH)\lib\libmsg\thrdbvw.cpp
	$(DEPTH)\lib\libmsg\thrhead.cpp
	$(DEPTH)\lib\libmsg\thrlstst.cpp
	$(DEPTH)\lib\libmsg\thrnewvw.cpp
	$(DEPTH)\lib\libmsg\mozdb.cpp

	$(DEPTH)\lib\libneo\enstring.cpp
	$(DEPTH)\lib\libneo\enswizz.cpp
	$(DEPTH)\lib\libneo\nappl.cpp
	$(DEPTH)\lib\libneo\nappsa.cpp
	$(DEPTH)\lib\libneo\narray.cpp
	$(DEPTH)\lib\libneo\nblob.cpp
	$(DEPTH)\lib\libneo\nclass.cpp
	$(DEPTH)\lib\libneo\ncstream.cpp
	$(DEPTH)\lib\libneo\ndata.cpp
	$(DEPTH)\lib\libneo\ndblndx.cpp
	$(DEPTH)\lib\libneo\ndoc.cpp
	$(DEPTH)\lib\libneo\nfltndx.cpp
	$(DEPTH)\lib\libneo\nformat.cpp
	$(DEPTH)\lib\libneo\nfree.cpp
	$(DEPTH)\lib\libneo\nfstream.cpp
	$(DEPTH)\lib\libneo\nidindex.cpp
	$(DEPTH)\lib\libneo\nidlist.cpp
	$(DEPTH)\lib\libneo\nindexit.cpp
	$(DEPTH)\lib\libneo\ninode.cpp
	$(DEPTH)\lib\libneo\nioblock.cpp
	$(DEPTH)\lib\libneo\niter.cpp
	$(DEPTH)\lib\libneo\nlaundry.cpp
	$(DEPTH)\lib\libneo\nlongndx.cpp
	$(DEPTH)\lib\libneo\nmeta.cpp
	$(DEPTH)\lib\libneo\nmrswsem.cpp
	$(DEPTH)\lib\libneo\nmsem.cpp
	$(DEPTH)\lib\libneo\nnode.cpp
	$(DEPTH)\lib\libneo\nnstrndx.cpp
	$(DEPTH)\lib\libneo\noffsprn.cpp
	$(DEPTH)\lib\libneo\npartmgr.cpp
	$(DEPTH)\lib\libneo\npersist.cpp
	$(DEPTH)\lib\libneo\npliter.cpp
	$(DEPTH)\lib\libneo\nquery.cpp
	$(DEPTH)\lib\libneo\nselect.cpp
	$(DEPTH)\lib\libneo\nsselect.cpp
	$(DEPTH)\lib\libneo\nstream.cpp
	$(DEPTH)\lib\libneo\nstrndx.cpp
	$(DEPTH)\lib\libneo\nsub.cpp
	$(DEPTH)\lib\libneo\nthread.cpp
	$(DEPTH)\lib\libneo\ntrans.cpp
	$(DEPTH)\lib\libneo\nulngndx.cpp
	$(DEPTH)\lib\libneo\nutils.cpp
	$(DEPTH)\lib\libneo\nwselect.cpp
	$(DEPTH)\lib\libneo\semnspr.cpp
	$(DEPTH)\lib\libneo\thrnspr.cpp
!endif

	$(DEPTH)\lib\libparse\pa_amp.c
	$(DEPTH)\lib\libparse\pa_hash.c   
	$(DEPTH)\lib\libparse\pa_hook.c   
	$(DEPTH)\lib\libparse\pa_mdl.c
	$(DEPTH)\lib\libparse\pa_parse.c  

	$(DEPTH)\lib\libstyle\libstyle.c
	$(DEPTH)\lib\libstyle\csslex.c
	$(DEPTH)\lib\libstyle\csstab.c
	$(DEPTH)\lib\libstyle\csstojs.c
	$(DEPTH)\lib\libstyle\jssrules.c
	$(DEPTH)\lib\libstyle\stystack.c
	$(DEPTH)\lib\libstyle\stystruc.c

	$(DEPTH)\modules\libutil\src\obs.c
!if "$(MOZ_BITS)"=="16"
	$(DEPTH)\modules\libimg\src\color.c
	$(DEPTH)\modules\libimg\src\colormap.c
	$(DEPTH)\modules\libimg\src\dither.c
	$(DEPTH)\modules\libimg\src\dummy_nc.c
	$(DEPTH)\modules\libimg\src\external.c
	$(DEPTH)\modules\libimg\src\gif.c
	$(DEPTH)\modules\libimg\src\if.c
	$(DEPTH)\modules\libimg\src\ilclient.c
	$(DEPTH)\modules\libimg\src\il_util.c
	$(DEPTH)\modules\libimg\src\jpeg.c
	$(DEPTH)\modules\libimg\src\MIMGCB.c
	$(DEPTH)\modules\libimg\src\scale.c
	$(DEPTH)\modules\libimg\src\xbm.c
	$(DEPTH)\modules\libimg\src\ipng.c
	$(DEPTH)\modules\libimg\src\png_png.c
!if defined(MOZ_JAVA)
	$(DEPTH)\sun-java\jtools\src\jmc.c
!endif
!endif

	$(DEPTH)\lib\plugin\npassoc.c 
	$(DEPTH)\lib\plugin\npglue.cpp
	$(DEPTH)\lib\plugin\npwplat.cpp 
	$(DEPTH)\lib\plugin\nsplugin.cpp 

	$(DEPTH)\lib\xlate\isotab.c   
	$(DEPTH)\lib\xlate\stubs.c
	$(DEPTH)\lib\xlate\tblprint.c 
	$(DEPTH)\lib\xlate\text.c 

	$(DEPTH)\lib\xp\allxpstr.c
	$(DEPTH)\lib\xp\xp_alloc.c
	$(DEPTH)\lib\xp\xp_cntxt.c
	$(DEPTH)\lib\xp\xp_core.c 
	$(DEPTH)\lib\xp\xp_error.c
	$(DEPTH)\lib\xp\xp_file.c 
	$(DEPTH)\lib\xp\xp_hash.c
	$(DEPTH)\lib\xp\xp_mesg.c 
	$(DEPTH)\lib\xp\xp_ncent.c
	$(DEPTH)\lib\xp\xp_reg.c  
	$(DEPTH)\lib\xp\xp_rgb.c  
	$(DEPTH)\lib\xp\xp_str.c  
	$(DEPTH)\lib\xp\xp_thrmo.c
	$(DEPTH)\lib\xp\xp_time.c 
	$(DEPTH)\lib\xp\xp_trace.c
	$(DEPTH)\lib\xp\xp_wrap.c 
	$(DEPTH)\lib\xp\xpassert.c
	$(DEPTH)\lib\xp\xp_list.c
	$(DEPTH)\lib\xp\xplocale.c

	$(DEPTH)\lib\libpwcac\pwcacapi.c

	$(DEPTH)\lib\libpics\cslabel.c
	$(DEPTH)\lib\libpics\csparse.c
	$(DEPTH)\lib\libpics\htchunk.c
	$(DEPTH)\lib\libpics\htstring.c
	$(DEPTH)\lib\libpics\htlist.c
	$(DEPTH)\lib\libpics\lablpars.c
	$(DEPTH)\lib\libpics\picsapi.c
!if "$(MOZ_BITS)" == "16"
	$(DEPTH)\nspr20\pr\src\md\windows\w16stdio.c
!endif
!ifndef MOZ_MAIL_NEWS
	$(DEPTH)\cmd\winfe\compmapi.cpp
!endif
!ifdef MOZ_MAIL_NEWS
	$(DEPTH)\cmd\winfe\addrfrm.cpp   
	$(DEPTH)\cmd\winfe\addrdlg.cpp   
	$(DEPTH)\cmd\winfe\abmldlg.cpp   
	$(DEPTH)\cmd\winfe\advopdlg.cpp
	$(DEPTH)\cmd\winfe\compbar.cpp
	$(DEPTH)\cmd\winfe\compfe.cpp
	$(DEPTH)\cmd\winfe\compfile.cpp
	$(DEPTH)\cmd\winfe\compfrm.cpp
	$(DEPTH)\cmd\winfe\compfrm2.cpp
	$(DEPTH)\cmd\winfe\compmisc.cpp
!endif
!ifdef EDITOR
	$(DEPTH)\cmd\winfe\edframe.cpp
	$(DEPTH)\cmd\winfe\edprops.cpp
	$(DEPTH)\cmd\winfe\edtable.cpp
	$(DEPTH)\cmd\winfe\edview.cpp 
	$(DEPTH)\cmd\winfe\edview2.cpp 
	$(DEPTH)\cmd\winfe\eddialog.cpp
	$(DEPTH)\cmd\winfe\edlayout.cpp   
!endif
!ifdef MOZ_MAIL_NEWS
	$(DEPTH)\cmd\winfe\filter.cpp 
	$(DEPTH)\cmd\winfe\edhdrdlg.cpp
	$(DEPTH)\cmd\winfe\mailfrm.cpp
	$(DEPTH)\cmd\winfe\mailfrm2.cpp
	$(DEPTH)\cmd\winfe\mailmisc.cpp
	$(DEPTH)\cmd\winfe\mailpriv.cpp
	$(DEPTH)\cmd\winfe\mailqf.cpp
	$(DEPTH)\cmd\winfe\mapihook.cpp
	$(DEPTH)\cmd\winfe\mapismem.cpp
	$(DEPTH)\cmd\winfe\mapimail.cpp
	$(DEPTH)\cmd\winfe\nsstrseq.cpp
	$(DEPTH)\cmd\winfe\mnprefs.cpp  
	$(DEPTH)\cmd\winfe\mnwizard.cpp  
	$(DEPTH)\cmd\winfe\msgfrm.cpp
	$(DEPTH)\cmd\winfe\msgtmpl.cpp
	$(DEPTH)\cmd\winfe\msgview.cpp
	$(DEPTH)\cmd\winfe\namcomp.cpp
	$(DEPTH)\cmd\winfe\numedit.cpp
	$(DEPTH)\cmd\winfe\srchfrm.cpp 
	$(DEPTH)\cmd\winfe\subnews.cpp 
	$(DEPTH)\cmd\winfe\taskbar.cpp 
	$(DEPTH)\cmd\winfe\thrdfrm.cpp
!endif
!ifdef EDITOR
	$(DEPTH)\cmd\winfe\edtrccln.cpp
	$(DEPTH)\cmd\winfe\edtclass.cpp
	$(DEPTH)\cmd\winfe\spellcli.cpp
!endif
!ifdef MOZ_MAIL_NEWS
	$(DEPTH)\cmd\winfe\dlgdwnld.cpp
	$(DEPTH)\cmd\winfe\dlghtmmq.cpp
	$(DEPTH)\cmd\winfe\dlghtmrp.cpp
	$(DEPTH)\cmd\winfe\dlgseldg.cpp
   $(DEPTH)\cmd\winfe\nsadrlst.cpp
   $(DEPTH)\cmd\winfe\nsadrnam.cpp
   $(DEPTH)\cmd\winfe\nsadrtyp.cpp
	$(DEPTH)\cmd\winfe\fldrfrm.cpp        
   $(DEPTH)\cmd\winfe\dspppage.cpp
	$(DEPTH)\cmd\winfe\srchdlg.cpp
	$(DEPTH)\cmd\winfe\srchobj.cpp
	$(DEPTH)\cmd\winfe\mnrccln.cpp
!endif
	$(DEPTH)\cmd\winfe\setupwiz.cpp  
	$(DEPTH)\cmd\winfe\ngdwtrst.cpp
	$(DEPTH)\cmd\winfe\animbar.cpp   
	$(DEPTH)\cmd\winfe\animbar2.cpp   
	$(DEPTH)\cmd\winfe\apiapi.cpp 
	$(DEPTH)\cmd\winfe\animecho.cpp
	$(DEPTH)\cmd\winfe\askmedlg.cpp 
	$(DEPTH)\cmd\winfe\authdll.cpp
	$(DEPTH)\cmd\winfe\button.cpp 
	$(DEPTH)\cmd\winfe\cfe.cpp
	$(DEPTH)\cmd\winfe\cmdparse.cpp
	$(DEPTH)\cmd\winfe\cntritem.cpp   
	$(DEPTH)\cmd\winfe\confhook.cpp
	$(DEPTH)\cmd\winfe\csttlbr2.cpp
	$(DEPTH)\cmd\winfe\custom.cpp 
	$(DEPTH)\cmd\winfe\cuvfm.cpp
	$(DEPTH)\cmd\winfe\cuvfs.cpp
	$(DEPTH)\cmd\winfe\cvffc.cpp
	$(DEPTH)\cmd\winfe\cxabstra.cpp   
	$(DEPTH)\cmd\winfe\cxdc.cpp   
	$(DEPTH)\cmd\winfe\cxdc1.cpp   
	$(DEPTH)\cmd\winfe\cxicon.cpp
	$(DEPTH)\cmd\winfe\cxinit.cpp
	$(DEPTH)\cmd\winfe\cxmeta.cpp 
	$(DEPTH)\cmd\winfe\cxnet1.cpp 
	$(DEPTH)\cmd\winfe\cxpane.cpp
	$(DEPTH)\cmd\winfe\cxprint.cpp
	$(DEPTH)\cmd\winfe\cxprndlg.cpp   
	$(DEPTH)\cmd\winfe\cxsave.cpp 
	$(DEPTH)\cmd\winfe\cxstubs.cpp
	$(DEPTH)\cmd\winfe\cxwin.cpp
	$(DEPTH)\cmd\winfe\cxwin1.cpp
	$(DEPTH)\cmd\winfe\dateedit.cpp
	$(DEPTH)\cmd\winfe\dde.cpp
	$(DEPTH)\cmd\winfe\ddecmd.cpp
	$(DEPTH)\cmd\winfe\ddectc.cpp 
	$(DEPTH)\cmd\winfe\dialog.cpp 
	$(DEPTH)\cmd\winfe\display.cpp
	$(DEPTH)\cmd\winfe\dragbar.cpp   
	$(DEPTH)\cmd\winfe\drawable.cpp   
	$(DEPTH)\cmd\winfe\dropmenu.cpp   
	$(DEPTH)\cmd\winfe\edcombtb.cpp   
	$(DEPTH)\cmd\winfe\extgen.cpp
	$(DEPTH)\cmd\winfe\extview.cpp
	$(DEPTH)\cmd\winfe\feembed.cpp
	$(DEPTH)\cmd\winfe\fegrid.cpp 
	$(DEPTH)\cmd\winfe\fegui.cpp  
	$(DEPTH)\cmd\winfe\feimage.cpp   
	$(DEPTH)\cmd\winfe\feimages.cpp   
	$(DEPTH)\cmd\winfe\feorphan.cpp 
	$(DEPTH)\cmd\winfe\feorphn2.cpp 
	$(DEPTH)\cmd\winfe\femess.cpp 
	$(DEPTH)\cmd\winfe\fenet.cpp  
	$(DEPTH)\cmd\winfe\feselect.cpp 
	$(DEPTH)\cmd\winfe\feutil.cpp 
	$(DEPTH)\cmd\winfe\findrepl.cpp   
	$(DEPTH)\cmd\winfe\fmabstra.cpp   
	$(DEPTH)\cmd\winfe\fmbutton.cpp   
	$(DEPTH)\cmd\winfe\fmfile.cpp 
	$(DEPTH)\cmd\winfe\fmradio.cpp
	$(DEPTH)\cmd\winfe\fmrdonly.cpp   
	$(DEPTH)\cmd\winfe\fmselmul.cpp   
	$(DEPTH)\cmd\winfe\fmselone.cpp   
	$(DEPTH)\cmd\winfe\fmtext.cpp 
	$(DEPTH)\cmd\winfe\fmtxarea.cpp
	$(DEPTH)\cmd\winfe\frameglu.cpp   
	$(DEPTH)\cmd\winfe\framinit.cpp   
	$(DEPTH)\cmd\winfe\genchrom.cpp   
	$(DEPTH)\cmd\winfe\gendoc.cpp 
	$(DEPTH)\cmd\winfe\genedit.cpp
	$(DEPTH)\cmd\winfe\genframe.cpp   
	$(DEPTH)\cmd\winfe\genfram2.cpp   
	$(DEPTH)\cmd\winfe\prefs.cpp   
	$(DEPTH)\cmd\winfe\genview.cpp
	$(DEPTH)\cmd\winfe\gridedge.cpp   
	$(DEPTH)\cmd\winfe\helpers.cpp
	$(DEPTH)\cmd\winfe\hiddenfr.cpp   
	$(DEPTH)\cmd\winfe\histbld.cpp   
	$(DEPTH)\cmd\winfe\imagemap.cpp
!if "$(MOZ_BITS)"  == "32"
	$(DEPTH)\cmd\winfe\intelli.cpp
!endif
	$(DEPTH)\cmd\winfe\intlwin.cpp
	$(DEPTH)\cmd\winfe\ipframe.cpp
	$(DEPTH)\cmd\winfe\lastacti.cpp
	$(DEPTH)\cmd\winfe\logindg.cpp   
	$(DEPTH)\cmd\winfe\mainfrm.cpp
	$(DEPTH)\cmd\winfe\medit.cpp  
	$(DEPTH)\cmd\winfe\mozock.cpp 
	$(DEPTH)\cmd\winfe\mucwiz.cpp  
	$(DEPTH)\cmd\winfe\mucproc.cpp 
	$(DEPTH)\cmd\winfe\navbar.cpp 
	$(DEPTH)\cmd\winfe\navcntr.cpp  
	$(DEPTH)\cmd\winfe\navcontv.cpp  
	$(DEPTH)\cmd\winfe\navfram.cpp  
	$(DEPTH)\cmd\winfe\navigate.cpp   
	$(DEPTH)\cmd\winfe\ncapiurl.cpp 
	$(DEPTH)\cmd\winfe\nethelp.cpp  
	$(DEPTH)\cmd\winfe\mozilla.cpp   
	$(DEPTH)\cmd\winfe\nsapp.cpp   
	$(DEPTH)\cmd\winfe\netsdoc.cpp
	$(DEPTH)\cmd\winfe\nsfont.cpp
	$(DEPTH)\cmd\winfe\netsprnt.cpp   
	$(DEPTH)\cmd\winfe\netsvw.cpp 
	$(DEPTH)\cmd\winfe\nsshell.cpp
	$(DEPTH)\cmd\winfe\odctrl.cpp
	$(DEPTH)\cmd\winfe\olectc.cpp 
	$(DEPTH)\cmd\winfe\olehelp.cpp 
	$(DEPTH)\cmd\winfe\oleprot1.cpp   
	$(DEPTH)\cmd\winfe\oleregis.cpp   
	$(DEPTH)\cmd\winfe\olestart.cpp   
	$(DEPTH)\cmd\winfe\oleshut.cpp   
	$(DEPTH)\cmd\winfe\oleview.cpp   
	$(DEPTH)\cmd\winfe\oleview1.cpp   
	$(DEPTH)\cmd\winfe\outliner.cpp 
	$(DEPTH)\cmd\winfe\ownedlst.cpp  
	$(DEPTH)\cmd\winfe\pain.cpp
	$(DEPTH)\cmd\winfe\plginvw.cpp
	$(DEPTH)\cmd\winfe\popup.cpp  
	$(DEPTH)\cmd\winfe\prefinfo.cpp   
	$(DEPTH)\cmd\winfe\presentm.cpp   
	$(DEPTH)\cmd\winfe\printpag.cpp   
	$(DEPTH)\cmd\winfe\profile.cpp   
	$(DEPTH)\cmd\winfe\qahook.cpp  
	$(DEPTH)\cmd\winfe\quickfil.cpp 
	$(DEPTH)\cmd\winfe\rdfliner.cpp   
	$(DEPTH)\cmd\winfe\region.cpp   
	$(DEPTH)\cmd\winfe\regproto.cpp 
	$(DEPTH)\cmd\winfe\shcut.cpp  
	$(DEPTH)\cmd\winfe\shcutdlg.cpp   
	$(DEPTH)\cmd\winfe\slavewnd.cpp 
	$(DEPTH)\cmd\winfe\splash.cpp 
	$(DEPTH)\cmd\winfe\spiwrap.c 
	$(DEPTH)\cmd\winfe\srvritem.cpp   
	$(DEPTH)\cmd\winfe\statbar.cpp 
	$(DEPTH)\cmd\winfe\stshfont.cpp 
!ifdef MOZ_LOC_INDEP
	$(DEPTH)\cmd\winfe\stshli.cpp 
!endif
	$(DEPTH)\cmd\winfe\stshplug.cpp 
	$(DEPTH)\cmd\winfe\styles.cpp 
	$(DEPTH)\cmd\winfe\sysinfo.cpp
	$(DEPTH)\cmd\winfe\template.cpp
!if "$(MOZ_USERNAME)" == "WHITEBOX"
    $(DEPTH)\cmd\winfe\qadelmsg.cpp
    $(DEPTH)\cmd\winfe\qaoutput.cpp
	$(DEPTH)\cmd\winfe\qatrace.cpp
	$(DEPTH)\cmd\winfe\qaui.cpp
    $(DEPTH)\cmd\winfe\testcase.cpp
    $(DEPTH)\cmd\winfe\testcasemanager.cpp
    $(DEPTH)\cmd\winfe\tclist.cpp
    $(DEPTH)\cmd\winfe\testcasedlg.cpp
!endif 
	$(DEPTH)\cmd\winfe\timer.cpp  
	$(DEPTH)\cmd\winfe\tip.cpp 
	$(DEPTH)\cmd\winfe\tlbutton.cpp   
	$(DEPTH)\cmd\winfe\toolbar2.cpp 
	$(DEPTH)\cmd\winfe\tooltip.cpp  
	$(DEPTH)\cmd\winfe\urlbar.cpp 
	$(DEPTH)\cmd\winfe\urlecho.cpp
	$(DEPTH)\cmd\winfe\usertlbr.cpp
	$(DEPTH)\cmd\winfe\viewerse.cpp    
	$(DEPTH)\cmd\winfe\winclose.cpp   
	$(DEPTH)\cmd\winfe\winpref.c
!ifdef MOZ_LOC_INDEP
	$(DEPTH)\cmd\winfe\winli.cpp
!endif
	$(DEPTH)\cmd\winfe\resdll\resdll.c
!if "$(MOZ_BITS)"=="32"
	$(DEPTH)\cmd\winfe\talk.cpp
!endif
	$(DEPTH)\cmd\winfe\nsguids.cpp
!if "$(MOZ_BITS)" == "16"
	$(DEPTH)\cmd\winfe\except.cpp
!endif
	$(DEPTH)\cmd\winfe\xpstrsw.cpp
	$(DEPTH)\cmd\winfe\widgetry.cpp
	$(DEPTH)\cmd\winfe\woohoo.cpp
<<

$(DEPTH)\cmd\winfe\mkfiles32\makedep.exe: $(DEPTH)\cmd\winfe\mkfiles32\makedep.cpp
!if "$(MOZ_BITS)"=="32"
    @cl -MT -Fo"$(OUTDIR)/" -Fe"$(DEPTH)\cmd\winfe\mkfiles32\makedep.exe" $(DEPTH)\cmd\winfe\mkfiles32\makedep.cpp
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

$(GENDIR)\personal.rc: $(DEPTH)\modules\rdf\images\personal.gif
	$(BIN2RC) $(DEPTH)\modules\rdf\images\personal.gif image/gif > $(GENDIR)\personal.rc

$(GENDIR)\history.rc: $(DEPTH)\modules\rdf\images\history.gif
	$(BIN2RC) $(DEPTH)\modules\rdf\images\history.gif image/gif > $(GENDIR)\history.rc

$(GENDIR)\channels.rc: $(DEPTH)\modules\rdf\images\channels.gif
	$(BIN2RC) $(DEPTH)\modules\rdf\images\channels.gif image/gif > $(GENDIR)\channels.rc

$(GENDIR)\sitemap.rc: $(DEPTH)\modules\rdf\images\sitemap.gif
	$(BIN2RC) $(DEPTH)\modules\rdf\images\sitemap.gif image/gif > $(GENDIR)\sitemap.rc

$(GENDIR)\search.rc: $(DEPTH)\modules\rdf\images\search.gif
	$(BIN2RC) $(DEPTH)\modules\rdf\images\search.gif image/gif > $(GENDIR)\search.rc

$(GENDIR)\guide.rc: $(DEPTH)\modules\rdf\images\guide.gif
	$(BIN2RC) $(DEPTH)\modules\rdf\images\guide.gif image/gif > $(GENDIR)\guide.rc

$(GENDIR)\file.rc: $(DEPTH)\modules\rdf\images\file.gif
	$(BIN2RC) $(DEPTH)\modules\rdf\images\file.gif image/gif > $(GENDIR)\file.rc

$(GENDIR)\ldap.rc: $(DEPTH)\modules\rdf\images\ldap.gif
	$(BIN2RC) $(DEPTH)\modules\rdf\images\ldap.gif image/gif > $(GENDIR)\ldap.rc

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
$(GENDIR)\javalogo.rc: $(DEPTH)\lib\xp\javalogo.gif
	$(BIN2RC) $(DEPTH)\lib\xp\javalogo.gif image/gif > $(GENDIR)\javalogo.rc
!endif

!ifdef FORTEZZA
$(GENDIR)\litronic.rc: $(DEPTH)\lib\xp\litronic.gif
	$(BIN2RC) $(DEPTH)\lib\xp\litronic.gif image/gif > $(GENDIR)\litronic.rc
!endif

$(GENDIR)\biglogo.rc: $(DEPTH)\lib\xp\biglogo.gif
	$(BIN2RC) $(DEPTH)\lib\xp\biglogo.gif image/gif > $(GENDIR)\biglogo.rc

$(GENDIR)\rsalogo.rc: $(DEPTH)\lib\xp\rsalogo.gif
	$(BIN2RC) $(DEPTH)\lib\xp\rsalogo.gif image/gif > $(GENDIR)\rsalogo.rc

$(GENDIR)\qt_logo.rc: $(DEPTH)\lib\xp\qt_logo.gif
	$(BIN2RC) $(DEPTH)\lib\xp\qt_logo.gif image/gif > $(GENDIR)\qt_logo.rc

$(GENDIR)\visilogo.rc: $(DEPTH)\lib\xp\visilogo.gif
	$(BIN2RC) $(DEPTH)\lib\xp\visilogo.gif image/gif > $(GENDIR)\visilogo.rc

$(GENDIR)\coslogo.rc: $(DEPTH)\lib\xp\coslogo.jpg
	$(BIN2RC) $(DEPTH)\lib\xp\coslogo.jpg image/jpeg > $(GENDIR)\coslogo.rc

$(GENDIR)\insologo.rc: $(DEPTH)\lib\xp\insologo.gif
	$(BIN2RC) $(DEPTH)\lib\xp\insologo.gif image/gif > $(GENDIR)\insologo.rc

$(GENDIR)\mclogo.rc: $(DEPTH)\lib\xp\mclogo.gif
	$(BIN2RC) $(DEPTH)\lib\xp\mclogo.gif image/gif > $(GENDIR)\mclogo.rc

$(GENDIR)\ncclogo.rc: $(DEPTH)\lib\xp\ncclogo.gif
	$(BIN2RC) $(DEPTH)\lib\xp\ncclogo.gif image/gif > $(GENDIR)\ncclogo.rc

$(GENDIR)\odilogo.rc: $(DEPTH)\lib\xp\odilogo.gif
	$(BIN2RC) $(DEPTH)\lib\xp\odilogo.gif image/gif > $(GENDIR)\odilogo.rc

$(GENDIR)\symlogo.rc: $(DEPTH)\lib\xp\symlogo.gif
	$(BIN2RC) $(DEPTH)\lib\xp\symlogo.gif image/gif > $(GENDIR)\symlogo.rc

$(GENDIR)\tdlogo.rc: $(DEPTH)\lib\xp\tdlogo.gif
	$(BIN2RC) $(DEPTH)\lib\xp\tdlogo.gif image/gif > $(GENDIR)\tdlogo.rc
!else

AboutImages: $(GENDIR) \
	$(GENDIR)\flamer.rc
	
$(GENDIR)\flamer.rc: $(DEPTH)\lib\xp\flamer.gif
	$(BIN2RC) $(DEPTH)\lib\xp\flamer.gif image/gif > $(GENDIR)\flamer.rc
!endif


#
# Creation of resource files needed for building
#
#
prebuild: $(GENDIR) $(GENDIR)\initpref.rc $(GENDIR)\allpref.rc \
	$(GENDIR)\allpref2.rc $(GENDIR)\allpref3.rc $(GENDIR)\allpref4.rc\
	$(GENDIR)\winpref.rc $(GENDIR)\config.rc NavCenterImages \
	AboutImages
	
$(GENDIR)\initpref.rc: $(DEPTH)\modules\libpref\src\initpref.js
	$(TXT2RC) init_prefs $(DEPTH)\modules\libpref\src\initpref.js \
		$(GENDIR)\initpref.rc

$(GENDIR)\allpref.rc: $(DEPTH)\modules\libpref\src\init\all.js
	$(TXT2RC) all_prefs $(DEPTH)\modules\libpref\src\init\all.js \
		$(GENDIR)\allpref.rc

$(GENDIR)\allpref2.rc: $(DEPTH)\modules\libpref\src\init\mailnews.js
	$(TXT2RC) mailnews_prefs $(DEPTH)\modules\libpref\src\init\mailnews.js \
		$(GENDIR)\allpref2.rc

$(GENDIR)\allpref3.rc: $(DEPTH)\modules\libpref\src\init\editor.js
	$(TXT2RC) editor_prefs $(DEPTH)\modules\libpref\src\init\editor.js \
		$(GENDIR)\allpref3.rc

$(GENDIR)\allpref4.rc: $(DEPTH)\modules\libpref\src\init\security.js
	$(TXT2RC) security_prefs $(DEPTH)\modules\libpref\src\init\security.js \
		$(GENDIR)\allpref4.rc

$(GENDIR)\winpref.rc: $(DEPTH)\modules\libpref\src\win\winpref.js
	$(TXT2RC) win_prefs $(DEPTH)\modules\libpref\src\win\winpref.js \
	    $(GENDIR)\winpref.rc

# May need a new one for MOZ_MEDIUM.
!ifndef MOZ_COMMUNICATOR_CONFIG_JS
$(GENDIR)\config.rc:  $(DEPTH)\modules\libpref\src\init\configr.js
	$(TXT2RC) config_prefs $(DEPTH)\modules\libpref\src\init\configr.js \
		$(GENDIR)\config.rc
!else
$(GENDIR)\config.rc:  $(DEPTH)\modules\libpref\src\init\config.js
	$(TXT2RC) config_prefs $(DEPTH)\modules\libpref\src\init\config.js \
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
!IF EXIST($(DEPTH)\cmd\winfe\nstdfp32.dll)
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
!IF EXIST($(DEPTH)\cmd\winfe\nstdfp16.dll)
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

!if "$(_MSC_VER)" == "1100"
REBASE=rebase.exe
!if [for %i in (. %PATH%) do $(QUIET)if exist %i\$(REBASE) echo %i\$(REBASE) > rebase.yes]
!endif
!if exist(rebase.yes)
!if [for %i in ($(OUTDIR)\*.dll) do $(QUIET)echo %i >> rebase.lst]
!endif
!if [for %i in ($(OUTDIR)\java\bin\*.dll) do $(QUIET)echo %i >> rebase.lst]
!endif
!if [for %i in ($(OUTDIR)\spellchk\*.dll) do $(QUIET)echo %i >> rebase.lst]
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
!else
rebase:

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

$(OUTDIR)\dynfonts\nstdfp32.dll:   $(DEPTH)\cmd\winfe\nstdfp32.dll
    @IF NOT EXIST "$(OUTDIR)\dynfonts/$(NULL)" mkdir "$(OUTDIR)\dynfonts"
    @IF EXIST $(DEPTH)\cmd\winfe\nstdfp32.dll copy $(DEPTH)\cmd\winfe\nstdfp32.dll $(OUTDIR)\dynfonts\nstdfp32.dll

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

$(OUTDIR)\dynfonts\nstdfp16.dll:   $(DEPTH)\cmd\winfe\nstdfp16.dll
    @IF NOT EXIST "$(OUTDIR)\dynfonts/$(NULL)" mkdir "$(OUTDIR)\dynfonts"
    @IF EXIST $(DEPTH)\cmd\winfe\nstdfp16.dll copy $(DEPTH)\cmd\winfe\nstdfp16.dll $(OUTDIR)\dynfonts\nstdfp16.dll

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
    $(DEPTH)\cmd\winfe\mkfiles32\waitfor $(TMP)\bb1.sem
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
    $(DIST)\lib\abouturl.lib +
    $(DIST)\lib\dataurl.lib +
    $(DIST)\lib\fileurl.lib +
    $(DIST)\lib\ftpurl.lib +
    $(DIST)\lib\gophurl.lib +
    $(DIST)\lib\httpurl.lib +
    $(DIST)\lib\jsurl.lib +
    $(DIST)\lib\marimurl.lib +
    $(DIST)\lib\remoturl.lib +
    $(DIST)\lib\netcache.lib +
    $(DIST)\lib\netcnvts.lib +
    $(DIST)\lib\network.lib +
    $(DIST)\lib\cnetinit.lib +
!ifdef MOZ_MAIL_NEWS
	$(DIST)\lib\mnrc16.lib +
!endif
    $(DIST)\lib\zip$(MOZ_BITS)$(VERSION_NUMBER).lib +
    $(DIST)\lib\jpeg$(MOZ_BITS)$(VERSION_NUMBER).lib +
    $(DIST)\lib\dbm$(MOZ_BITS).lib +
    $(BINREL_DIST)\lib\watcomfx.lib
    $(DEPTH)\cmd\winfe\mozilla.def;
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
	$(DEPTH)\cmd\winfe\mozilla.rc\
	$(DEPTH)\cmd\winfe\editor.rc\
	$(DEPTH)\cmd\winfe\edres2.h\
	$(GENDIR)\allpref.rc\
	$(GENDIR)\allpref2.rc\
	$(GENDIR)\allpref3.rc\
	$(GENDIR)\allpref4.rc\
	$(GENDIR)\initpref.rc\
	$(GENDIR)\winpref.rc\
	$(GENDIR)\config.rc\
	$(DEPTH)\cmd\winfe\res\convtbls.rc\
	$(DEPTH)\cmd\winfe\res\editor.rc2\
	$(DEPTH)\cmd\winfe\res\license.rc\
	$(DEPTH)\cmd\winfe\res\mail.rc\
	$(DEPTH)\cmd\winfe\res\mozilla.rc2\
	$(DEPTH)\cmd\winfe\res\mozilla.rc3\
	\
	$(DEPTH)\cmd\winfe\res\address.bmp\
	$(DEPTH)\cmd\winfe\res\addrnew.bmp\
	$(DEPTH)\cmd\winfe\res\arrow1.bmp\
	$(DEPTH)\cmd\winfe\res\arrow2.bmp\
	$(DEPTH)\cmd\winfe\res\BITMAP3.bmp\
	$(DEPTH)\cmd\winfe\res\bkfdopen.bmp\
	$(DEPTH)\cmd\winfe\res\bkmkfld2.bmp\
	$(DEPTH)\cmd\winfe\res\bm_qf.bmp\
	$(DEPTH)\cmd\winfe\res\BMKITEM.bmp\
	$(DEPTH)\cmd\winfe\res\BMP00001.bmp\
	$(DEPTH)\cmd\winfe\res\BMP00002.bmp\
	$(DEPTH)\cmd\winfe\res\bookmark.bmp\
	$(DEPTH)\cmd\winfe\res\collect.bmp\
	$(DEPTH)\cmd\winfe\res\column.bmp\
	$(DEPTH)\cmd\winfe\res\compbar.bmp\
	$(DEPTH)\cmd\winfe\res\comptabs.bmp\
	$(DEPTH)\cmd\winfe\res\DOWND.bmp\
	$(DEPTH)\cmd\winfe\res\DOWNF.bmp\
	$(DEPTH)\cmd\winfe\res\DOWNU.bmp\
	$(DEPTH)\cmd\winfe\res\DOWNX.bmp\
	$(DEPTH)\cmd\winfe\res\EDAL_A_U.bmp\
	$(DEPTH)\cmd\winfe\res\EDAL_B_U.bmp\
	$(DEPTH)\cmd\winfe\res\EDAL_C_U.bmp\
	$(DEPTH)\cmd\winfe\res\EDAL_L_U.bmp\
	$(DEPTH)\cmd\winfe\res\EDAL_R_U.bmp\
	$(DEPTH)\cmd\winfe\res\EDAL_T_U.bmp\
	$(DEPTH)\cmd\winfe\res\EDALCB_U.bmp\
	$(DEPTH)\cmd\winfe\res\edalignc.bmp\
	$(DEPTH)\cmd\winfe\res\edalignl.bmp\
	$(DEPTH)\cmd\winfe\res\edalignr.bmp\
	$(DEPTH)\cmd\winfe\res\edanch.bmp\
	$(DEPTH)\cmd\winfe\res\edanchm.bmp\
	$(DEPTH)\cmd\winfe\res\edcombo.bmp\
	$(DEPTH)\cmd\winfe\res\edform.bmp\
	$(DEPTH)\cmd\winfe\res\edformm.bmp\
	$(DEPTH)\cmd\winfe\res\edhrule.bmp\
	$(DEPTH)\cmd\winfe\res\edimage.bmp\
	$(DEPTH)\cmd\winfe\res\edlink.bmp\
	$(DEPTH)\cmd\winfe\res\edtable.bmp\
	$(DEPTH)\cmd\winfe\res\edtag.bmp\
	$(DEPTH)\cmd\winfe\res\edtage.bmp\
	$(DEPTH)\cmd\winfe\res\edtagem.bmp\
	$(DEPTH)\cmd\winfe\res\edtagm.bmp\
	$(DEPTH)\cmd\winfe\res\edtarg.bmp\
	$(DEPTH)\cmd\winfe\res\EDTARGET.bmp\
	$(DEPTH)\cmd\winfe\res\FILE.bmp\
	$(DEPTH)\cmd\winfe\res\GAUDIO.bmp\
	$(DEPTH)\cmd\winfe\res\GBINARY.bmp\
	$(DEPTH)\cmd\winfe\res\GFIND.bmp\
	$(DEPTH)\cmd\winfe\res\GFOLDER.bmp\
	$(DEPTH)\cmd\winfe\res\GGENERIC.bmp\
	$(DEPTH)\cmd\winfe\res\GIMAGE.bmp\
	$(DEPTH)\cmd\winfe\res\GMOVIE.bmp\
	$(DEPTH)\cmd\winfe\res\GOPHER_F.bmp\
	$(DEPTH)\cmd\winfe\res\GTELNET.bmp\
	$(DEPTH)\cmd\winfe\res\GTEXT.bmp\
	$(DEPTH)\cmd\winfe\res\IBAD.bmp\
	$(DEPTH)\cmd\winfe\res\IMAGE_BA.bmp\
	$(DEPTH)\cmd\winfe\res\IMAGE_MA.bmp\
	$(DEPTH)\cmd\winfe\res\IREPLACE.bmp\
	$(DEPTH)\cmd\winfe\res\IUNKNOWN.bmp\
	$(DEPTH)\cmd\winfe\res\mailcol.bmp\
	$(DEPTH)\cmd\winfe\res\mailingl.bmp\
	$(DEPTH)\cmd\winfe\res\mailthrd.bmp\
	$(DEPTH)\cmd\winfe\res\msgback.bmp\
	$(DEPTH)\cmd\winfe\res\N.bmp\
	$(DEPTH)\cmd\winfe\res\newsart.bmp\
	$(DEPTH)\cmd\winfe\res\newsthrd.bmp\
	$(DEPTH)\cmd\winfe\res\outliner.bmp\
	$(DEPTH)\cmd\winfe\res\outlmail.bmp\
	$(DEPTH)\cmd\winfe\res\person.bmp\
	$(DEPTH)\cmd\winfe\res\PICTURES.bmp\
	$(DEPTH)\cmd\winfe\res\PICTURES.bmp\
	$(DEPTH)\cmd\winfe\res\smidelay.bmp\
	$(DEPTH)\cmd\winfe\res\smmask.bmp\
	$(DEPTH)\cmd\winfe\res\SREPLACE.bmp\
	$(DEPTH)\cmd\winfe\res\SSECURE.bmp\
	$(DEPTH)\cmd\winfe\res\submenu.bmp\
	$(DEPTH)\cmd\winfe\res\submenu2.bmp\
	$(DEPTH)\cmd\winfe\res\taskbarl.bmp\
	$(DEPTH)\cmd\winfe\res\taskbars.bmp\
	$(DEPTH)\cmd\winfe\res\tb_dock.bmp\
	$(DEPTH)\cmd\winfe\res\UPD.bmp\
	$(DEPTH)\cmd\winfe\res\UPF.bmp\
	$(DEPTH)\cmd\winfe\res\UPU.bmp\
	$(DEPTH)\cmd\winfe\res\UPX.bmp\
	$(DEPTH)\cmd\winfe\res\vflippy.bmp\
	\
	$(DEPTH)\cmd\winfe\res\ADRESSWD.ico\
	$(DEPTH)\cmd\winfe\res\BOOKMKWD.ico\
	$(DEPTH)\cmd\winfe\res\COMPOSWD.ico\
	$(DEPTH)\cmd\winfe\res\COMPOSWD.ico\
	$(DEPTH)\cmd\winfe\res\idelay.ico\
	$(DEPTH)\cmd\winfe\res\IDR_DOC.ico\
	$(DEPTH)\cmd\winfe\res\IDR_DOC.ico\
	$(DEPTH)\cmd\winfe\res\mail.ico\
	$(DEPTH)\cmd\winfe\res\MAILWD.ico\
	$(DEPTH)\cmd\winfe\res\MAILWD.ico\
	$(DEPTH)\cmd\winfe\res\MAILWD.ico\
	$(DEPTH)\cmd\winfe\res\NEWSWD.ico\
	$(DEPTH)\cmd\winfe\res\NEWSWD.ico\
	$(DEPTH)\cmd\winfe\res\NEWSWD.ico\
	$(DEPTH)\cmd\winfe\res\SILVER.ico\
	$(DEPTH)\cmd\winfe\res\SRCHWD.ico\
	\
	$(DEPTH)\cmd\winfe\res\actembed.cur\
	$(DEPTH)\cmd\winfe\res\edhtmlcp.cur\
	$(DEPTH)\cmd\winfe\res\edhtmlmv.cur\
	$(DEPTH)\cmd\winfe\res\edimgcp.cur\
	$(DEPTH)\cmd\winfe\res\edimgmv.cur\
	$(DEPTH)\cmd\winfe\res\edlinkcp.cur\
	$(DEPTH)\cmd\winfe\res\edlinkmv.cur\
	$(DEPTH)\cmd\winfe\res\edtbeaml.cur\
	$(DEPTH)\cmd\winfe\res\edtbeams.cur\
	$(DEPTH)\cmd\winfe\res\edtextcp.cur\
	$(DEPTH)\cmd\winfe\res\edtextmv.cur\


$(OUTDIR)\mozilla.res : $(RES_FILES) "$(OUTDIR)"
    @SET SAVEINCLUDE=%%INCLUDE%%
    @SET INCLUDE=$(RCINCLUDES);$(RCDISTINCLUDES);%%SAVEINCLUDE%%
    $(RSC) /Fo$(PROD)$(VERSTR).res $(RCFILEFLAGS) $(RCFLAGS) $(DEPTH)\cmd\winfe\mozilla.rc
    @IF EXIST $(PROD)$(VERSTR).res copy $(PROD)$(VERSTR).res $(OUTDIR)\mozilla.res 
    @IF EXIST $(PROD)$(VERSTR).res del $(PROD)$(VERSTR).res
    @SET INCLUDE=%%SAVEINCLUDE%%
    @SET SAVEINCLUDE=

$(OUTDIR)\appicon.res : $(DEPTH)\cmd\winfe\res\silver.ico "$(OUTDIR)"
    @SET SAVEINCLUDE=%%INCLUDE%%
    @SET INCLUDE=$(DEPTH)\cmd\winfe\res;%%SAVEINCLUDE%%
    $(RSC) /Fo$(PROD)$(VERSTR).res $(RCFILEFLAGS) $(RCFLAGS) $(DEPTH)\cmd\winfe\res\appicon.rc
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
    $(DEPTH)\cmd\winfe\mozilla.odl
!ELSE
    $(DEPTH)\cmd\winfe\nscape16.odl
!ENDIF

PRECOMPILED_TLB= \
!IF "$(MOZ_BITS)"=="32"
    $(DEPTH)\cmd\winfe\mozilla.tlb
!ELSE
    $(DEPTH)\cmd\winfe\nscape16.tlb
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
    -xcopy $(DEPTH)\lib\layout\*.h $(EXPORTINC) $(XCF)
    -xcopy $(DEPTH)\lib\libstyle\*.h $(EXPORTINC) $(XCF)
    -xcopy $(DEPTH)\lib\liblayer\include\*.h $(EXPORTINC) $(XCF)
    -xcopy $(DEPTH)\lib\libi18n\*.h $(EXPORTINC) $(XCF)
    -xcopy $(DEPTH)\lib\libjar\*.h $(EXPORTINC) $(XCF)
    -xcopy $(DEPTH)\lib\libparse\*.h $(EXPORTINC) $(XCF)
!ifdef MOZ_MAIL_NEWS
    -xcopy $(DEPTH)\lib\libaddr\*.h $(EXPORTINC) $(XCF)
    -xcopy $(DEPTH)\lib\libmsg\*.h $(EXPORTINC) $(XCF)
    -xcopy $(DEPTH)\lib\libneo\*.h $(EXPORTINC) $(XCF)
!endif
!ifdef MOZ_LDAP
    -xcopy $(DEPTH)\netsite\ldap\include\*.h $(EXPORTINC) $(XCF)
    -xcopy $(XPDIST)\public\ldap\*.h $(EXPORTINC) $(XCF)
!endif
!ifdef EDITOR
!ifdef MOZ_JAVA
    -xcopy $(DEPTH)\modules\edtplug\include\*.h $(EXPORTINC) $(XCF)
!endif
!endif
    -xcopy $(DEPTH)\lib\plugin\*.h $(EXPORTINC) $(XCF)
!if defined(MOZ_JAVA)
    -xcopy $(DEPTH)\modules\applet\include\*.h $(EXPORTINC) $(XCF)
    -xcopy $(DEPTH)\modules\libreg\include\*.h $(EXPORTINC) $(XCF)
!endif
    -xcopy $(DEPTH)\modules\libutil\public\xp_obs.h $(EXPORTINC) $(XCF)
    -xcopy $(DEPTH)\modules\libimg\public\*.h $(EXPORTINC) $(XCF)
    -xcopy $(DEPTH)\modules\libpref\public\*.h $(EXPORTINC) $(XCF)
    -xcopy $(DEPTH)\modules\coreincl\*.h $(EXPORTINC) $(XCF)
!if defined(MOZ_JAVA)
    -xcopy $(DEPTH)\sun-java\jtools\include\*.h $(EXPORTINC) $(XCF)
    -xcopy $(DEPTH)\sun-java\include\*.h $(EXPORTINC) $(XCF)
    -xcopy $(DEPTH)\sun-java\md-include\*.h $(EXPORTINC) $(XCF)
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
    -xcopy $(DEPTH)\dist\public\winfont\*.h $(EXPORTINC) $(XCF)
    -xcopy $(DEPTH)\dist\public\spellchk\*.h $(EXPORTINC) $(XCF)
    -xcopy $(DEPTH)\jpeg\*.h $(EXPORTINC) $(XCF)
    -xcopy $(DEPTH)\lib\libcnv\*.h $(EXPORTINC) $(XCF)

pure:
	$(MOZ_PURIFY)\purify /Run=no /ErrorCallStackLength=20 /AllocCallStackLength=20 \
	/CacheDir="$(DEPTH)\cmd\winfe\mkfiles32\$(PROD)$(VERSTR)\PurifyCache" \
	/Out "$(DEPTH)\cmd\winfe\mkfiles32\$(PROD)$(VERSTR)\mozilla.exe"

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
