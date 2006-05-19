# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is the Netscape Portable Runtime (NSPR).
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1998-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
# 

#
# Configuration common to all (supported) versions of OS/2
#
# OS_CFLAGS is the command line options for the compiler when
#   building the .DLL object files.
# OS_EXE_CFLAGS is the command line options for the compiler
#   when building the .EXE object files; this is for the test
#   programs.
# the macro OS_CFLAGS is set to OS_EXE_CFLAGS inside of the
#   makefile for the pr/tests directory. ... Hack.

# Specify toolset.  Default to EMX.
ifeq ($(MOZ_OS2_TOOLS),VACPP)
XP_OS2_VACPP = 1
else
ifeq ($(MOZ_OS2_TOOLS),PGCC)
XP_OS2_EMX   = 1
else
MOZ_OS2_TOOLS = EMX
XP_OS2_EMX   = 1
endif
endif

ifeq ($(XP_OS2_EMX),1)
MOZ_EMXTAG = $(subst .,,$(MOZ_OS2_EMX_OBJECTFORMAT))
endif

#
# On OS/2 we proudly support gbash...
#
SHELL = GBASH.EXE

CC			= icc -q -DXP_OS2 -N10
CCC			= icc -q -DXP_OS2 -DOS2=4 -N10
LINK			= -ilink
AR			= -ilib /noignorecase /nologo /Out:$(subst /,\\,$@)
RANLIB 			= @echo RANLIB
BSDECHO 		= @echo BSDECHO
NSINSTALL 		= nsinstall
INSTALL			= $(NSINSTALL)
MAKE_OBJDIR 		= if test ! -d $(OBJDIR); then mkdir $(OBJDIR); fi
IMPLIB 			= implib -nologo -noignorecase
FILTER 			= cppfilt -b -p -q
RC 			= rc.exe

GARBAGE =

XP_DEFINE 		= -DXP_PC
LIB_SUFFIX 		= lib
DLL_SUFFIX 		= dll
OBJ_SUFFIX		= obj

OS_CFLAGS     		= -W3 -Wcnd- -gm -gd+ -sd- -su4 -ge- -Mp
OS_EXE_CFLAGS 		= -W3 -Wcnd- -gm -gd+ -sd- -su4 -Mp
AR_EXTRA_ARGS 		= 

ifdef BUILD_OPT
OPTIMIZER		= -O+ -Oi 
DEFINES 		= -UDEBUG -U_DEBUG -DNDEBUG
DLLFLAGS		= -DLL -OUT:$@ -MAP:$(@:.dll=.map)
EXEFLAGS    		= -PMTYPE:VIO -OUT:$@ -MAP:$(@:.exe=.map) -nologo -NOE
OBJDIR_TAG 		= _OPT
else
OPTIMIZER		= -Ti+ -DE
DEFINES 		= -DDEBUG -D_DEBUG -DDEBUGPRINTS
DLLFLAGS		= -DEBUG -DLL -OUT:$@ -MAP:$(@:.dll=.map)
EXEFLAGS    		= -DEBUG -PMTYPE:VIO -OUT:$@ -MAP:$(@:.exe=.map) -nologo -NOE
OBJDIR_TAG 		= _DBG
LDFLAGS 		= -DEBUG 
endif

DEFINES += -DOS2=4
DEFINES += -D_X86_
DEFINES += -D_PR_GLOBAL_THREADS_ONLY -DBSD_SELECT

# Name of the binary code directories
ifdef MOZ_LITE
OBJDIR_NAME 		= $(subst OS2,NAV,$(OS_CONFIG))_$(MOZ_OS2_TOOLS)$(MOZ_EMXTAG)$(OBJDIR_TAG).OBJ
else
OBJDIR_NAME 		= $(OS_CONFIG)_$(MOZ_OS2_TOOLS)$(MOZ_EMXTAG)$(OBJDIR_TAG).OBJ
endif

OS_DLLFLAGS 		= -nologo -DLL -FREE -NOE

ifdef XP_OS2_VACPP

OS_LIBS 		= so32dll.lib tcp32dll.lib

DEFINES += -DXP_OS2_VACPP -DTCPV40HDRS

else
CC			= gcc
CCC			= gcc
LINK			= gcc
RC 			= rc.exe
FILTER  		= emxexp
IMPLIB  		= emximp -o

# Determine which object format to use.  Two choices:
# a.out and omf.  We default to omf.
ifeq ($(MOZ_OS2_EMX_OBJECTFORMAT), A.OUT)
AR      	= ar -q $@
LIB_SUFFIX	= a
else
OMF_FLAG 	= -Zomf
AR		= emxomfar r $@
LIB_SUFFIX	= lib
endif

OS_LIBS     		= -lsocket -lemxio

DEFINES += -DXP_OS2 -DXP_OS2_EMX -DOS2EMX_PLAIN_CHAR

OS_CFLAGS     		= $(OMF_FLAG) -Wall -Wno-unused -Zmtd
OS_EXE_CFLAGS 		= $(OMF_FLAG) -Wall -Wno-unused -Zmtd
OS_DLLFLAGS 		= $(OMF_FLAG) -Zmt -Zdll -Zcrtdll -o $@
ifeq ($(MOZ_OS2_EMX_OBJECTFORMAT),OMF)
EXEFLAGS                += -Zlinker /DE
endif

ifdef BUILD_OPT
OPTIMIZER		= -O3
DLLFLAGS		= 
EXEFLAGS    		= -Zmtd -o $@
else
OPTIMIZER		= -g #-s
DLLFLAGS		= -g #-s
EXEFLAGS		= -g $(OMF_FLAG) -Zmtd -L$(DIST)/lib -o $@   # -s
ifeq ($(MOZ_OS2_EMX_OBJECTFORMAT),OMF)
EXEFLAGS                += -Zlinker /DE
endif
endif

AR_EXTRA_ARGS 		=
endif


