#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "MPL"); you may not use this file
# except in compliance with the MPL. You may obtain a copy of
# the MPL at http://www.mozilla.org/MPL/
# 
# Software distributed under the MPL is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the MPL for the specific language governing
# rights and limitations under the MPL.
# 
# The Original Code is lineterm.
# 
# The Initial Developer of the Original Code is Ramalingam Saravanan.
# Portions created by Ramalingam Saravanan <svn@xmlterm.org> are
# Copyright (C) 1999 Ramalingam Saravanan. All Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License (the "GPL"), in which case
# the provisions of the GPL are applicable instead of
# those above. If you wish to allow use of your version of this
# file only under the terms of the GPL and not to allow
# others to use your version of this file under the MPL, indicate
# your decision by deleting the provisions above and replace them
# with the notice and other provisions required by the GPL.
# If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
#

# xmlterm_config.mk: XMLTERM-specific configuration options

# Options
#  STAND_ALONE:    compile outside Mozilla/NSPR environment
#  USE_NCURSES:    use NCURSES (for stand-alone mode only)
#  DEBUG:          debug option
#  NO_WORKAROUND:  disables workarounds to expose bugs
#  USE_GTK_WIDGETS use GTK widget library
#  USE_NSPR_BASE:  use basic NSPR API (excluding I/O and process creation)
#  USE_NSPR_IO:    use NSPR I/O and process API instead of Unix API
#  NO_PTY:         force use of pipes rather than PTY for process communication
#  NO_CALLBACK:    do not use GTK callbacks to handle LineTerm output
#                  (use polling instead)

ifdef NO_WORKAROUND
DEFINES        += -DNO_WORKAROUND
endif

#
# OS dependent options
#
ifneq (,$(filter-out Linux SunOS FreeBSD,$(OS_ARCH)))
# Unsupported platform for PTY; use pipes for process communication
NO_PTY = 1
endif

ifeq ($(OS_ARCH),Linux)
DEFINES    += -DLINUX -DHAVE_WCSSTR
endif

ifeq ($(OS_CONFIG),SunOS5)
DEFINES    += -DSOLARIS
endif

ifeq ($(OS_ARCH),FreeBSD)
DEFINES    += -DBSDFAMILY
endif

#
# Netscape Portable Runtime options
#
ifdef STAND_ALONE
ifdef USE_NCURSES
DEFINES        += -DUSE_NCURSES
endif

else
# Use NSPR base
USE_NSPR_BASE   = 1

ifeq ($(MOZ_WIDGET_TOOLKIT),gtk)
USE_GTK_WIDGETS = 1
endif

endif

ifdef USE_GTK_WIDGETS
DEFINES        += -DUSE_GTK_WIDGETS
else
# No callback
NO_CALLBACK = 1
endif

ifdef USE_NSPR_IO
DEFINES        += -DUSE_NSPR_IO
USE_NSPR_BASE   = 1
NO_CALLBACK = 1
endif

ifdef USE_NSPR_BASE
DEFINES        += -DUSE_NSPR_BASE
LIBS           += $(NSPR_LIBS)
endif

ifdef NO_PTY
DEFINES        += -DNO_PTY
endif

ifdef NO_CALLBACK
DEFINES        += -DNO_CALLBACK
endif
