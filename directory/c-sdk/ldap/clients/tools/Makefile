#
# GNU Makefile for LDAP client tools.
#

LDAP_SRC = ../..
MCOM_ROOT = ../../../..

NOSTDCLEAN=true # don't let nsconfig.mk define target clean
NOSTDSTRIP=true # don't let nsconfig.mk define target strip
NSPR20=true	# probably should be defined somewhere else (not sure where)

OBJDEST = $(OBJDIR)/clients/tools/obj
BINDIR = $(OBJDIR)/clients/bin

include $(MCOM_ROOT)/c-sdk/nsdefs.mk
include $(MCOM_ROOT)/c-sdk/nsconfig.mk

include $(LDAP_SRC)/nsldap.mk
ifndef LDAP_USE_OLD_DB
include $(MCOM_ROOT)/c-sdk/ns_usedb.mk
endif

# NLS Library

NLS_COMP_NAME = libnls
NLS_VERS_NUM = 31
NLS_COMP_DIR = $(NLS_COMP_NAME)$(NLS_VERS_NUM)
NLS_ROOT = $(MCOM_ROOT)/../components/$(NLS_COMP_DIR)

NLS_LIBPATH = $(NLS_ROOT)/$(NLSOBJDIR)/lib
NLS_INCLUDE = $(NLS_ROOT)/$(NLSOBJDIR)/include
NLS_LOCALE  = $(NLS_ROOT)/$(NLSOBJDIR)/locale$(NLS_VERS_NUM)
NLS_CONV    = $(NLS_ROOT)/$(NLSOBJDIR)/conv$(NLS_VERS_NUM)
NLS_VERSION_FILE = $(NLS_ROOT)/Version


NLS_RELDATE=v3.2

NLS_VERSION = $(NLS_RELDATE)

ifeq ($(ARCH), WINNT)
  ifeq ($(OS_TARGET), WIN95)
	ifeq ($(DEBUG), full)
	  NLSOBJDIR = WIN954.0_DBG.OBJ
	else
	  ifeq ($(DEBUG), fulld)
	    NLSOBJDIR = WIN954.0_DBG.OBJD
	  else
	    NLSOBJDIR = WIN954.0_OPT.OBJ
	  endif
	endif
  else
	ifeq ($(DEBUG), full)
	  NLSOBJDIR = WINNT4.0_DBG.OBJ
	else
	  ifeq ($(DEBUG), fulld)
	    NLSOBJDIR = WINNT4.0_DBG.OBJD
	  else
	    NLSOBJDIR = WINNT4.0_OPT.OBJ
	  endif
	endif
  endif
  NLS_RELEASE = $(COMPONENTS_DIR)/libnls$(NLS_VERS_NUM)/$(NLS_VERSION)/$(NLSOBJDIR)
#  NLS_LIBNAMES = brk32$(NLS_VERS_NUM) cnv32$(NLS_VERS_NUM) col32$(NLS_VERS_NUM) fmt32$(NLS_VERS_NUM) res32$(NLS_VERS_NUM) uni32$(NLS_VERS_NUM)
  NLS_LIBNAMES = cnv32$(NLS_VERS_NUM) 
  NLS_CONV_LIBNAMES = jpn32$(NLS_VERS_NUM) cck32$(NLS_VERS_NUM) sb32$(NLS_VERS_NUM)
  NLS_LIBS = $(addsuffix .lib,  $(addprefix ns, $(NLS_LIBNAMES)))
  NLS_CONV_LIBS = $(addsuffix .lib, $(addprefix ns, $(NLS_CONV_LIBNAMES)))
  NLSDLL_NAMES = $(addsuffix .dll, $(addprefix ns, $(NLS_LIBNAMES)))
  NLS_CONVDLL_NAMES = $(addsuffix .dll, $(addprefix ns, $(NLS_CONV_LIBNAMES)))
  LIBNLS = $(addprefix $(NLS_LIBPATH)/, $(NLS_LIBS))
  LIBNLS_CONV = $(addprefix $(NLS_LIBPATH)/, $(NLS_CONV_LIBS))
  NLSOBJNAME = $(NLS_LIBNAMES)
else
ifeq ($(ARCH), HPUX)
  ifeq ($(NSOS_RELEASE), B.11.00)
    NLSOBJDIR = $(NSOBJDIR_NAME)
    NLS_RELEASE = $(COMPONENTS_DIR)/libnls$(NLS_VERS_NUM)/$(NLS_VERSION)/$(NLSOBJDIR)
  else
    NLSOBJDIR = $(NSOBJDIR_NAME)
    NLS_RELEASE = $(COMPONENTS_DIR)/libnls$(NLS_VERS_NUM)/$(NLS_VERSION)/$(NLSOBJDIR)
  endif
else
# Here is a hack to replace _PTH with n32, this must be done before
# the _PTH substitustion

  ifeq ($(ARCH),IRIX)
    ifeq ($(USE_N32),1)
      NLSOBJDIR = $(NSOBJDIR_NAME2_NOTAG)
    else 
      NLSOBJDIR = $(NSOBJDIR_NAME_NOTAG)
    endif
  else
    NLSOBJDIR = $(NSOBJDIR_NAME_NOTAG)

  ifeq ($(ARCH), OSF1)
    NLSOBJDIR = $(NSOBJDIR_NAME)
  endif
  endif

# NLS Does not has PTH version, so here is the hack which treat non PTH
# version as PTH version
 ifeq ($(USE_PTHREADS), 1)
    NLS_RELEASE = $(COMPONENTS_DIR)/libnls$(NLS_VERS_NUM)/$(NLS_VERSION)/$(NLSOBJDIR)
 else
  NLS_RELEASE = $(COMPONENTS_DIR)/libnls$(NLS_VERS_NUM)/$(NLS_VERSION)/$(NLSOBJDIR)
 endif
endif # HPUX
  NLS_LIBNAMES = cnv$(NLS_VERS_NUM)
  NLS_CONV_LIBNAMES = jpn$(NLS_VERS_NUM) cck$(NLS_VERS_NUM) sb$(NLS_VERS_NUM)
  NLS_SOLIB_NAMES = $(addsuffix $(DLL_PRESUF), $(addprefix libns, $(NLS_LIBNAMES)))
  NLS_CONV_SOLIB_NAMES = $(addsuffix $(DLL_PRESUF), $(addprefix libns, $(NLS_CONV_LIBNAMES)))
  NLS_LIBS = $(addsuffix .a, $(NLS_SOLIB_NAMES))
  NLS_CONV_LIBS = $(addsuffix .a, $(NLS_CONV_SOLIB_NAMES))
  NLS_SOLIBS = $(addsuffix .$(DLL_SUFFIX), $(NLS_SOLIB_NAMES))
  NLS_CONV_SOLIBS = $(addsuffix .$(DLL_SUFFIX), $(NLS_CONV_SOLIB_NAMES))
  NLSOBJNAME = $(NLS_SOLIBS)
  LIBNLS = $(addprefix $(NLS_LIBPATH)/, $(NLS_SOLIBS))
 
  ifeq ($(ARCH), OSF1)
   NLSLINK = -L$(NLS_LIBPATH) $(addprefix -l,  $(addsuffix $(DLL_PRESUF), $(addprefix ns, $(NLS_LIBNAMES)))) -lcxx
   NLSLINK_STATIC = $(addprefix $(NLS_LIBPATH)/, $(NLS_LIBS)) -lcxx
   NLSLINK_CONV_STATIC = $(addprefix $(NLS_LIBPATH)/, $(NLS_CONV_LIBS))
  else
  ifeq ($(ARCH),HPUX)
   #linking with libC is *BAD* on HPUX11
   NLSLINK = -L$(NLS_LIBPATH) $(addprefix -l,  $(addsuffix $(DLL_PRESUF), $(addprefix ns, $(NLS_LIBNAMES))))
   NLSLINK_STATIC = $(addprefix $(NLS_LIBPATH)/, $(NLS_LIBS))
   NLSLINK_CONV_STATIC = $(addprefix $(NLS_LIBPATH)/, $(NLS_CONV_LIBS))
  else
  ifeq ($(ARCH),Linux)
   NLSLINK = -L$(NLS_LIBPATH) $(addprefix -l,  $(addsuffix $(DLL_PRESUF), $(addprefix ns, $(NLS_LIBNAMES)))) -lresolv -lpthread
   NLSLINK_STATIC = $(addprefix $(NLS_LIBPATH)/, $(NLS_LIBS)) -lresolv -lpthread
  else
  ifeq ($(ARCH),IRIX)
   NLSLINK = -L$(NLS_LIBPATH) $(addprefix -l,  $(addsuffix $(DLL_PRESUF), $(addprefix ns, $(NLS_LIBNAMES))))  -lpthread
   NLSLINK_STATIC = $(addprefix $(NLS_LIBPATH)/, $(NLS_LIBS)) -lpthread
  else
   NLSLINK = -L$(NLS_LIBPATH) $(addprefix -l,  $(addsuffix $(DLL_PRESUF), $(addprefix ns, $(NLS_LIBNAMES)))) -lC
   NLSLINK_STATIC = $(addprefix $(NLS_LIBPATH)/, $(NLS_LIBS)) -lC
  endif             # Irix
  endif             # Linux
   NLSLINK_CONV_STATIC = $(addprefix $(NLS_LIBPATH)/, $(NLS_CONV_LIBS))
  endif             # HPUX
endif               # OSF1
endif
 
ifeq ($(ARCH), SOLARIS)
NLSLINK += -lw
endif

$(LIBNLS): $(NLS_VERSION_FILE) nls_version_check
 
$(NLS_VERSION_FILE):
		mkdir -p $(NLS_ROOT); \
		cd $(NLS_ROOT); echo "garbage" > Version

ifneq ($(ARCH), WINNT)
ifndef FORCE_FTP
nls_version_check:
		@if [ `cat $(NLS_VERSION_FILE)` != "$(NLS_VERSION)" -o ! -d $(NLS_ROOT)/$(NLSOBJDIR) ]; then \
 		if [ -d $(NLS_RELEASE) ]; then \
			if [ "$(RELEASE_TREE)" != "" ]; then \
                                (cd $(NLS_ROOT); \
				\rm -rf $(NLSOBJDIR) $(NLS_VERSION_FILE); \
				ln -s $(RELEASE_TREE)/libnls31/$(NLSOBJDIR) $(NLSOBJDIR)); \
				echo "$(NLS_VERSION)" > $(NLS_VERSION_FILE); \
			else \
				(cd $(NLS_ROOT); \
				\rm -rf $(NLSOBJDIR) $(NLS_VERSION_FILE); \
				ln -s $(NLS_RELEASE) $(NLSOBJDIR)); \
				echo "$(NLS_VERSION)" > $(NLS_VERSION_FILE); \
			fi; \
		else \
			echo "************************** ERROR *************************"; \
			echo "LIBNLS client libraries are missing.  Make sure your machine"; \
			echo "has access to $(NLS_RELEASE)"; \
			echo "**********************************************************"; \
			exit 1; \
		fi; \
	fi; \
 
else
 
nls_version_check:
# The following shell script recalculates $(NLS_INCLUDE), $(NLS_LIBPATH) and
# $(NLS_VERSION_FILE)
	sh $(MCOM_ROOT)/c-sdk/compver.sh $(NLS_ROOT) $(NLS_VERSION) \
	$(NLSOBJDIR) $(MCOM_ROOT) $(MODULE) $(NLS_RELEASE) libnls$(NLS_VERS_NUM) \
	"' include lib conv$(NLS_VERS_NUM) locale$(NLS_VERS_NUM) '" $(NLS_ROOT)/$(NLSOBJDIR)/locale$(NLS_VERS_NUM)/index.txt
endif
else
 
nls_version_check:
# The following shell script recalculates $(NLS_INCLUDE), $(NLS_LIBPATH) and
# $(NLS_VERSION_FILE)
	sh $(MCOM_ROOT)/c-sdk/compver.sh $(NLS_ROOT) $(NLS_VERSION) \
	$(NLSOBJDIR) $(MCOM_ROOT) ldapsdk $(NLS_RELEASE) \
	libnls$(NLS_VERS_NUM) \
	"' include lib conv$(NLS_VERS_NUM) locale$(NLS_VERS_NUM) '" \
	$(NLS_ROOT)/$(NLSOBJDIR)/locale$(NLS_VERS_NUM)/index.txt
endif

ifeq ($(ARCH), HPUX)
  ifeq ($(NSOS_RELEASE), B.11.00)
    EXTRA_LIBS += -lrt 
    CXX=aCC -DHPUX_ACC -D__STDC_EXT__ -D_POSIX_C_SOURCE=199506L
    LINK_EXE_NOLIBSOBJS_NOCXX       = $(CXX) -Wl,-E $(ALDFLAGS) $(LDFLAGS) \
	  $(RPATHFLAG_PREFIX)$(RPATHFLAG) -o $@
    LINK_EXE_NOCXX = $(CXX) -Wl,-E $(ALDFLAGS) $(LDFLAGS) \
	$(RPATHFLAG_PREFIX)$(RPATHFLAG) -o $@ $(OBJS) $(EXTRA_LIBS)
  endif
  ifeq ($(NSOS_RELEASE), B.11.00_64)
    EXTRA_LIBS += -lrt 
    CXX=aCC -DHPUX_ACC -D__STDC_EXT__ -D_POSIX_C_SOURCE=199506L  +DA2.0W +DS2.0
    LINK_EXE_NOLIBSOBJS_NOCXX       = $(CXX) -Wl,-E $(ALDFLAGS) $(LDFLAGS) \
	  $(RPATHFLAG_PREFIX)$(RPATHFLAG) -o $@
    LINK_EXE_NOCXX = $(CXX) -Wl,-E $(ALDFLAGS) $(LDFLAGS) \
	$(RPATHFLAG_PREFIX)$(RPATHFLAG) -o $@ $(OBJS) $(EXTRA_LIBS)
  endif
  ifeq ($(NSOS_RELEASE), B.10.10)
     CC= CC +a1
#    CXX=CC -D__STDC_EXT__
#    $(subst "+e", "",$(CFLAGS))
  endif
endif

ifeq ($(ARCH),Linux)
CXX = egcs
LINK_EXE        = $(CXX) $(ALDFLAGS) $(LDFLAGS) \
			$(RPATHFLAG_PREFIX)$(RPATHFLAG)$(RPATHFLAG_EXTRAS) \
			-o $@ $(OBJS) $(EXTRA_LIBS)
LINK_EXE_NOLIBSOBJS     = $(CXX) $(ALDFLAGS) $(LDFLAGS) \
			$(RPATHFLAG_PREFIX)$(RPATHFLAG)$(RPATHFLAG_EXTRAS) -o $@

else
ifeq ($(ARCH), OSF1)
ifdef USE_LD_RUN_PATH
#does RPATH differently.  instead we export RPATHFLAG as LD_RUN_PATH
#see ns/c-sdk/ldap/clients/tools/Makefile for an example
export LD_RUN_PATH=$(RPATHFLAG)
LINK_EXE        = $(CXX) $(ALDFLAGS) $(LDFLAGS) \
			-o $@ $(OBJS) $(EXTRA_LIBS)
LINK_EXE_NOLIBSOBJS     =  $(CXX) $(ALDFLAGS) $(LDFLAGS) -o $@
else # USE_LD_RUN_PATH
LINK_EXE        = $(CXX) $(ALDFLAGS) $(LDFLAGS) \
			$(RPATHFLAG_PREFIX)$(RPATHFLAG)$(RPATHFLAG_EXTRAS) \
			-o $@ $(OBJS) $(EXTRA_LIBS)
LINK_EXE_NOLIBSOBJS     = $(CXX) $(ALDFLAGS) $(LDFLAGS) \
			$(RPATHFLAG_PREFIX)$(RPATHFLAG)$(RPATHFLAG_EXTRAS) -o $@
endif # USE_LD_RUN_PATH
endif 
endif 
MCOM_INCLUDE += -I$(NLS_INCLUDE)

INCLUDES += $(SSLINCLUDE) $(NO_LIBLCACHE)
DEFS += $(SSL)

ALDFLAGS += $(SSLLIBFLAG)

ifeq ($(ARCH), WINNT)
SUBSYSTEM=console
endif

ifneq ($(ARCH), WINNT)
# Run-time search path for shared libraries: in admin server 3.x installations,
# the ldap tools get installed under NSHOME/userdb/ldap/tools and the LDAP SDK
# libraries (libldap and liblcache) get installed under NSHOME/lib, so the
# correct library search path is ../../../lib. We also want to search
# NSHOME/bin/slapd, where more recent versions of the SDK so's may be.
RPATHFLAG=..:../lib:../../lib:../../../lib:../../../../lib
ifdef USE_LD_RUN_PATH
LD_RUN_PATH=$(RPATHFLAG)
export LD_RUN_PATH
endif
endif

# svrcore is used by ntpinobj, and pinarg handling
LIBSVRCORE_DEP=  $(NSCP_DISTDIR)/lib/svrcore.$(LIB_SUFFIX)
ifeq ($(ARCH), WINNT)
LIBSVRCORE= $(NSCP_DISTDIR)/lib/svrcore.$(LIB_SUFFIX)
else
LIBSVRCORE = -lsvrcore
endif

ifeq ($(INCLUDE_SSL),1)
# libsec and friends are already in libldap, so don't link with them here
  LDTOOLS_LIBS_DEP = $(LDAP_SDK_LIBLDAP_DLL_DEP) $(EXTRA_LIBS_DEP)
  LDTOOLS_LIBS = $(LDAP_SDK_LIBLDAP_DLL)
  ifneq ($(LDAP_NO_LIBLCACHE),1)
    LDTOOLS_LIBS_DEP += $(LDAP_LIBLDIF_DEP)
    LDTOOLS_LIBS += $(LDAP_LIBLDIF) $(LDAP_SDK_LIBLCACHE_DLL)
  else
    LDTOOLS_LIBS_DEP += $(LDAP_LIBLDIF_DEP)
    LDTOOLS_LIBS += $(LDAP_LIBLDIF)
  endif

  LDTOOLS_LIBS += $(EXTRA_LIBS) $(EXTRA_MALLOC_LIBS) $(LDAP_SDK_LIBLDAP_DLL) $(LIBSVRCORE)

# XXXceb The linker on NT is auto optimizing.  We still need to have libsec around for svrcore
#        Do Not Mess with the link order.  This was not straightforward, and "optimizing" the text
#        will break the builds.
  ifeq ($(ARCH), WINNT)
    LDTOOLS_LIBS += $(LIBSEC)
  endif
  ifeq ($(ARCH), SOLARIS)
    LDTOOLS_LIBS += $(LIBSEC) $(LIBDB)
  endif
  ifeq ($(ARCH), HPUX)
    LDTOOLS_LIBS += $(LIBSEC)
  endif
  ifeq ($(ARCH), IRIX)
    LDTOOLS_LIBS += $(LIBSEC) $(LIBDB)
  endif
  ifeq ($(ARCH), AIX)
    LDTOOLS_LIBS += $(LIBSEC) $(LIBDB)
  endif
  ifeq ($(ARCH), WINNT)
  LDTOOLS_LIBS += $(LIBNSPR)
  else
  LDTOOLS_LIBS += $(DYNAMICNSPR)
  endif
else
# SSL is not included in the LDAP shared library...
# need to link with libsec and friends explicitly
  LDTOOLS_LIBS_DEP = $(LDAP_SDK_LIBS_DEP) $(LDAP_LIBLDIF_DEP) \
	$(EXTRA_LIBS_DEP)
  LDTOOLS_LIBS_DEP += $(LDAP_LIBLDIF_DEP)
ifeq ($(ARCH), OSF1)
  LDTOOLS_LIBS = $(LDAP_SDK_LIBSSLDAP_LIB) $(LDAP_SDK_LIBLDAP_DLL) $(LDAP_SDK_LIBPRLDAP_DLL) 
else
  LDTOOLS_LIBS = $(LDAP_SDK_LIBSSLDAP_LIB) $(LDAP_SDK_LIBPRLDAP_DLL) $(LDAP_SDK_LIBLDAP_DLL) 
endif
  ifneq ($(LDAP_NO_LIBLCACHE),1)
    LDTOOLS_LIBS += $(LDAP_SDK_LIBLCACHE_DLL)
  endif
  LDTOOLS_LIBS +=  $(LIBSVRCORE) $(LIBSEC) $(LIBDB) $(EXTRA_LIBS)\
	$(EXTRA_MALLOC_LIBS) $(LDAP_LIBLDIF)

  ifneq ($(LDAP_NO_LIBLCACHE),1)
    ifndef LDAP_USE_OLD_DB
      LDTOOLS_LIBS_DEP+=$(DB_LIB_DEP)
      LDTOOLS_LIBS+=$(DB_LIB)
    endif
  endif

  ifeq ($(LDAP_NO_LIBLCACHE),1)
    CFLAGS+=-DNO_LIBLCACHE
  endif

ifeq ($(ARCH), WINNT)
LDTOOLS_LIBS += $(LIBNSPR)
else
LDTOOLS_LIBS += $(DYNAMICNSPR)
endif

# INCLUDE_SSL
endif

LDTOOLS_LIBS_DEP += $(LIBNLS)
ifeq ($(ARCH), WINNT)
LDTOOLS_LIBS += $(LIBNLS) $(LIBNLS_CONV)
else
LDTOOLS_LIBS += $(NLSLINK_STATIC) $(NLSLINK_CONV_STATIC)
endif




#LDTOOLS_LIBS += $(LIBSVRCORE)

ifeq ($(ARCH), AIX)
LDTOOLS_LIBS += $(EXE_EXTRA_LIBS)
endif 

ifeq ($(ARCH), WINNT)
ALDFLAGS += -FORCE:MULTIPLE /NODEFAULTLIB:MSVCRTD
endif

#ifeq ($(ARCH), HPUX)
#CC=CC +a1
#LINK_EXE = $(LINK_EXE_NOCXX)
#endif

ifeq ($(ARCH),IRIX)
ifeq ($(USE_N32),1)
CXX=CC
LDFLAGS += -n32 -mips3
CC=cc
LINK_EXE        = $(CXX) $(ALDFLAGS) $(LDFLAGS) \
			-o $@ $(OBJS) $(EXTRA_LIBS)
endif
endif

ifeq ($(ARCH), SOLARIS)
ifeq ($(USE_64), 1)
LINK_EXE += -xarch=v9 -L/usr/lib/sparcv9 -lCrun
endif
endif

LINK_EXE += -$(LIBPATH)../$(MCOM_ROOT)/dist/$(NC_BUILD_FLAVOR)/lib

LDAPDELETE_OBJ		= $(addprefix $(OBJDEST)/, ldapdelete.o) 
LDAPMODIFY_OBJ		= $(addprefix $(OBJDEST)/, ldapmodify.o fileurl.o) 
LDAPMODRDN_OBJ		= $(addprefix $(OBJDEST)/, ldapmodrdn.o) 
LDAPSEARCH_OBJ		= $(addprefix $(OBJDEST)/, ldapsearch.o fileurl.o)
LDAPCMP_OBJ     	= $(addprefix $(OBJDEST)/, ldapcmp.o)
LDAPTOOLCOMMON_OBJ	= $(addprefix $(OBJDEST)/, common.o convutf8.o argpin.o ntuserpin.o)

CLIENT_OBJS = $(LDAPDELETE_OBJ) $(LDAPMODIFY_OBJ) $(LDAPMODRDN_OBJ) \
	$(LDAPSEARCH_OBJ) $(LDAPCMP_OBJ) $(LDAPTOOLCOMMON_OBJ)

LDAPDELETE = $(addsuffix $(EXE_SUFFIX), $(addprefix $(BINDIR)/, ldapdelete))
LDAPMODIFY = $(addsuffix $(EXE_SUFFIX), $(addprefix $(BINDIR)/, ldapmodify))
LDAPMODRDN = $(addsuffix $(EXE_SUFFIX), $(addprefix $(BINDIR)/, ldapmodrdn))
LDAPSEARCH = $(addsuffix $(EXE_SUFFIX), $(addprefix $(BINDIR)/, ldapsearch))
LDAPADD    = $(addsuffix $(EXE_SUFFIX), $(addprefix $(BINDIR)/, ldapadd))
LDAPCMP    = $(addsuffix $(EXE_SUFFIX), $(addprefix $(BINDIR)/, ldapcmp))

BINS=	 $(LDAPDELETE) $(LDAPMODIFY) $(LDAPSEARCH) $(LDAPCMP)

all:	$(OBJDEST) $(BINDIR) $(BINS)
cmp:	$(LDAPCMP)

$(LDAPTOOLCOMMON_OBJ):	$(LIBNLS)

$(LDAPDELETE):	$(LDAPDELETE_OBJ) $(LDAPTOOLCOMMON_OBJ) $(LDTOOLS_LIBS_DEP)
	$(LINK_EXE) $(LDAPDELETE_OBJ) $(LDAPTOOLCOMMON_OBJ) $(LDTOOLS_LIBS)

$(LDAPMODIFY):	$(LDAPMODIFY_OBJ) $(LDAPTOOLCOMMON_OBJ) $(LDTOOLS_LIBS_DEP)
	$(LINK_EXE) $(LDAPMODIFY_OBJ) $(LDAPTOOLCOMMON_OBJ) $(LDTOOLS_LIBS)

$(LDAPMODRDN):	$(LDAPMODRDN_OBJ) $(LDAPTOOLCOMMON_OBJ) $(LDTOOLS_LIBS_DEP)
	$(LINK_EXE) $(LDAPMODRDN_OBJ) $(LDAPTOOLCOMMON_OBJ) $(LDTOOLS_LIBS)

$(LDAPSEARCH):	$(LDAPSEARCH_OBJ) $(LDAPTOOLCOMMON_OBJ) $(LDTOOLS_LIBS_DEP)
	$(LINK_EXE) $(LDAPSEARCH_OBJ) $(LDAPTOOLCOMMON_OBJ) $(LDTOOLS_LIBS)

$(LDAPADD): $(LDAPMODIFY) $(LDTOOLS_LIBS_DEP)
	$(RM) $(LDAPADD)
	$(HARDLN) $(LDAPMODIFY) $(LDAPADD)

$(LDAPCMP):	$(LDAPCMP_OBJ) $(LDAPTOOLCOMMON_OBJ) $(LDTOOLS_LIBS_DEP)
	$(LINK_EXE) $(LDAPCMP_OBJ) $(LDAPTOOLCOMMON_OBJ) $(LDTOOLS_LIBS)

clientSDK:	all

$(OBJDEST):
	$(MKDIR) $(OBJDEST)

$(BINDIR):
	$(MKDIR) $(BINDIR)

clean:
	$(RM) $(CLIENT_OBJS)
	$(RM) $(BINS)

#
# Simple, local dependencies
#
LDAPTOOL_COMMON_DEPS = ldaptool.h Makefile
$(LDAPTOOLCOMMON_OBJ):	$(LDAPTOOL_COMMON_DEPS)
$(LDAPDELETE_OBJ):	$(LDAPTOOL_COMMON_DEPS)
$(LDAPMODIFY_OBJ):	$(LDAPTOOL_COMMON_DEPS) fileurl.h
$(LDAPMODRDN_OBJ):	$(LDAPTOOL_COMMON_DEPS)
$(LDAPSEARCH_OBJ):	$(LDAPTOOL_COMMON_DEPS) fileurl.h
$(LDAPCMP_OBJ):		$(LDAPTOOL_COMMON_DEPS)
