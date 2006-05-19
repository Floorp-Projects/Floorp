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
NSINSTALL_DIR  = $(CORE_DEPTH)/coreconf/nsinstall
NSINSTALL      = nsinstall
INSTALL      = $(NSINSTALL)
MAKE_OBJDIR  = mkdir
MAKE_OBJDIR += $(OBJDIR)
RC           = rc.exe
GARBAGE     += $(OBJDIR)/vc20.pdb $(OBJDIR)/vc40.pdb
XP_DEFINE   += -DXP_PC
LIB_SUFFIX   = lib
DLL_SUFFIX   = dll

ifdef BUILD_OPT
	OS_CFLAGS  += -MD
	OPTIMIZER  += -O2
	DEFINES    += -UDEBUG -U_DEBUG -DNDEBUG
	DLLFLAGS   += -OUT:"$@"
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
	LDFLAGS    += -DEBUG -DEBUGTYPE:CV
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
