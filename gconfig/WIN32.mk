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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#
# Configuration common to all versions of Windows NT
# and Windows 95
#

DEFAULT_COMPILER = cl

CC           = cl
CCC          = cl
LINK         = link
AR           = lib
AR          += -NOLOGO -OUT:"$@"
RANLIB       = echo
BSDECHO      = echo
NSINSTALL_DIR  = $(GDEPTH)/gconfig/nsinstall
NSINSTALL      = nsinstall
INSTALL      = $(NSINSTALL)
MAKE_OBJDIR  = mkdir
MAKE_OBJDIR += $(OBJDIR)
RC           = rc.exe
GARBAGE     += $(OBJDIR)/vc20.pdb $(OBJDIR)/vc40.pdb
MAPFILE      = $(PROGRAM:.exe=.map)
MAP          = /MAP:$(MAPFILE)
PDBFILE      = $(PROGRAM:.exe=.pdb)
PDB          = /PDB:$(PDBFILE)
IMPFILE      = $(PROGRAM:.exe=.lib)
IMP          = /IMPLIB:$(IMPFILE)
XP_DEFINE   += -DXP_PC
LIB_SUFFIX   = lib
DLL_SUFFIX   = dll
OUT_NAME     = -out:
ARCHIVE_SUFFIX = _s
NATIVE_PLATFORM=win
NATIVE_RAPTOR_WIDGET =
NATIVE_RAPTOR_GFX = raptorgfxwin
NATIVE_RAPTOR_WEB=raptorweb
NATIVE_SMTP_LIB=libsmtp
NATIVE_MIME_LIB=libmime
NATIVE_MSG_COMM_LIB=libcomm
RAPTOR_GFX=
VERSION_NUMBER=50
NATIVE_JULIAN_DLL=jul$(MOZ_BITS)$(VERSION_NUMBER)
PLATFORM_DIRECTORY=windows
NATIVE_ZLIB_DLL=zip$(MOZ_BITS)$(VERSION_NUMBER)
NATIVE_XP_DLL=xplib

ifdef MOZ_ZULU_FREE
NATIVE_LIBNLS_LIBS=nlsstub10
else
NATIVE_LIBNLS_LIBS=nsfmt$(MOZ_BITS)30 nsuni$(MOZ_BITS)30 nscck$(MOZ_BITS)30 nsjpn$(MOZ_BITS)30 nscnv$(MOZ_BITS)30 nssb$(MOZ_BITS)30
endif

XP_PREF_DLL=xppref$(MOZ_BITS)

ifdef RCFILE
RCFILE       := $(RCFILE).rc
RESFILE      = $(OBJDIR)/$(RCFILE:.rc=.res)
endif

OS_LIBS = gdi32.lib kernel32.lib advapi32.lib user32.lib
MATH_LIB=

GUI_LIBS = 
NSPR_LIBS = libplds21 libplc21 libnspr21 
OPT_SLASH = /
LIB_PREFIX      =
XP_REG_LIB      = libreg$(MOZ_BITS)


ifdef BUILD_OPT
	OS_CFLAGS  += -MD
	OPTIMIZER  += -O2
	DEFINES    += -UDEBUG -U_DEBUG -DNDEBUG
	DLLFLAGS   += -OUT:"$@"
	LDFLAGS    += /SUBSYSTEM:WINDOWS /NOLOGO
else
	#
	# Define USE_DEBUG_RTL if you want to use the debug runtime library
	# (RTL) in the debug build
	#
	ifdef USE_DEBUG_RTL
		OS_CFLAGS += -MDd
	else
		OS_CFLAGS += -MD
	endif
	OPTIMIZER  += -Od -Z7
	#OPTIMIZER += -Zi -Fd$(OBJDIR)/ -Od
	DEFINES    += -DDEBUG -D_DEBUG -UNDEBUG
	DLLFLAGS   += -DEBUG -DEBUGTYPE:CV -OUT:"$@"
	LDFLAGS    += -DEBUG -DEBUGTYPE:BOTH  /SUBSYSTEM:CONSOLE /NOLOGO
endif

# XXX FIXME: I doubt we use this.  It is redundant with
# SHARED_LIBRARY.
ifdef DLL
	DLL := $(addprefix $(OBJDIR)/, $(DLL))
endif

DEFINES += -DWIN32

#
#  The following is NOT needed for the NSPR 2.0 library.
#

DEFINES += -D_WINDOWS

#
#  The default is no MFC anywhere
#

DEFINES += -DWIN32_LEAN_AND_MEAN

ifdef MOZ_TRACE_XPCOM_REFCNT
DEFINES += -DMOZ_TRACE_XPCOM_REFCNT
endif

