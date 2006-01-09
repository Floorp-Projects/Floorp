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
# The Original Code is the Netscape Portable Runtime (NSPR).
# 
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998-2000
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

ifeq ($(OS_ARCH), HP-UX)
      ifeq ($(OS_TEST), ia64)
              ifdef USE_64
                      64BIT_TAG=_ia64_64
              else
                      64BIT_TAG=_ia64_32
              endif
      endif
endif

PR_UNIXOS	= 1
XP_DEFINE	= -DXP_UNIX
OBJ_SUFFIX	= o
LIB_SUFFIX	= a
DLL_SUFFIX	= so
AR		= ar cr $@

ifdef BUILD_OPT
OPTIMIZER	= -O
DEFINES		= -UDEBUG -DNDEBUG
ifeq ($(OS_ARCH), HP-UX)
OBJDIR_TAG      = $(64BIT_TAG)_OPT
else
OBJDIR_TAG      = _OPT
endif
else
OPTIMIZER	= -g
DEFINES		= -DDEBUG -UNDEBUG -DDEBUG_$(shell whoami)
ifeq ($(OS_ARCH), HP-UX)
OBJDIR_TAG      = $(64BIT_TAG)_DBG
else
OBJDIR_TAG      = _DBG
endif
endif

# Name of the binary code directories
ifdef MOZILLA_CLIENT
OBJDIR_NAME	= $(OS_CONFIG)$(CPU_ARCH_TAG)$(OBJDIR_TAG).OBJ
else
OBJDIR_NAME	= $(OS_CONFIG)$(CPU_ARCH_TAG)$(COMPILER_TAG)$(IMPL_STRATEGY)$(OBJDIR_TAG).OBJ
endif

MKDEPEND_DIR    = $(DEPTH)/config/mkdepend
MKDEPEND 	= $(MKDEPEND_DIR)/$(OBJDIR_NAME)/mkdepend
MKDEPENDENCIES  = $(OBJDIR)/depend.mk

####################################################################
#
# One can define the makefile variable NSDISTMODE to control
# how files are published to the 'dist' directory.  If not
# defined, the default is "install using relative symbolic
# links".  The two possible values are "copy", which copies files
# but preserves source mtime, and "absolute_symlink", which
# installs using absolute symbolic links.  The "absolute_symlink"
# option requires NFSPWD.
#
####################################################################

NSINSTALL	= $(MOD_DEPTH)/config/$(OBJDIR_NAME)/nsinstall

ifeq ($(NSDISTMODE),copy)
# copy files, but preserve source mtime
INSTALL		= $(NSINSTALL) -t
else
ifeq ($(NSDISTMODE),absolute_symlink)
# install using absolute symbolic links
INSTALL		= $(NSINSTALL) -L `$(NFSPWD)`
else
# install using relative symbolic links
INSTALL		= $(NSINSTALL) -R
endif
endif

define MAKE_OBJDIR
if test ! -d $(@D); then rm -rf $(@D); $(NSINSTALL) -D $(@D); else true; fi
endef
