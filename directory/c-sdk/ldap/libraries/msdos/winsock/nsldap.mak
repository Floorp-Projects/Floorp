#
#  Windows Makefile for LDAP Client SDK. 
#
#  This makefile produces:
#       16-bit          32-bit
#       ------          ------
#       nsldap.dll      nsldap32.dll    LDAP DLL
#       nsldap.lib      nsldap32.lib    LDAP import library
#       nsldaps.lib     nsldaps32.lib   LDAP static library
#
#  This makefile works the same way as MOZILLA.MAK
#
#  Setting in your environment or the command line will determine what you 
#   build.  
#
#   MOZ_SRC     place holding the ns tree.  Default = 'y:'
#   LDAP_OUT    place you would like output to go.  Default = '.\'
#   MOZ_DEBUG   if defined, you are building debug
#   MOZ_BITS    set to 16 to build Win16, defaults to Win32.
#   MOZ_SEC     set to DOMESTIC for 128 US, defaults to EXPORT.
#   MOZ_TOOLS   place holding the build tools (e.g. makedep.exe)
#   LINK_SEC    if defined and pointing to a directory containing libsec &
#               friends, then link them in. Default not set.
#
#   LDAP_SRC    place holding the ldap tree.  Default = 
#               '$(MOZ_SRC)\mozilla\directory\c-sdk
#   MSVC2       if defined, you are using Visual C++ 2.x tools, 
#               else you are using Visual C++ 4.* tools
#   ALPHA       define to build for DEC Alpha
#   NO_PDB      define to place debug info in object files, rather than PDB.
#
#  In order to build, you first have to build dependencies. Build dependencies
#   by:
#
#       nmake -f nsldap.mak DEPEND=1 MOZ_DEBUG=1
#   
#
#  Once dependencies are built, you can build by:
#
#       nmake -f nsldap.mak MOZ_DEBUG=1
#
#  Build a static library with
#
#       nmake -f nsldap.mak STATIC=1 static

.SUFFIXES: .cpp .c .rc

!if !defined(MOZ_SRC)
MOZ_SRC=y:
!endif

!if !defined(MOZ_BITS)
MOZ_BITS=32
!endif

!if !defined(MOZ_LDAP_VER)
MOZ_LDAP_VER=30
!endif

!if !defined(LDAP_SRC)
!if "$(MOZ_BITS)"=="32"
LDAP_SRC=$(MOZ_SRC)\mozilla\directory\c-sdk
!else
LDAP_SRC=l:
!endif
!endif

!if "$(MOZ_BITS)"=="16" && !EXIST( $(LDAP_SRC)\ldap\Makefile )
!error For Win16 you need to SUBST l: %MOZ_SRC%\mozilla\directory\c-sdk
!endif

!if !defined(LDAP_OUT)
LDAP_OUT=. 
!endif

!if !defined(MOZ_INT)
MOZ_INT=$(LDAP_OUT)
!endif

!if !defined(MOZ_SEC)
MOZ_SEC=EXPORT
!endif

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!if "$(MOZ_BITS)"=="32"
DLL_BITS=32
!endif

CPP=cl.exe /nologo
MTL=mktyplib.exe /nologo
LINK= \
!if "$(MOZ_BITS)"=="32"
    link.exe /nologo
!else
    link
!endif
#    optlinks /BYORDINAL /XNOI /DETAILEDMAP /XREF /WARNDUPS \
#/NTHOST /IMPLIB:"$(OUTDIR)\nsldap$(DLL_BITS).lib"

RSC= \
!if "$(MOZ_BITS)"=="32"
    rc.exe
!else
    rc
!endif

#
# Add different product values here, like dec alpha, mips etc, win16...
#
!if defined(ALPHA)
PROD=alpha
MACHINE=/machine:alpha
MOZ_BITS=32
!else
!if "$(MOZ_BITS)"=="32"
PROD=x86
MACHINE=/machine:i386
!else
PROD=16x86
!endif
!endif

LIBLDAP=$(LDAP_SRC)\ldap\libraries\libldap
LIBLBER=$(LDAP_SRC)\ldap\libraries\liblber
LIBUTIL=$(LDAP_SRC)\ldap\libraries\libutil
BUILDDIR=$(LDAP_SRC)\ldap\libraries\msdos\winsock

# Generated file containing version and build numbers
VERFILE=$(LDAP_SRC)\ldap\include\sdkver.h
VERSRC=$(LDAP_SRC)\ldap\build\dirver.c
VERPROG=$(LDAP_SRC)\ldap\build\dirver.exe
DIRSDK_VERSION=3.0

########## Security #######################
!if defined (LINK_SEC)
!if defined(MOZ_DEBUG)
OPTNAME=dbg
!else
OPTNAME=opt
!endif
#SECDIR=$(MOZ_SRC)\ns\dist\winnt4.0_$(OPTNAME).obj\lib
SECDIR=$(LINK_SEC)

!if "$(MOZ_BITS)"=="32"
# Used by libsec in Win32
RPCLIB=rpcrt4.lib
!endif

!if "$(MOZ_BITS)"=="32"
SECSUPPORT=$(SECDIR)\libsslio.lib $(SECDIR)\libxp.lib $(SECDIR)\libnspr20.lib $(SECDIR)\libdbm.lib $(SECDIR)\libares.lib
!else
SECSUPPORT1=$(SECDIR)\ssl16.lib \
        $(SECDIR)\secutl16.lib \
!ifdef NSPR20
        $(SECDIR)\nspr20.lib \
!else
        $(SECDIR)\pr1640.lib \
!endif
        $(SECDIR)\secnav16.lib \
        $(SECDIR)\cert16.lib \
        $(SECDIR)\sp1640.lib
SECSUPPORT2=$(SECDIR)\key16.lib \
        $(SECDIR)\hash16.lib \
        $(SECDIR)\pkcs716.lib \
        $(SECDIR)\pkcs1216.lib \
        $(SECDIR)\crypto16.lib \
!endif

!if "$(MOZ_SEC)"=="EXPORT"
!if "$(MOZ_BITS)"=="32"
SECLIB=$(SECDIR)\libsec-export.lib $(SECSUPPORT)
!else
SECLIB=$(SECDIR)\secmod16.lib $(SECSUPPORT)
!endif
SSL_FLAG=/DNET_SSL
SECMODEL=\export
LINK_SSL_FLAG=/DLINK_SSL
!else
!if "$(MOZ_SEC)"=="DOMESTIC"
!if "$(MOZ_BITS)"=="32"
SECLIB=$(SECDIR)\libsec-us.lib $(SECSUPPORT)
!else
SECLIB=$(SECDIR)\secmod16.lib $(SECSUPPORT)
!endif
SSL_FLAG=/DNET_SSL
SECMODEL=\domestic
LINK_SSL_FLAG=/DLINK_SSL
!else
SECMODEL=\none
!endif
!endif
!endif
########## end Security ###################


# Static library name
STATICLIB=$(OUTDIR)\nsldaps$(DLL_BITS)v$(MOZ_LDAP_VER).lib

# Get C runtime library version info right
#
!if defined(MSVC2)
# using Visual C++ 2.*
C_RUNTIME=msvcrt.lib
!else
!if defined(MOZ_DEBUG)
C_RUNTIME=msvcrtd.lib
!else
C_RUNTIME=msvcrt.lib
!endif
!endif


# Command to build a static library
LIBCMD= \
!if "$(MOZ_BITS)"=="32"
        link.exe -lib /nologo
!else
        lib.exe /nologo
!endif


#
#       Should reflect non debug settings always,
#               regardless if CFLAGS_DEBUG is doing
#               so also.
#       This is so 16 bits can compile only portions desired
#               as debug (and still link).
#
# Note: the 16-bit DLL does not work if compiled with /Ox
CFLAGS_RELEASE=/DNDEBUG \
!IF "$(MOZ_BITS)"=="32"
 /Ox /Gy \
!if defined(NO_PDB)
 /Z7
!else
 /Zi \
!if !defined(MSVC2)
 /Gm /Gi 
!endif
!endif
!ENDIF

RCFLAGS_RELEASE=/DNODEBUG

LINKFLAGS_RELEASE= \
!if "$(MOZ_BITS)"=="32"
    $(C_RUNTIME) winmm.lib\
!else
        /NOLOGO /NOD /NOI /NOE /PACKC:61440 /ALIGN:16 /ONERROR:NOEXE
#    /STACK:35000 /ALIGN:64 /PACKC:61440 /SEG:1024 /NOD /PACKD /NOI /ONERROR:NOEXE
!endif

###

# To produce a PDB, add /Zi /Gm /Gi and remove /Z7 from CFLAGS_DEBUG, 
# also remove pdb:none from link.
# from link options.
!if defined(MOZ_DEBUG)
!if defined(NO_PDB)
CFLAGS_DEBUG=/Z7 /Od /D_DEBUG $(MOZ_USERDEBUG)
PDB=/pdb:none
!else
CFLAGS_DEBUG=/Zi /Od /D_DEBUG $(MOZ_USERDEBUG) \
!if "$(MOZ_BITS)"=="32"
!if !defined(MSVC2)
 /Gm /Gi \
!endif
 /Gy /DDEBUG
!endif
!endif
!endif

RCFLAGS_DEBUG=/DDEBUG /D_DEBUG

LINKFLAGS_DEBUG= \
!if "$(MOZ_BITS)"=="32"
    /debug /incremental:yes $(C_RUNTIME) winmm.lib\
!else
        /NOLOGO /NOD /NOI /NOE /PACKC:61440 /ALIGN:16 /ONERROR:NOEXE /CO /MAP:FULL
#    /STACK:30000 /ALIGN:128 /PACKC:61440 /SEG:1024 /NOD /PACKD /NOI /CO /ONERROR:NOEXE /NTHOST\
!endif


!IF "$(MOZ_BITS)"=="32"
!if defined(MOZ_DEBUG)
!if !defined(MSVC2)
DLLFLAGS=/MDd
!else
DLLFLAGS=/MD
!endif
!else
DLLFLAGS=/MD
!endif
!else
# 16-bit
!if defined(STATIC)
DLLFLAGS=/AL
!else
DLLFLAGS=/D_WINDLL /ALu
!endif
!endif


############### DEBUG #########################################

!if defined(MOZ_DEBUG)
VERSTR=Dbg
DISTBASE=WIN$(MOZ_BITS)_D.OBJ
CFLAGS=$(CFLAGS_DEBUG)
RCFLAGS=$(RCFLAGS_DEBUG)
LFLAGS=$(LINKFLAGS_DEBUG)

############### RELEASE ########################################

!else
VERSTR=Rel
DISTBASE=WIN$(MOZ_BITS)_O.OBJ
CFLAGS=$(CFLAGS_RELEASE)
RCFLAGS=$(RCFLAGS_RELEASE)
LFLAGS=$(LINKFLAGS_RELEASE)
!endif

############### END ###########################################

DIST_XP = $(MOZ_SRC)\mozilla\dist
DIST=$(DIST_XP)\$(DISTBASE)
DIST_PUBLIC=$(DIST_XP)\public

#
#       Edit these in order to control 16 bit
#               debug targets.
#
CFLAGS_LIBLDAP_C=\
!if "$(MOZ_BITS)"=="32"
    $(CFLAGS)
!else
    $(CFLAGS)
!endif
CFLAGS_LIBLBER_C=\
!if "$(MOZ_BITS)"=="32"
    $(CFLAGS)
!else
    $(CFLAGS)
!endif
CFLAGS_WINSOCK_C=\
!if "$(MOZ_BITS)"=="32"
    $(CFLAGS)
!else
    $(CFLAGS)
!endif
CFLAGS_LIBUTIL_C=\
!if "$(MOZ_BITS)"=="32"
    $(CFLAGS)
!else
    $(CFLAGS)
!endif

OUTDIR=$(LDAP_OUT)$(SECMODEL)\$(PROD)$(VERSTR)

LINK_FLAGS= \
!if "$(MOZ_BITS)"=="32"
    $(OUTDIR)\nsldap.res \
    $(LFLAGS) \
    $(SECLIB) $(RPCLIB) $(C_RUNTIME) oldnames.lib kernel32.lib user32.lib \
        /subsystem:windows $(PDB) $(MACHINE) \
        /dll /def:"$(BUILDDIR)\nsldap$(DLL_BITS).def" \
        /implib:"$(OUTDIR)/nsldap$(DLL_BITS)v$(MOZ_LDAP_VER).lib" \
    /nodefaultlib /out:"$(OUTDIR)/nsldap$(DLL_BITS)v$(MOZ_LDAP_VER).dll" 
!else
    $(LFLAGS) \
!if defined(LINK_SEC)
        /SEG:1024
!endif
!endif

CFLAGS_GENERAL=/c $(DLLFLAGS) /W3 /Fo"$(OUTDIR)/" /Fd"$(OUTDIR)/" \
!if "$(MOZ_BITS)"=="32"
    /GX 
!else
    /G2
!endif

RCFLAGS_GENERAL= \
!if "$(MOZ_BITS)"=="32"
    /l 0x409
!else
    /r
!endif

CINCLUDES= \
!if defined (LINK_SEC) && "$(SEC_MODEL)" != "none"
    /I$(MOZ_SRC)\ns\include \
    /I$(MOZ_SRC)\ns\dist\public\nspr \
!endif
    /I$(LDAP_SRC)\ldap\include \
    /I$(LIBLDAP) \
    /I$(LIBLBER) \
    /I$(LDAP_SRC)\ldap\libraries\msdos

RCINCLUDES=$(LDAP_SRC)\ldap\include

CDEFINES= \
        /D_WINDOWS /DWINSOCK \
        /DTEMPLATEFILE=\"ldaptemplate.conf\" \
        /DFILTERFILE=\"ldapfilter.conf\" \
!if defined(MOZ_DEBUG)
        /DLDAP_DEBUG \
!endif
        /DNEEDPROTOS \
        /DLDBM_USE_DBBTREE \
!if "$(MOZ_BITS)" == "32"
        /D_WIN32 \
        /DWIN32_KERNEL_THREADS \
!else
        /DUSE_DEBUG_WIN \
!endif
        $(SSL_FLAG) \
        $(LINK_SSL_FLAG) \
        /DLDAP_REFERRALS \
        /DNO_USERINTERFACE \
        /DLDAP_SSLIO_HOOKS

LIB_FLAGS= \
!if "$(MOZ_BITS)"=="32"
        /out:"$(STATICLIB)"
!else
        $(STATICLIB)
!endif

RCDEFINES= 

CFILEFLAGS=$(CFLAGS_GENERAL) ^
    $(CDEFINES) ^
    $(CINCLUDES) ^

RCFILEFLAGS=$(RCFLAGS_GENERAL)\
    $(RCFLAGS_DEBUG)\
    $(RCDEFINES)


!ifdef DEPEND

#==============================================================================
#
# Build dependencies step
#
#==============================================================================

all: \
!if "$(MOZ_BITS)"=="16"
win16suxrox \
!endif
"$(OUTDIR)" $(OUTDIR)\nsldap.dep $(VERFILE)

!if "$(MOZ_BITS)"=="16"
# Copy long-named files into 8.3 since NT 3.51 seems to require it. 
# Win95 and NT 4.0 seem to tolerate the long names
win16suxrox : \
        $(LIBLDAP)\countval.c \
        $(LIBLDAP)\freevalu.c \
        $(LIBLDAP)\getdxbyn.c \
        $(LIBLDAP)\getfilte.c \
        $(LIBLDAP)\getoptio.c \
        $(LIBLDAP)\getvalue.c \
        $(LIBLDAP)\setoptio.c \
        $(LIBLDAP)\vlistctr.c \
        $(LIBLDAP)\proxyaut.c \

$(LIBLDAP)\countval.c : $(LIBLDAP)\countvalues.c
        copy $(LIBLDAP)\countvalues.c $(LIBLDAP)\countval.c

$(LIBLDAP)\freevalu.c : $(LIBLDAP)\freevalues.c
        copy $(LIBLDAP)\freevalues.c $(LIBLDAP)\freevalu.c

$(LIBLDAP)\getdxbyn.c :
        copy $(LIBLDAP)\getdxbyname.c $(LIBLDAP)\getdxbyn.c

$(LIBLDAP)\getfilte.c : $(LIBLDAP)\getfilter.c
        copy $(LIBLDAP)\getfilter.c $(LIBLDAP)\getfilte.c

$(LIBLDAP)\getoptio.c : $(LIBLDAP)\getoption.c
        copy $(LIBLDAP)\getoption.c $(LIBLDAP)\getoptio.c

$(LIBLDAP)\getvalue.c : $(LIBLDAP)\getvalues.c
        copy $(LIBLDAP)\getvalues.c $(LIBLDAP)\getvalue.c

$(LIBLDAP)\setoptio.c : $(LIBLDAP)\setoption.c
        copy $(LIBLDAP)\setoption.c $(LIBLDAP)\setoptio.c

$(LIBLDAP)\vlistctr.c : $(LIBLDAP)\vlistctrl.c
        copy $(LIBLDAP)\vlistctrl.c $(LIBLDAP)\vlistctr.c

$(LIBLDAP)\proxyaut.c : $(LIBLDAP)\proxyauthctrl.c
        copy $(LIBLDAP)\proxyauthctrl.c $(LIBLDAP)\proxyaut.c
!endif

$(OUTDIR)\nsldap.dep: $(BUILDDIR)\\nsldap.mak
        @rem <<$(PROD)$(VERSTR).dep
        $(CINCLUDES) -O $(OUTDIR)\nsldap.dep
<<
	$(MOZ_TOOLS)\makedep @$(PROD)$(VERSTR).dep -F <<
                $(LIBLDAP)\abandon.c
                $(LIBLDAP)\add.c
                $(LIBLDAP)\bind.c
                $(LIBLDAP)\cache.c
                $(LIBLDAP)\charray.c
                $(LIBLDAP)\charset.c
                $(LIBLDAP)\compare.c
		$(LIBLDAP)\control.c
!if "$(MOZ_BITS)"=="32"
                $(LIBLDAP)\countvalues.c
!else
                $(LIBLDAP)\countval.c
!endif
                $(LIBLDAP)\delete.c
                $(LIBLDAP)\disptmpl.c
		$(LIBLDAP)\dllmain.c
                $(LIBLDAP)\dsparse.c
                $(LIBLDAP)\error.c
		$(LIBLDAP)\extendop.c
                $(LIBLDAP)\free.c
!if "$(MOZ_BITS)"=="32"
                $(LIBLDAP)\freevalues.c
!else
                $(LIBLDAP)\freevalu.c
!endif
                $(LIBLDAP)\friendly.c
                $(LIBLDAP)\getattr.c
                $(LIBLDAP)\getdn.c
!if "$(MOZ_BITS)"=="32"
                $(LIBLDAP)\getdxbyname.c
!else
                $(LIBLDAP)\getdxbyn.c
!endif
                $(LIBLDAP)\getentry.c
!if "$(MOZ_BITS)"=="32"
                $(LIBLDAP)\getfilter.c
                $(LIBLDAP)\getoption.c
                $(LIBLDAP)\getvalues.c
!else
                $(LIBLDAP)\getfilte.c
                $(LIBLDAP)\getoptio.c
                $(LIBLDAP)\getvalue.c
!endif
		$(LIBLDAP)\memcache.c
                $(LIBLDAP)\message.c
                $(LIBLDAP)\modify.c
		$(LIBLDAP)\open.c
                $(LIBLDAP)\os-ip.c
		$(LIBLDAP)\psearch.c
		$(LIBLDAP)\referral.c
                $(LIBLDAP)\regex.c
		$(LIBLDAP)\rename.c
                $(LIBLDAP)\request.c
                $(LIBLDAP)\reslist.c
                $(LIBLDAP)\result.c
		$(LIBLDAP)\saslbind.c
                $(LIBLDAP)\sbind.c
                $(LIBLDAP)\search.c
!if "$(MOZ_BITS)"=="32"
                $(LIBLDAP)\setoption.c
!else
                $(LIBLDAP)\setoptio.c
!endif
                $(LIBLDAP)\sort.c
		$(LIBLDAP)\sortctrl.c
                $(LIBLDAP)\srchpref.c
                $(LIBLDAP)\tmplout.c
                $(LIBLDAP)\ufn.c
                $(LIBLDAP)\unbind.c
                $(LIBLDAP)\unescape.c
                $(LIBLDAP)\url.c
		$(LIBLDAP)\utf8.c
!if "$(MOZ_BITS)"=="32"
		$(LIBLDAP)\vlistctrl.c
                $(LIBLDAP)\proxyauthctrl.c
!else
		$(LIBLDAP)\vlistctr.c
                $(LIBLDAP)\proxyaut.c
!endif
		
                $(LIBLBER)\bprint.c
                $(LIBLBER)\decode.c
                $(LIBLBER)\encode.c
                $(LIBLBER)\io.c

                $(BUILDDIR)\mozock.c
!if defined(LINK_SEC)
                $(BUILDDIR)\ssl16.c
!endif

<<

!endif 

!ifdef EXPORT

#==============================================================================
#
# Export to DIST step
#
#==============================================================================

all : $(DIST) $(DIST)\lib $(DIST)\bin $(DIST_PUBLIC) $(DIST_PUBLIC)\ldap install

# Create all the directories we could possibly need

$(DIST_XP) :
        if not exist "$(DIST_XP)\$(NULL)" mkdir "$(DIST_XP)"

$(DIST) : $(DIST_XP)
        if not exist "$(DIST)\$(NULL)" mkdir "$(DIST)"

$(DIST)\lib :
        if not exist "$(DIST)\lib\$(NULL)" mkdir "$(DIST)\lib"

$(DIST)\bin :
        if not exist "$(DIST)\bin\$(NULL)" mkdir "$(DIST)\bin"

$(DIST_PUBLIC) :
        if not exist "$(DIST_PUBLIC)\$(NULL)" mkdir "$(DIST_PUBLIC)"

$(DIST_PUBLIC)\ldap :
        if not exist "$(DIST_PUBLIC)\ldap\$(NULL)" mkdir "$(DIST_PUBLIC)\ldap"


# Copy everything an LDAP client could need up to DIST

install : \
        $(DIST)\bin\nsldap$(DLL_BITS)v$(MOZ_LDAP_VER).dll \
        $(DIST)\lib\nsldap$(DLL_BITS)v$(MOZ_LDAP_VER).lib \
!if "$(MOZ_BITS)"=="32"
# makedep needs to generate syntax for 16-bit lib.exe
        $(DIST)\lib\nsldaps$(DLL_BITS)v$(MOZ_LDAP_VER).lib \
!endif
!if "$(MOZ_BITS)"=="32"
        $(DIST_PUBLIC)\ldap\lber.h \
        $(DIST_PUBLIC)\ldap\ldap.h \
        $(DIST_PUBLIC)\ldap\disptmpl.h \
!else
        $(DIST_PUBLIC)\win16\lber.h \
        $(DIST_PUBLIC)\win16\ldap.h \
        $(DIST_PUBLIC)\win16\disptmpl.h \
!endif

$(DIST)\bin\nsldap$(DLL_BITS)v$(MOZ_LDAP_VER).dll : $(OUTDIR)\nsldap$(DLL_BITS)v$(MOZ_LDAP_VER).dll
        copy $(OUTDIR)\nsldap$(DLL_BITS)v$(MOZ_LDAP_VER).dll $(DIST)\bin\nsldap$(DLL_BITS)v$(MOZ_LDAP_VER).dll

$(DIST)\lib\nsldap$(DLL_BITS)v$(MOZ_LDAP_VER).lib : $(OUTDIR)\nsldap$(DLL_BITS)v$(MOZ_LDAP_VER).lib
        copy $(OUTDIR)\nsldap$(DLL_BITS)v$(MOZ_LDAP_VER).lib $(DIST)\lib\nsldap$(DLL_BITS)v$(MOZ_LDAP_VER).lib

!if "$(MOZ_BITS)"=="32"
# makedep needs to generate syntax for 16-bit lib.exe
$(DIST)\lib\nsldaps$(DLL_BITS)v$(MOZ_LDAP_VER).lib : $(OUTDIR)\nsldaps$(DLL_BITS)v$(MOZ_LDAP_VER).lib
        copy $(OUTDIR)\nsldaps$(DLL_BITS)v$(MOZ_LDAP_VER).lib $(DIST)\lib\nsldaps$(DLL_BITS)v$(MOZ_LDAP_VER).lib
!endif

!if "$(MOZ_BITS)"=="32"
$(DIST_PUBLIC)\ldap\lber.h : $(LDAP_SRC)\ldap\include\lber.h
        copy $(LDAP_SRC)\ldap\include\lber.h $(DIST_PUBLIC)\ldap\lber.h

$(DIST_PUBLIC)\ldap\ldap.h : $(LDAP_SRC)\ldap\include\ldap.h
        copy $(LDAP_SRC)\ldap\include\ldap.h $(DIST_PUBLIC)\ldap\ldap.h

$(DIST_PUBLIC)\ldap\disptmpl.h : $(LDAP_SRC)\ldap\include\disptmpl.h
        copy $(LDAP_SRC)\ldap\include\disptmpl.h $(DIST_PUBLIC)\ldap\disptmpl.h
!else
$(DIST_PUBLIC)\win16\lber.h : $(LDAP_SRC)\ldap\include\lber.h
        copy $(LDAP_SRC)\ldap\include\lber.h $(DIST_PUBLIC)\win16\lber.h

$(DIST_PUBLIC)\win16\ldap.h : $(LDAP_SRC)\ldap\include\ldap.h
        copy $(LDAP_SRC)\ldap\include\ldap.h $(DIST_PUBLIC)\win16\ldap.h

$(DIST_PUBLIC)\win16\disptmpl.h : $(LDAP_SRC)\ldap\include\disptmpl.h
        copy $(LDAP_SRC)\ldap\include\disptmpl.h $(DIST_PUBLIC)\win16\disptmpl.h
!endif

!endif

!if !defined(EXPORT) && !defined(DEPEND)

#==============================================================================
#
# Normal build step
#
#==============================================================================

all : $(OUTDIR)\nsldap.dep "$(OUTDIR)" $(OUTDIR)\nsldap$(DLL_BITS)v$(MOZ_LDAP_VER).dll \
!if "$(MOZ_BITS)"=="32"
# makedep needs to generate syntax for 16-bit lib.exe
$(STATICLIB)
!endif

# Allow makefile to work without dependencies generated.
!if exist("$(OUTDIR)\nsldap.dep")
!include "$(OUTDIR)\nsldap.dep"
!endif

#nuke all the output directories
clobber_all:
    -rd /s /q $(LDAP_OUT)\x86Dbg
    -rd /s /q $(LDAP_OUT)\x86Rel
    -rd /s /q $(LDAP_OUT)\16x86Dbg
    -rd /s /q $(LDAP_OUT)\16x86Rel

!endif

$(OUTDIR) :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# 
# Build static library
# 
static: $(STATICLIB)

$(STATICLIB) : "$(OUTDIR)" $(OBJ_FILES)
!if "$(MOZ_BITS)"=="32"
    $(LIBCMD) @<<
    $(LIB_FLAGS) $(DEF_FLAGS) $(OBJ_FILES)
<<
!else
        del $(STATICLIB)
    $(LIBCMD) $(STATICLIB) \
        +$(OUTDIR)\ABANDON.obj \
        +$(OUTDIR)\ADD.obj \
        +$(OUTDIR)\BIND.obj,,
    $(LIBCMD) $(STATICLIB) \
        +$(OUTDIR)\CACHE.obj \
        +$(OUTDIR)\CHARRAY.obj \
        +$(OUTDIR)\CHARSET.obj,,
    $(LIBCMD) $(STATICLIB) \
        +$(OUTDIR)\COMPARE.obj   \
     +$(OUTDIR)\COUNTVAL.obj   \
     +$(OUTDIR)\DELETE.obj,,
    $(LIBCMD) $(STATICLIB) \
     +$(OUTDIR)\DISPTMPL.obj   \
     +$(OUTDIR)\DSPARSE.obj   \
     +$(OUTDIR)\ERROR.obj,,
    $(LIBCMD) $(STATICLIB) \
     +$(OUTDIR)\FREE.obj   \
     +$(OUTDIR)\FREEVALU.obj   \
     +$(OUTDIR)\FRIENDLY.obj,,
    $(LIBCMD) $(STATICLIB) \
     +$(OUTDIR)\GETATTR.obj   \
     +$(OUTDIR)\GETDN.obj   \
     +$(OUTDIR)\GETDXBYN.obj,,
    $(LIBCMD) $(STATICLIB) \
     +$(OUTDIR)\GETENTRY.obj   \
     +$(OUTDIR)\GETFILTE.obj   \
     +$(OUTDIR)\GETOPTIO.obj,,
    $(LIBCMD) $(STATICLIB) \
     +$(OUTDIR)\GETVALUE.obj   \
     +$(OUTDIR)\MESSAGE.obj   \
     +$(OUTDIR)\MODIFY.obj,,
    copy $(OUTDIR)\OS-IP.obj $(OUTDIR)\OSIP.obj
    $(LIBCMD) $(STATICLIB) \
     +$(OUTDIR)\MODRDN.obj   \
     +$(OUTDIR)\OPEN.obj   \
     +$(OUTDIR)\OSIP.obj,,
        del $(OUTDIR)\OSIP.obj
    $(LIBCMD) $(STATICLIB) \
     +$(OUTDIR)\REGEX.obj   \
     +$(OUTDIR)\REQUEST.obj   \
     +$(OUTDIR)\RESLIST.obj,,
    $(LIBCMD) $(STATICLIB) \
     +$(OUTDIR)\RESULT.obj   \
     +$(OUTDIR)\SBIND.obj   \
     +$(OUTDIR)\SEARCH.obj,,
    $(LIBCMD) $(STATICLIB) \
     +$(OUTDIR)\SETOPTIO.obj   \
     +$(OUTDIR)\SORT.obj   \
     +$(OUTDIR)\SRCHPREF.obj,,
    $(LIBCMD) $(STATICLIB) \
     +$(OUTDIR)\TMPLOUT.obj   \
     +$(OUTDIR)\UFN.obj   \
     +$(OUTDIR)\UNESCAPE.obj,,
    $(LIBCMD) $(STATICLIB) \
     +$(OUTDIR)\UNBIND.obj \
     +$(OUTDIR)\URL.obj   \
     +$(OUTDIR)\UTF8.obj   \
     +$(OUTDIR)\VLISTCTR.obj   \
     +$(OUTDIR)\PROXYAUT.obj,,,
    $(LIBCMD) $(STATICLIB) \
     +$(OUTDIR)\BPRINT.obj   \
     +$(OUTDIR)\DECODE.obj,,
    $(LIBCMD) $(STATICLIB) \
     +$(OUTDIR)\ENCODE.obj   \
     +$(OUTDIR)\IO.obj   \
     +$(OUTDIR)\WSA.obj,,
!endif

#
"$(OUTDIR)\nsldap$(DLL_BITS)v$(MOZ_LDAP_VER).dll" : "$(OUTDIR)" $(OBJ_FILES) $(OUTDIR)\nsldap.res
   @rem <<$(PROD)$(VERSTR).lk
!if "$(MOZ_BITS)"=="32"
    $(LINK_FLAGS) $(LINK_OBJS)
!else
    $(LINK_FLAGS) +
        $(OUTDIR)\ABANDON.obj +
        $(OUTDIR)\ADD.obj +
        $(OUTDIR)\BIND.obj +
        $(OUTDIR)\CACHE.obj +
        $(OUTDIR)\CHARRAY.obj +
        $(OUTDIR)\CHARSET.obj +
        $(OUTDIR)\COMPARE.obj +
    $(OUTDIR)\COUNTVAL.obj +
    $(OUTDIR)\DELETE.obj +
    $(OUTDIR)\DISPTMPL.obj +
    $(OUTDIR)\DSPARSE.obj +
    $(OUTDIR)\ERROR.obj +
    $(OUTDIR)\FREE.obj +
    $(OUTDIR)\FREEVALU.obj +
    $(OUTDIR)\FRIENDLY.obj +
    $(OUTDIR)\GETATTR.obj +
    $(OUTDIR)\GETDN.obj +
    $(OUTDIR)\GETDXBYN.obj +
    $(OUTDIR)\GETENTRY.obj +
    $(OUTDIR)\GETFILTE.obj +
    $(OUTDIR)\GETOPTIO.obj +
    $(OUTDIR)\GETVALUE.obj +
    $(OUTDIR)\MESSAGE.obj +
    $(OUTDIR)\MODIFY.obj +
    $(OUTDIR)\MODRDN.obj +
    $(OUTDIR)\MOZOCK.obj +
    $(OUTDIR)\OPEN.obj +
    $(OUTDIR)\OS-IP.obj +
    $(OUTDIR)\REGEX.obj +
    $(OUTDIR)\REQUEST.obj +
    $(OUTDIR)\RESLIST.obj +
    $(OUTDIR)\RESULT.obj +
    $(OUTDIR)\SBIND.obj +
    $(OUTDIR)\SEARCH.obj +
    $(OUTDIR)\SETOPTIO.obj +
    $(OUTDIR)\SORT.obj +
    $(OUTDIR)\SRCHPREF.obj +
    $(OUTDIR)\TMPLOUT.obj +
    $(OUTDIR)\UFN.obj +
    $(OUTDIR)\UNBIND.obj +
    $(OUTDIR)\UNESCAPE.obj +
    $(OUTDIR)\URL.obj +
    $(OUTDIR)\UTF8.OBJ +
    $(OUTDIR)\VLISTCTRL.OBJ +
    $(OUTDIR)\PROXYAUTHCTRL.OBJ +
    $(OUTDIR)\VLISTC.obj +

    $(OUTDIR)\BPRINT.obj +
    $(OUTDIR)\DECODE.obj +
    $(OUTDIR)\ENCODE.obj +
    $(OUTDIR)\IO.obj +
!if defined(LINK_SEC)
        $(OUTDIR)\ssl16.obj +
        $(SECDIR)\allxpstr.obj +
        $(SECDIR)\db.obj +
        $(SECDIR)\hash.obj +
        $(SECDIR)\hash_buf.obj +
        $(SECDIR)\hsearch.obj +
        $(SECDIR)\h_bigkey.obj +
        $(SECDIR)\h_func.obj +
        $(SECDIR)\h_log2.obj +
        $(SECDIR)\h_page.obj +
        $(SECDIR)\memmove.obj +
        $(SECDIR)\mktemp.obj +
        $(SECDIR)\ndbm.obj +
        $(SECDIR)\snprintf.obj +
        $(SECDIR)\strerror.obj +
        $(SECDIR)\nsres.obj +
        $(SECDIR)\xp_error.obj +
        $(SECDIR)\xpassert.obj +
        $(SECDIR)\xp_reg.obj +
        $(SECDIR)\xp_str.obj +
        $(SECDIR)\xp_trace.obj +
!endif
    $(OUTDIR)\WSA.obj
    $(OUTDIR)\nsldap$(DLL_BITS)v$(MOZ_LDAP_VER).dll
    $(OUTDIR)\nsldap$(DLL_BITS)v$(MOZ_LDAP_VER).map
    c:\msvc\lib\ + 
!if defined(LINK_SEC)
    $(SECLIB) +
    $(SECSUPPORT1) +
    $(SECSUPPORT2) +
!endif
    oldnames.lib libw.lib ldllcew.lib ver.lib 
    .\libldap.def;
!endif
<<
   $(LINK) @$(PROD)$(VERSTR).lk
!if "$(MOZ_BITS)"=="16"
    $(RSC) /K $(OUTDIR)\nsldap.res $(OUTDIR)\nsldap$(DLL_BITS)v$(MOZ_LDAP_VER).dll
!if "$(LINK)"=="link"
        implib /nowep /noi $(OUTDIR)\nsldap$(DLL_BITS)v$(MOZ_LDAP_VER).lib libldap.def
!endif
!endif

#    $(OUTDIR)\*.obj 

#
# Build rules
#

{$(LIBLDAP)}.c{$(OUTDIR)}.obj:
   @rem <<$(PROD)$(VERSTR).cl
 $(CFILEFLAGS)
 $(CFLAGS_LIBLDAP_C) 
<<
   $(CPP) @$(PROD)$(VERSTR).cl %s

{$(LIBLBER)}.c{$(OUTDIR)}.obj:
   @rem <<$(PROD)$(VERSTR).cl
 $(CFILEFLAGS)
 $(CFLAGS_LIBLBER_C) 
<<
   $(CPP) @$(PROD)$(VERSTR).cl %s

{$(BUILDDIR)}.c{$(OUTDIR)}.obj:
   @rem <<$(PROD)$(VERSTR).cl
 $(CFILEFLAGS)
 $(CFLAGS_WINSOCK_C) 
<<
   $(CPP) @$(PROD)$(VERSTR).cl %s

{$(LIBUTIL)}.c{$(OUTDIR)}.obj:
   @rem <<$(PROD)$(VERSTR).cl
 $(CFILEFLAGS)
 $(CFLAGS_LIBUTIL_C) 
<<
   $(CPP) @$(PROD)$(VERSTR).cl %s


RES_FILES = \
!if "$(MOZ_BITS)"=="32"
        $(BUILDDIR)\nsldap.rc
!else
        .\nsldap.rc
!endif

$(OUTDIR)\nsldap.res : $(RES_FILES) "$(OUTDIR)"
    @SET SAVEINCLUDE=%%INCLUDE%%
    @SET INCLUDE=$(RCINCLUDES);$(RCDISTINCLUDES);%%SAVEINCLUDE%%
    $(RSC) /Fo$(PROD)$(VERSTR).res $(RCFILEFLAGS) $(RCFLAGS) $(RES_FILES)
    @IF EXIST $(PROD)$(VERSTR).res copy $(PROD)$(VERSTR).res $(OUTDIR)\nsldap.res 
    @IF EXIST $(PROD)$(VERSTR).res del $(PROD)$(VERSTR).res
    @SET INCLUDE=%%SAVEINCLUDE%%
    @SET SAVEINCLUDE=

!if "$(MOZ_BITS)"=="32"
$(VERFILE) : $(VERPROG)
        $(VERPROG) $(DIRSDK_VERSION) UseSystemDate $@

$(VERPROG) : $(VERSRC)
        cl $(VERSRC) -link -out:$@
!endif
