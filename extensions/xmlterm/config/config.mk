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

# config.mk: config info for stand-alone LineTerm only

# Options
#  DEBUG:          debug option

# OS details
OS_ARCH         := $(subst /,_,$(shell uname -s))
OS_RELEASE      := $(shell uname -r)

ifneq (,$(filter Linux SunOS,$(OS_ARCH)))
OS_VERS         := $(suffix $(OS_RELEASE))
OS_RELEASE      := $(basename $(OS_RELEASE))
endif

OS_CONFIG       := $(OS_ARCH)$(OS_RELEASE)

ifeq ($(OS_ARCH),Linux)
LIB_SUFFIX      = a
RANLIB          = /usr/bin/ranlib
endif

ifeq ($(OS_CONFIG),SunOS5)
LIB_SUFFIX      = a
RANLIB          = /bin/true
endif

ifeq ($(OS_ARCH),FreeBSD)
LIB_SUFFIX      = a
RANLIB          = /usr/bin/ranlib
endif

# C++ compiler
CCC          = $(CXX)

# OS flags
OS_CFLAGS   += $(PLATFORM_FLAGS)
OS_CXXFLAGS += $(PLATFORM_FLAGS)
OS_LDFLAGS  +=

# Object directory
OBJDIR       = lib

# Library directory
LIBS_DIR     = -L$(topsrcdir)/distrib/$(OBJDIR)

# Distribution includes
LOCAL_INCLUDES += -I$(topsrcdir)/distrib/include

# NSPR libraries
NSPR_LIBS = -lnspr3

#
# Debug option
#
ifdef DEBUG
OPTIMIZER    = -g
DEFINES     += -DDEBUG
else
OPTIMIZER    =
endif

INCLUDES     = $(LOCAL_INCLUDES) $(OS_INCLUDES)

CFLAGS       = $(OPTIMIZER) $(OS_CFLAGS) $(DEFINES) $(INCLUDES)
CXXFLAGS     = $(OPTIMIZER) $(OS_CXXFLAGS) $(DEFINES) $(INCLUDES)
LDFLAGS      = $(OS_LDFLAGS)
