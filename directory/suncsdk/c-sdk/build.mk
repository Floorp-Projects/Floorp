# 
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#  
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#  
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation. Portions created by Netscape are
# Copyright (C) 1998-1999 Netscape Communications Corporation. All
# Rights Reserved.
# 
# Contributor(s): 
#

COMPVERSIONDIR	= $(DEPTH)/directory/c-sdk

DEFAULT_VENDOR_NAME="Sun Microsystems Inc."
DEFAULT_VENDOR_VERSION=600

LDAPVERS        = 60
LDAPVERS_SUFFIX = 6.0

ifndef VENDOR_NAME
VENDOR_NAME	= $(DEFAULT_VENDOR_NAME)
endif

ifndef VENDOR_VERSION
VENDOR_VERSION = $(DEFAULT_VENDOR_VERSION)
endif

__BUILD_MARKER	= "\"$(VENDOR_VERSION) $(OS_ARCH)$(OS_RELEASE) \
			$(USER) $(BUILD_NOTE)\""
DEFINES		+= -D__BUILD_MARKER=$(__BUILD_MARKER)

ifeq ($(OS_ARCH), WINNT)
	COMPONENT_PULL_METHOD=FTP
endif

ifdef HAVE_CCONF
# component tags for internal build only
include	$(COMPVERSIONDIR)/component_versions.mk
endif

ifeq ($(DEBUG), full)
	DBG_OR_OPT = DBG
else
	DBG_OR_OPT = OPT
endif

# Ldap library
ifeq ($(OS_ARCH), WINNT)
LDAP_LIBNAME	= nsldap32v$(LDAPVERS)
else
LDAP_LIBNAME	= ldap$(LDAPVERS)
endif
DIR_VERSION     = $(LDAPVERS_SUFFIX)
DIRSDK_VERSION  = $(LDAPVERS_SUFFIX)

# PrLdap library
ifeq ($(OS_ARCH), WINNT)
PRLDAP_LIBNAME	= nsldappr32v$(PRLDAPVERS)
else
PRLDAP_LIBNAME	= prldap$(PRLDAPVERS)
endif

# lber library
ifeq ($(OS_ARCH), WINNT)
LBER_LIBNAME	= nslber32v$(LBERVERS)
else
LBER_LIBNAME	= lber$(LBERVERS)
endif

# ldif library
ifeq ($(OS_ARCH), WINNT)
LDIF_LIBNAME	= nsldif32v$(LDIFVERS)
else
LDIF_LIBNAME	= ldif$(LDIFVERS)
endif

# iutil library
ifeq ($(OS_ARCH), WINNT)
IUTIL_LIBNAME	= nsiutil32v$(IUTILVERS)
else
IUTIL_LIBNAME	= iutil$(IUTILVERS)
endif

# util library
ifeq ($(OS_ARCH), WINNT)
UTIL_LIBNAME	= nsutil32v$(UTILVERS)
else
UTIL_LIBNAME	= util$(UTILVERS)
endif

# ssl library
ifeq ($(OS_ARCH), WINNT)
SSLDAP_LIBNAME	= nsldapssl32v$(SSLDAPVERS)
else
SSLDAP_LIBNAME	= ssldap$(SSLDAPVERS)
endif

# nss library
NSS_LIBNAME	= nss$(NSSVERS)
SSL_LIBNAME	= ssl$(NSSVERS)
STKN_LIBNAME    = softokn$(NSSVERS)
HYBRID_LIBNAME	= freebl_hybrid_$(NSSVERS)
PURE32_LIBNAME	= freebl_pure32_$(NSSVERS)
FREEBL_LIBNAME	= freebl*
COPYFREEBL      = 1

ifneq ($(USE_64), 1)
ifeq ($(OS_ARCH), SunOS)
ifneq ($(OS_TEST),i86pc)
COPYFREEBL      = 1
endif
endif
ifeq ($(OS_ARCH), HP-UX)
COPYFREEBL      = 1
endif
endif

# svrcore library
SVRCOREVERS	=
SVRCOREVERS_SUFFIX =
SVRCORE_LIBNAME	= svrcore$(SVRCOREVERS)

#
# NSPR library
#

ifeq ($(OS_TARGET), WIN95)
PLC_BASENAME=plc$(NSPR_LIBVERSION)
PLDS_BASENAME=plds$(NSPR_LIBVERSION)
NSPR_BASENAME=nspr$(NSPR_LIBVERSION)
else
PLC_BASENAME=libplc$(NSPR_LIBVERSION)
PLDS_BASENAME=libplds$(NSPR_LIBVERSION)
NSPR_BASENAME=libnspr$(NSPR_LIBVERSION)
endif

PLCBASE=plc$(NSPR_LIBVERSION)
PLDSBASE=plds$(NSPR_LIBVERSION)
NSPRBASE=nspr$(NSPR_LIBVERSION)

DYNAMICNSPR = -l$(PLCBASE) -l$(PLDSBASE) -l$(NSPRBASE)

PLC_LIBNAME=plc$(NSPR_LIBVERSION)
PLDS_LIBNAME=plds$(NSPR_LIBVERSION)
NSPR_LIBNAME=nspr$(NSPR_LIBVERSION)

#
# SASL library
#
LIBSASL_INCLUDES_LOC = /share/builds/integration/sasl$(SASLVERS)/$(SASL_RELEASE_TAG)/$(OBJDIR_NAME)/include
LIBSASL_LIB_LOC      = /share/builds/integration/sasl$(SASLVERS)/$(SASL_RELEASE_TAG)/$(OBJDIR_NAME)/lib

ifeq ($(HAVE_SASL_LOCAL), 1)
  LIBSASL_INCLUDES  = /usr/include/sasl
  LIBSASL_LIBDIR	=
else
  LIBSASL_INCLUDES =../../../../../dist/public/libsasl
  LIBSASL_LIBDIR	=../../../../../dist/$(OBJDIR_NAME)/libsasl
endif

SASL_LIBNAME=sasl
SASL_BASENAME=sasl32

################################
# LIB ICU (for I18N)           #
################################
# default setting
ICU_COMP_NAME = icu
ICUOBJDIR=$(OBJDIR_NAME)

#ifeq ($(OS_ARCH), SunOS)
#  ifeq ($(OS_TEST),i86pc)
#    ICUOBJDIR = SunOS5.8_x86_$(DBG_OR_OPT).OBJ
#  endif
#endif
# because we don't have a real Win 95 ICU component...
ifeq ($(OS_TARGET), WIN95)
  ICUOBJDIR = WINNT4.0_$(DBG_OR_OPT).OBJ
endif

ifeq ($(OS_ARCH), Linux)
	ifeq ($(USE_64), 1)
		ICUOBJDIR = $(OS_ARCH)$(OS_RELEASE)_64$(OBJDIR_TAG).OBJ
	else
		ICUOBJDIR = $(OS_ARCH)$(OS_RELEASE)$(OBJDIR_TAG).OBJ
	endif
endif

ifeq ($(OS_ARCH), AIX)
	ICU_VERS_NUM	= 2.1
	ICU_LIBVERSION	= 2.1.6
	LIBICU_RELDATE	= 20040126_21.1
	ICU_RELDATE	= 20040126_21.1
else
	ICU_VERS_NUM	= 3.2
	ICU_LIBVERSION	= 3.2
	LIBICU_RELDATE	= 20051214
	ICU_RELDATE	= 20051214
endif

ICU_VERSION = $(ICU_RELDATE)
ICU_COMP_DIR = lib$(ICU_COMP_NAME)$(ICU_VERS_NUM)
ICU_INT=
ifeq ($(ICU_INT), 1)
  LIBICU_INCLUDES_LOC = /share/builds/components/icu/$(ICU_LIBVERSION)/$(ICU_RELDATE)/$(ICUOBJDIR)/include
  LIBICU_LIB_LOC      = /share/builds/components/icu/$(ICU_LIBVERSION)/$(ICU_RELDATE)/$(ICUOBJDIR)/lib
else
  LIBICU_INCLUDES_LOC = /share/builds/integration/icu/$(ICU_LIBVERSION)/$(ICU_RELDATE)/$(ICUOBJDIR)/include
  LIBICU_LIB_LOC      = /share/builds/integration/icu/$(ICU_LIBVERSION)/$(ICU_RELDATE)/$(ICUOBJDIR)/lib
endif

ifneq ($(HAVE_LIBICU_LOCAL), 1)
   LIBICU_DIR	    = ../../../../../dist/libicu$(ICU_LIBVERSION)
   LIBICU_INCLUDES =../../../../../dist/public/libicu
   LIBICU_LIBDIR	=../../../../../dist/$(OBJDIR_NAME)/libicu
else
   LIBICU_DIR	    =
   LIBICU_INCLUDES  = /usr/include
   LIBICU_LIBDIR	=
endif

ICU_LIBPATH = $(LIBICU_LIBDIR)
ICU_INCLUDE = $(LIBICU_INCLUDEDIR)

ifeq ($(OS_ARCH), WINNT)
  ICU_RELEASE = $(COMPONENTS_DIR)/icu/$(ICU_VERSION)/$(ICUOBJDIR)
  ICU_LIBNAMES = icuin icuuc icudt
  ICU_LIBS = $(addsuffix .lib,  $(ICU_LIBNAMES))
  ICUDLL_NAMES = $(addsuffix .dll, $(ICU_LIBNAMES))
  LIBICU = $(addprefix $(ICU_LIBPATH)/, $(ICU_LIBS))
  ICUOBJNAME = $(ICU_LIBNAMES)
else # WINNT
  ICU_LIBNAMES = icudata icui18n icuuc
  ICU_SOLIB_NAMES = $(addsuffix $(DLL_PRESUF), $(ICU_LIBNAMES))
  ICU_LIBS = $(addsuffix .a, $(ICU_SOLIB_NAMES))
  ICU_SOLIBS = $(addsuffix .$(DLL_SUFFIX), $(ICU_SOLIB_NAMES))
  ICUOBJNAME = $(ICU_SOLIBS)
  LIBICU = $(addprefix $(ICU_LIBPATH)/, $(ICU_SOLIBS))
  ICULINK = -L$(ICU_LIBPATH) $(addprefix -l,  $(addsuffix $(DLL_PRESUF), $(ICU_LIBNAMES)))
  ICULINK_STATIC = $(addprefix $(ICU_LIBPATH)/, $(ICU_LIBS))

  ifeq ($(OS_ARCH),SOLARIS)
   ICULINK += -lw
  endif # Solaris
  ifeq ($(OS_ARCH),HPUX)
   #linking with libC is *BAD* on HPUX11
   ICULINK = -L$(ICU_LIBPATH) $(addprefix -l,  $(addsuffix $(DLL_PRESUF), $(ICU_LIBNAMES)))
   ICULINK_STATIC = $(addprefix $(ICU_LIBPATH)/, $(ICU_LIBS))
  endif # HPUX
  ifeq ($(OS_ARCH),Linux)
   ICULINK += -lresolv
   ICULINK_STATIC += -lresolv
  endif # Linux
endif #WINNT


RM              = rm -f
SED             = sed

# uncomment to enable support for LDAP referrals
LDAP_REFERRALS  = -DLDAP_REFERRALS
DEFNETSSL	= -DNET_SSL 
NOLIBLCACHE	= -DNO_LIBLCACHE
NSDOMESTIC	= -DNS_DOMESTIC

#for including SASL options
ifdef HAVE_SASL
HAVESASLOPTIONS = -DLDAP_SASLIO_HOOKS -DHAVE_SASL_OPTIONS -DHAVE_SASL_OPTIONS_2
else
HAVESASLOPTIONS =
endif

ifdef BUILD_OPT
LDAP_DEBUG	=
else
LDAP_DEBUG	= -DLDAP_DEBUG
endif

ifdef HAVE_LIBICU
HAVELIBICU	= -DHAVE_LIBICU
else
HAVELIBICU	=
endif

ifdef BUILD_CLU
BUILDCLU	= 1
else
BUILDCLU	=
endif

#
# DEFS are included in CFLAGS
#
DEFS            = $(PLATFORMCFLAGS) $(LDAP_DEBUG) $(HAVELIBICU) \
                  $(CLDAP) $(DEFNETSSL) $(NOLIBLCACHE) \
                  $(LDAP_REFERRALS) $(LDAP_DNS) $(STR_TRANSLATION) \
                  $(LIBLDAP_CHARSETS) $(LIBLDAP_DEF_CHARSET) \
		  $(NSDOMESTIC) $(LDAPSSLIO) $(HAVESASLOPTIONS)


ifeq ($(OS_ARCH), WINNT)
DIRVER_PROG=$(COMMON_OBJDIR)/dirver.exe
else
DIRVER_PROG=$(COMMON_OBJDIR)/dirver
endif

ifeq ($(OS_ARCH), WINNT)
EXE_SUFFIX=.exe
RSC=rc
OFFLAG=/Fo
else
OFFLAG=-o
endif

ifeq ($(OS_ARCH), Linux)
DEFS            += -DLINUX2_0 -DLINUX1_2 -DLINUX2_1
endif

ifeq ($(OS_ARCH), WINNT)
DLLEXPORTS_PREFIX=/DEF:
USE_DLL_EXPORTS_FILE	= 1
endif

ifeq ($(OS_ARCH), SunOS)
DLLEXPORTS_PREFIX=-Blocal -M
USE_DLL_EXPORTS_FILE	= 1
endif

ifeq ($(OS_ARCH), IRIX)
DLLEXPORTS_PREFIX=-exports_file
USE_DLL_EXPORTS_FILE	= 1
endif

ifeq ($(OS_ARCH), HP-UX)
DEFS		+= -Dhpux -D_REENTRANT
endif

ifeq ($(OS_ARCH),AIX)
DLLEXPORTS_PREFIX=-bE:
DL=-ldl
USE_DLL_EXPORTS_FILE	= 1
endif

ifeq ($(OS_ARCH),OSF1)
DEFS		+= -DOSF1V4
DL=
endif

ifeq ($(OS_ARCH),ReliantUNIX)
DL=-ldl
endif

ifeq ($(OS_ARCH),UnixWare)
DL=
endif

RPATHFLAG = ..:../lib:../../lib:../../../lib:../../../../lib:../lib-private

ifeq ($(OS_ARCH), SunOS)
# flag to pass to cc when linking to set runtime shared library search path
# this is used like this, for example:   $(RPATHFLAG_PREFIX)../..
RPATHFLAG_PREFIX=-Wl,-R,

# flag to pass to ld when linking to set runtime shared library search path
# this is used like this, for example:   $(LDRPATHFLAG_PREFIX)../..
LDRPATHFLAG_PREFIX=-R

# OS network libraries
PLATFORMLIBS+=-lresolv -lsocket -lnsl -lgen -ldl -lposix4
endif

ifeq ($(OS_ARCH), OSF1)
# flag to pass to cc when linking to set runtime shared library search path
# this is used like this, for example:   $(RPATHFLAG_PREFIX)../..
RPATHFLAG_PREFIX=-Wl,-rpath,

# flag to pass to ld when linking to set runtime shared library search path
# this is used like this, for example:   $(LDRPATHFLAG_PREFIX)../..
LDRPATHFLAG_PREFIX=-rpath

# allow for unresolved symbols
DLL_LDFLAGS += -expect_unresolved "*"
endif # OSF1

ifeq ($(OS_ARCH), AIX)
# Flags to set runtime shared library search path.  For example:
# $(CC) $(RPATHFLAG_PREFIX)../..$(RPATHFLAG_EXTRAS)
RPATHFLAG_PREFIX=-blibpath:
RPATHFLAG_EXTRAS=:/usr/lib:/lib

# flag to pass to ld when linking to set runtime shared library search path
# this is used like this, for example:   $(LDRPATHFLAG_PREFIX)../..
LDRPATHFLAG_PREFIX=-blibpath:/usr/lib:/lib:
DLL_LDFLAGS= -bM:SRE -bnoentry \
    -L.:/usr/lib/threads:/usr/lpp/xlC/lib:/usr/lib:/lib
DLL_EXTRA_LIBS= -bI:/usr/lib/lowsys.exp -lC_r -lC -lpthreads -lc_r -lm \
    /usr/lib/libc.a

EXE_EXTRA_LIBS= -bI:/usr/lib/syscalls.exp -lsvld -lpthreads
endif # AIX

ifeq ($(OS_ARCH), HP-UX)
# flag to pass to cc when linking to set runtime shared library search path
# this is used like this, for example:   $(RPATHFLAG_PREFIX)../..
RPATHFLAG_PREFIX=-Wl,+s,+b,

# flag to pass to ld when linking to set runtime shared library search path
# this is used like this, for example:   $(LDRPATHFLAG_PREFIX)../..
LDRPATHFLAG_PREFIX=+s +b

# we need to link in the rt library to get sem_*()
PLATFORMLIBS += -lrt
PLATFORMCFLAGS= 

endif # HP-UX

ifeq ($(OS_ARCH), Linux)
# flag to pass to cc when linking to set runtime shared library search path
# this is used like this, for example:   $(RPATHFLAG_PREFIX)../..
RPATHFLAG_PREFIX=-Wl,-rpath,

# flag to pass to ld when linking to set runtime shared library search path
# this is used like this, for example:   $(LDRPATHFLAG_PREFIX)../..
# note, there is a trailing space
LDRPATHFLAG_PREFIX=-rpath
endif # Linux

#
# XXX: does anyone know of a better way to solve the "LINK_LIB2" problem? -mcs
#
# Link to produce a console/windows exe on Windows
#

ifeq ($(OS_ARCH), WINNT)

DEBUG_LINK_OPT=/DEBUG:FULL
ifeq ($(BUILD_OPT), 1)
  DEBUG_LINK_OPT=
endif

SUBSYSTEM=CONSOLE
LINK_EXE        = link $(DEBUG_LINK_OPT) -OUT:"$@" /MAP $(ALDFLAGS) $(LDFLAGS) $(ML_DEBUG) \
    $(LCFLAGS) /NOLOGO /PDB:NONE /DEBUGTYPE:BOTH /INCREMENTAL:NO \
    /NODEFAULTLIB:MSVCRTD /SUBSYSTEM:$(SUBSYSTEM) $(DEPLIBS) \
    $(EXTRA_LIBS) $(PLATFORMLIBS) $(OBJS)
LINK_LIB        = lib -OUT:"$@"  $(OBJS)
LINK_DLL        = link $(DEBUG_LINK_OPT) /nologo /MAP /DLL /PDB:NONE /DEBUGTYPE:BOTH \
        $(ML_DEBUG) /SUBSYSTEM:$(SUBSYSTEM) $(LLFLAGS) $(DLL_LDFLAGS) \
        $(EXTRA_LIBS) /out:"$@" $(OBJS)
else # WINNT
#
# UNIX link commands
#
LINK_LIB        = $(RM) $@; $(AR) $(OBJS); $(RANLIB) $@
LINK_LIB2       = $(RM) $@; $(AR) $@ $(OBJS2); $(RANLIB) $@
ifdef SONAMEFLAG_PREFIX
LINK_DLL        = $(LD) $(DSO_LDOPTS) $(RPATHFLAG_PREFIX)$(RPATHFLAG) $(ALDFLAGS) $(DLL_LDFLAGS) \
						$(DLL_EXPORT_FLAGS) -o $@ $(SONAMEFLAG_PREFIX)$(notdir $@) $(OBJS)
else # SONAMEFLAG_PREFIX
LINK_DLL        = $(LD) $(RPATHFLAG_PREFIX)$(RPATHFLAG) $(ALDFLAGS) $(DLL_LDFLAGS) $(DLL_EXPORT_FLAGS) \
                        -o $@ $(OBJS)
endif # SONAMEFLAG_PREFIX

ifeq ($(OS_ARCH), OSF1)
# The linker on OSF/1 gets confused if it finds an so_locations file
# that doesn't meet its expectations, so we arrange to remove it before
# linking.
SO_FILES_TO_REMOVE=so_locations
endif

ifeq ($(OS_ARCH), HP-UX)
# On HPUX, we need a couple of changes:
# 1) Use the C++ compiler for linking, which will pass the +eh flag on down to the
#    linker so the correct exception-handling-aware libC gets used (libnshttpd.sl
#    needs this).
# 2) Add a "-Wl,-E" option so the linker gets a "-E" flag.  This makes symbols
#    in an executable visible to shared libraries loaded at runtime.
LINK_EXE        = $(CCC) -AA -Wl,-E $(ALDFLAGS) $(LDFLAGS) $(RPATHFLAG_PREFIX)$(RPATHFLAG) -o $@ $(OBJS) $(EXTRA_LIBS) $(PLATFORMLIBS)

ifeq ($(USE_64), 1)
LINK_EXE        = $(CCC) -AA -DHPUX_ACC -D__STDC_EXT__ -D_POSIX_C_SOURCE=199506L  +DA2.0W +DS2.0 -Wl,-E $(ALDFLAGS) $(LDFLAGS) $(RPATHFLAG_PREFIX)$(RPATHFLAG) -o $@ $(OBJS) $(EXTRA_LIBS) $(PLATFORMLIBS)
endif

else # HP-UX
# everything except HPUX
ifeq ($(OS_ARCH), ReliantUNIX)
# Use the C++ compiler for linking if at least ONE object is C++
export LD_RUN_PATH=$(RPATHFLAG)
LINK_EXE      = $(CXX) $(ALDFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(EXTRA_LIBS) $(PLATFORMLIBS)

else # ReliantUNIX
ifdef USE_LD_RUN_PATH
#does RPATH differently.  instead we export RPATHFLAG as LD_RUN_PATH
#see ns/netsite/ldap/clients/tools/Makefile for an example
export LD_RUN_PATH=$(RPATHFLAG)
LINK_EXE        = $(CC) $(ALDFLAGS) $(LDFLAGS) \
                        -o $@ $(OBJS) $(EXTRA_LIBS) $(PLATFORMLIBS)
LINK_EXE_NOLIBSOBJS     =  $(CC) $(ALDFLAGS) $(LDFLAGS) -o $@
else # USE_LD_RUN_PATH
LINK_EXE        = $(CC) $(ALDFLAGS) $(LDFLAGS) \
                        $(RPATHFLAG_PREFIX)$(RPATHFLAG)$(RPATHFLAG_EXTRAS) \
                        -o $@ $(OBJS) $(EXTRA_LIBS) $(PLATFORMLIBS)
LINK_EXE_NOLIBSOBJS     = $(CC) $(ALDFLAGS) $(LDFLAGS) \
                        $(RPATHFLAG_PREFIX)$(RPATHFLAG)$(RPATHFLAG_EXTRAS) -o $@
endif # USE_LD_RUN_PATH
endif # ReliantUNIX
endif # HP-UX
endif # WINNT

ifeq ($(OS_ARCH), OSF1)
LINK_EXE        = $(CCC) $(ALDFLAGS) $(LDFLAGS) $(RPATHFLAG_PREFIX)$(RPATHFLAG) \
        -o $@ $(OBJS) $(EXTRA_LIBS) $(PLATFORMLIBS)
endif

ifeq ($(OS_ARCH), SunOS)
ifeq ($(USE_64), 1)
LINK_EXE        = $(CCC) $(ALDFLAGS) $(LDFLAGS)  -R:$(RPATHFLAG)\
        -o $@ $(OBJS) $(EXTRA_LIBS) $(PLATFORMLIBS)
endif
endif


PERL = perl
#
# shared library symbol export definitions
#
ifeq ($(OS_ARCH), WINNT)
GENEXPORTS=cmd /c  $(PERL) $(LDAP_SRC)/build/genexports.pl
else
GENEXPORTS=$(PERL) $(LDAP_SRC)/build/genexports.pl
endif

