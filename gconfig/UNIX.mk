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

XP_DEFINE  += -DXP_UNIX
LIB_SUFFIX  = a
DLL_SUFFIX  = so
AR          = ar
AR         += cr $@
LDOPTS     += -L$(SOURCE_LIB_DIR)

ifdef BUILD_OPT
	OPTIMIZER  += -O
	DEFINES    += -UDEBUG -DNDEBUG
else
	OPTIMIZER  += -g
	DEFINES    += -DDEBUG -UNDEBUG -DDEBUG_$(shell whoami)
endif

NSINSTALL_DIR  = $(GDEPTH)/gconfig/nsinstall
NSINSTALL      = $(NSINSTALL_DIR)/$(OBJDIR_NAME)/nsinstall

MKDEPEND_DIR    = $(GDEPTH)/gconfig/mkdepend
MKDEPEND        = $(MKDEPEND_DIR)/$(OBJDIR_NAME)/mkdepend
MKDEPENDENCIES  = $(NSINSTALL_DIR)/$(OBJDIR_NAME)/depend.mk

GUI_LIBS        = -lXm -lXt -lX11
MATH_LIB        = -lm
OPT_SLASH       = /
LIB_PREFIX      = lib
LIB_SUFFIX      = a
NSPR_LIBS       = nspr21 plds21 plc21 nspr21
LINK_PROGRAM    = $(CC)
XP_REG_LIB      = reg
ARCHIVE_SUFFIX  = 
VERSION_NUMBER  =
NATIVE_PLATFORM = unix
NATIVE_RAPTOR_WIDGET = widgetunix
NATIVE_RAPTOR_GFX = gfxunix
NATIVE_RAPTOR_WEB=raptorwebwidget
RAPTOR_GFX=raptorgfx
NATIVE_SMTP_LIB=smtp
NATIVE_MIME_LIB=mime
NATIVE_MSG_COMM_LIB=comm
NATIVE_JULIAN_DLL=julian
PLATFORM_DIRECTORY=unix
NATIVE_ZLIB_DLL=zlib
NATIVE_XP_DLL=xp
XP_PREF_DLL=pref
AR_ALL =
AR_NONE =

ifdef MOZ_ZULU_FREE
NATIVE_LIBNLS_LIBS=nlsstub10
else
NATIVE_LIBNLS_LIBS=nsfmt$(MOZ_BITS)30 nsuni$(MOZ_BITS)30 nscck$(MOZ_BITS)30 nsjpn$(MOZ_BITS)30 nscnv$(MOZ_BITS)30 nssb$(MOZ_BITS)30
endif

####################################################################
#
# One can define the makefile variable NSDISTMODE to control
# how files are published to the 'dist' directory.  If not
# defined, the default is "install using relative symbolic
# links".  The two possible values are "copy", which copies files
# but preserves source mtime, and "absolute_symlink", which
# installs using absolute symbolic links.  The "absolute_symlink"
# option requires NFSPWD.
#   - THIS IS NOT PART OF THE NEW BINARY RELEASE PLAN for 9/30/97
#   - WE'RE KEEPING IT ONLY FOR BACKWARDS COMPATIBILITY
####################################################################

ifeq ($(NSDISTMODE),copy)
	# copy files, but preserve source mtime
	INSTALL  = $(NSINSTALL)
	INSTALL += -t
else
	ifeq ($(NSDISTMODE),absolute_symlink)
		# install using absolute symbolic links
		INSTALL  = $(NSINSTALL)
		INSTALL += -L `$(NFSPWD)`
	else
		# install using relative symbolic links
		INSTALL  = $(NSINSTALL)
		INSTALL += -R
	endif
endif

define MAKE_OBJDIR
if test ! -d $(@D); then rm -rf $(@D); $(NSINSTALL) -D $(@D); fi
endef

ifdef MOZ_TRACE_XPCOM_REFCNT
DEFINES += -DMOZ_TRACE_XPCOM_REFCNT
endif
