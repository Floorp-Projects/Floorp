# 
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
# 
# The contents of this file are subject to the Mozilla Public License Version 
# 1.1 (the "License"); you may not use this file except in compliance with 
# the License. You may obtain a copy of the License at 
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
# 
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
# 
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998-1999
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
# 
# ***** END LICENSE BLOCK ***** 

# some vendors may wish to override COMPVERSIONDIR from the command-line
#
ifdef USE_AUTOCONF
COMPVERSIONDIR = $(topsrcdir)
else
COMPVERSIONDIR = $(DEPTH)/directory/c-sdk
endif

DEFAULT_VENDOR_NAME=mozilla.org
DEFAULT_VENDOR_VERSION=500

ifndef VENDOR_NAME
VENDOR_NAME	= $(DEFAULT_VENDOR_NAME)
endif

ifndef VENDOR_VERSION
VENDOR_VERSION = $(DEFAULT_VENDOR_VERSION)
endif

ifeq ($(OS_ARCH), WINNT)
	COMPONENT_PULL_METHOD=FTP
endif

# component tags for internal build only
include $(COMPVERSIONDIR)/component_versions.mk

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
ifeq ($(NSS_DYNAMIC_SOFTOKN),1)
SOFTOKN_LIBNAME	= softokn$(NSSVERS)
endif
SSL_LIBNAME	= ssl$(NSSVERS)
HYBRID_LIBNAME	= freebl_hybrid_$(NSSVERS)
PURE32_LIBNAME	= freebl_pure32_$(NSSVERS)

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
# NLS library
#
ifeq ($(OS_ARCH), WINNT)
NSCNV_LIBNAME	=nscnv32$(NLS_LIBVERSION).$(LIB_SUFFIX)
NSJPN_LIBNAME	=nsjpn32$(NLS_LIBVERSION).$(LIB_SUFFIX)
NSCCK_LIBNAME	=nscck32$(NLS_LIBVERSION).$(LIB_SUFFIX)
NSSB_LIBNAME	=nssb32$(NLS_LIBVERSION).$(LIB_SUFFIX)
else
NSCNV_LIBNAME	=libnscnv$(NLS_LIBVERSION).$(LIB_SUFFIX)
NSJPN_LIBNAME	=libnsjpn$(NLS_LIBVERSION).$(LIB_SUFFIX)
NSCCK_LIBNAME	=libnscck$(NLS_LIBVERSION).$(LIB_SUFFIX)
NSSB_LIBNAME	=libnssb$(NLS_LIBVERSION).$(LIB_SUFFIX)
endif

LIBNLS_INCLUDES_LOC = $(LIBNLS_RELEASE_TREE)/libnls$(NLS_LIBVERSION)/$(LIBNLS_RELDATE)/$(OBJDIR_NAME)/include
LIBNLS_LIB_LOC	    = $(LIBNLS_RELEASE_TREE)/libnls$(NLS_LIBVERSION)/$(LIBNLS_RELDATE)/$(OBJDIR_NAME)/lib

LIBNLS_DIR	    = ../../../../../dist/libnls$(NLS_LIBVERSION)
ifeq ($(COMPONENT_PULL_METHOD), FTP)
LIBNLS_INCLUDES =../../../../../dist/libnls$(NLS_LIBVERSION)/$(OBJDIR_NAME)/include
LIBNLS_LIBDIR	=../../../../../dist/libnls$(NLS_LIBVERSION)/$(OBJDIR_NAME)/lib
else
LIBNLS_INCLUDES =../../../../../dist/public/libnls
LIBNLS_LIBDIR	=../../../../../dist/$(OBJDIR_NAME)/libnls
endif

RM              = rm -f
SED             = sed

# uncomment to enable support for LDAP referrals
LDAP_REFERRALS  = -DLDAP_REFERRALS
DEFNETSSL	= -DNET_SSL 
NOLIBLCACHE	= -DNO_LIBLCACHE
NSDOMESTIC	= -DNS_DOMESTIC


ifdef BUILD_OPT
LDAP_DEBUG	=
else
LDAP_DEBUG	= -DLDAP_DEBUG
endif

ifdef HAVE_LIBNLS
HAVELIBNLS	= -DHAVE_LIBNLS
else
HAVELIBNLS	=
endif

ifdef BUILD_CLU
BUILDCLU	= 1
else
BUILDCLU	=
endif

#
# DEFS are included in CFLAGS
#
DEFS            = $(PLATFORMCFLAGS) $(LDAP_DEBUG) $(HAVELIBNLS) \
                  $(CLDAP) $(DEFNETSSL) $(NOLIBLCACHE) \
                  $(LDAP_REFERRALS) $(LDAP_DNS) $(STR_TRANSLATION) \
                  $(LIBLDAP_CHARSETS) $(LIBLDAP_DEF_CHARSET) \
		  $(NSDOMESTIC) $(LDAPSSLIO)


ifeq ($(OS_ARCH), WINNT)
DIRVER_PROG=$(COMMON_OBJDIR)/dirver.exe
else
DIRVER_PROG=$(COMMON_OBJDIR)/dirver
endif

ifeq ($(OS_ARCH), WINNT)
EXE_SUFFIX=.exe
RSC=rc
ifdef NS_USE_GCC
OFFLAG=-o #
else
OFFLAG=/Fo
endif
else
OFFLAG=-o
endif

ifeq ($(OS_ARCH), Linux)
DEFS            += -DLINUX2_0 -DLINUX1_2 -DLINUX2_1
endif

ifeq ($(OS_ARCH), WINNT)
ifndef NS_USE_GCC
DLLEXPORTS_PREFIX=/DEF:
USE_DLL_EXPORTS_FILE	= 1
endif
endif

ifeq ($(OS_ARCH), OS2)
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
USE_LD_RUN_PATH=1
USE_CCC_TO_LINK=1
CCC=$(CXX)
endif

ifeq ($(OS_ARCH),UnixWare)
DL=
endif

RPATHFLAG = ..:../lib:../../lib:../../../lib:../../../../lib

ifeq ($(OS_ARCH), SunOS)
# include $ORIGIN in run time library path (works on Solaris 8 10/01 and later)
RPATHFLAG := \$$ORIGIN/../lib:\$$ORIGIN/../../lib:$(RPATHFLAG)

# flag to pass to cc when linking to set runtime shared library search path
# this is used like this, for example:   $(RPATHFLAG_PREFIX)../..
# Also, use the C++ compiler to link for 64-bit builds.
ifeq ($(USE_64), 1)
USE_CCC_TO_LINK=1
RPATHFLAG_PREFIX=-R:
else
RPATHFLAG_PREFIX=-Wl,-R,
endif

# flag to pass to ld when linking to set runtime shared library search path
# this is used like this, for example:   $(LDRPATHFLAG_PREFIX)../..
LDRPATHFLAG_PREFIX=-R

# OS network libraries
PLATFORMLIBS+=-lresolv -lsocket -lnsl -lgen -ldl -lposix4
endif

ifeq ($(OS_ARCH), OSF1)
# Use the C++ compiler to link
USE_CCC_TO_LINK=1

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
# Use the C++ compiler to link
USE_CCC_TO_LINK=1

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
# Use the C++ compiler to link
USE_CCC_TO_LINK=1

# flag to pass to cc when linking to set runtime shared library search path
# this is used like this, for example:   $(RPATHFLAG_PREFIX)../..
RPATHFLAG_PREFIX=-Wl,-rpath,

# flag to pass to ld when linking to set runtime shared library search path
# this is used like this, for example:   $(LDRPATHFLAG_PREFIX)../..
# note, there is a trailing space
LDRPATHFLAG_PREFIX=-rpath
endif # Linux


# Use the C++ compiler to link... or not.
ifdef USE_CCC_TO_LINK
CC_FOR_LINK=$(CCC)
else
CC_FOR_LINK=$(CC)
endif


#
# XXX: does anyone know of a better way to solve the "LINK_LIB2" problem? -mcs
#
# Link to produce a console/windows exe on Windows
#

ifeq ($(OS_ARCH), WINNT)

ifdef NS_USE_GCC
LINK_EXE	= $(CC_FOR_LINK) -o $@ $(LDFLAGS) $(LCFLAGS) $(DEPLIBS) $(OBJS) $(EXTRA_LIBS) $(PLATFORMLIBS)
LINK_LIB	= $(AR) cr $@ $(OBJS)
LINK_DLL	= $(CC_FOR_LINK) -shared -Wl,--export-all-symbols -Wl,--out-implib -Wl,$(@:.$(DLL_SUFFIX)=.$(LIB_SUFFIX)) $(LLFLAGS) $(DLL_LDFLAGS) -o $@ $(OBJS) $(EXTRA_LIBS) $(EXTRA_DLL_LIBS)
else
DEBUG_LINK_OPT=/DEBUG
ifeq ($(BUILD_OPT), 1)
  ifndef MOZ_DEBUG_SYMBOLS
    DEBUG_LINK_OPT=
  endif
  DEBUG_LINK_OPT += -OPT:REF
endif

SUBSYSTEM=CONSOLE
ifndef MOZ_DEBUG_SYMBOLS
DEBUG_FLAGS=/PDB:NONE /DEBUGTYPE:BOTH
endif

LINK_EXE        = $(CYGWIN_WRAPPER) link $(DEBUG_LINK_OPT) -OUT:"$@" /MAP $(ALDFLAGS) $(LDFLAGS) $(ML_DEBUG) \
    $(LCFLAGS) /NOLOGO $(DEBUG_FLAGS) /INCREMENTAL:NO \
    /NODEFAULTLIB:MSVCRTD /SUBSYSTEM:$(SUBSYSTEM) $(DEPLIBS) \
    $(EXTRA_LIBS) $(PLATFORMLIBS) $(OBJS)
LINK_LIB        = $(CYGWIN_WRAPPER) lib -OUT:"$@"  $(OBJS)
LINK_DLL        = $(CYGWIN_WRAPPER) link $(DEBUG_LINK_OPT) /nologo /MAP /DLL $(DEBUG_FLAGS) \
        $(ML_DEBUG) /SUBSYSTEM:$(SUBSYSTEM) $(LLFLAGS) $(DLL_LDFLAGS) \
        $(EXTRA_LIBS) /out:"$@" $(OBJS)
endif # NS_USE_GCC
else # WINNT
#
# UNIX link commands
#
ifeq ($(OS_ARCH),OS2)
LINK_LIB        = -$(RM) $@ && $(AR) $(AR_FLAGS) $(OBJS) && $(RANLIB) $@
LINK_LIB2       = -$(RM) $@ && $(AR) $@ $(OBJS2) && $(RANLIB) $@
ifeq ($(MOZ_OS2_TOOLS),VACPP)
LINK_DLL        = $(LD) $(OS_DLLFLAGS) $(DLLFLAGS) $(OBJS)
else
LINK_DLL        = $(LD) $(DSO_LDOPTS) $(ALDFLAGS) $(DLL_LDFLAGS) $(DLL_EXPORT_FLAGS) \
                        -o $@ $(OBJS)
endif

else

LINK_LIB        = $(RM) $@; $(AR) $(AR_FLAGS) $(OBJS); $(RANLIB) $@
LINK_LIB2       = $(RM) $@; $(AR) $@ $(OBJS2); $(RANLIB) $@
ifdef SONAMEFLAG_PREFIX
LINK_DLL        = $(LD) $(DSO_LDOPTS) $(ALDFLAGS) $(DLL_LDFLAGS) $(DLL_EXPORT_FLAGS) \
                        -o $@ $(SONAMEFLAG_PREFIX)$(notdir $@) $(OBJS)
else # SONAMEFLAG_PREFIX
LINK_DLL        = $(LD) $(DSO_LDOPTS) $(ALDFLAGS) $(DLL_LDFLAGS) $(DLL_EXPORT_FLAGS) \
                        -o $@ $(OBJS)
endif # SONAMEFLAG_PREFIX
endif #!os2

ifeq ($(OS_ARCH), OSF1)
# The linker on OSF/1 gets confused if it finds an so_locations file
# that doesn't meet its expectations, so we arrange to remove it before
# linking.
SO_FILES_TO_REMOVE=so_locations
endif

ifneq (,$(filter BeOS Darwin NetBSD,$(OS_ARCH)))
LINK_DLL	= $(MKSHLIB) $(OBJS)
endif

ifeq ($(OS_ARCH), HP-UX)
# On HPUX, we need a couple of changes:
# 1) Use the C++ compiler for linking, which will pass the +eh flag on down to the
#    linker so the correct exception-handling-aware libC gets used (libnshttpd.sl
#    needs this).
# 2) Add a "-Wl,-E" option so the linker gets a "-E" flag.  This makes symbols
#    in an executable visible to shared libraries loaded at runtime.
LINK_EXE        = $(CC_FOR_LINK) -Wl,-E $(ALDFLAGS) $(LDFLAGS) $(RPATHFLAG_PREFIX)$(RPATHFLAG) -o $@ $(OBJS) $(EXTRA_LIBS) $(PLATFORMLIBS)

ifeq ($(USE_64), 1)
LINK_EXE        = $(CC_FOR_LINK) -DHPUX_ACC -D__STDC_EXT__ -D_POSIX_C_SOURCE=199506L  +DA2.0W +DS2.0 -Wl,-E $(ALDFLAGS) $(LDFLAGS) $(RPATHFLAG_PREFIX)$(RPATHFLAG) -o $@ $(OBJS) $(EXTRA_LIBS) $(PLATFORMLIBS)
endif

else # HP-UX
# everything except HPUX

ifdef USE_LD_RUN_PATH
#does RPATH differently.  instead we export RPATHFLAG as LD_RUN_PATH
#see ns/netsite/ldap/clients/tools/Makefile for an example
export LD_RUN_PATH=$(RPATHFLAG)
LINK_EXE        = $(CC_FOR_LINK) $(ALDFLAGS) $(LDFLAGS) \
                        -o $@ $(OBJS) $(EXTRA_LIBS) $(PLATFORMLIBS)
LINK_EXE_NOLIBSOBJS     =  $(CC_FOR_LINK) $(ALDFLAGS) $(LDFLAGS) -o $@
else # USE_LD_RUN_PATH
LINK_EXE        = $(CC_FOR_LINK) $(ALDFLAGS) $(LDFLAGS) \
                        $(RPATHFLAG_PREFIX)$(RPATHFLAG)$(RPATHFLAG_EXTRAS) \
                        -o $@ $(OBJS) $(EXTRA_LIBS) $(PLATFORMLIBS)
LINK_EXE_NOLIBSOBJS     = $(CC_FOR_LINK) $(ALDFLAGS) $(LDFLAGS) \
                        $(RPATHFLAG_PREFIX)$(RPATHFLAG)$(RPATHFLAG_EXTRAS) -o $@
endif # USE_LD_RUN_PATH
endif # HP-UX
endif # WINNT

ifndef PERL
PERL = perl
endif

#
# shared library symbol export definitions
#
ifeq ($(OS_ARCH), WINNT)
GENEXPORTS=cmd /c  $(PERL) $(LDAP_SRC)/build/genexports.pl
else
GENEXPORTS=$(PERL) $(LDAP_SRC)/build/genexports.pl
endif

