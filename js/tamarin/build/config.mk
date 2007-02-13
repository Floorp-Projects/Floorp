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
# The Original Code is [Open Source Virtual Machine].
#
# The Initial Developer of the Original Code is
# Adobe System Incorporated.
# Portions created by the Initial Developer are Copyright (C) 2005-2006
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

GARBAGE :=

VPATH = $(topsrcdir)

curdir := .
srcdir := $(topsrcdir)

COMPILE_CPPFLAGS = $(CPPFLAGS) $(APP_CPPFLAGS)
COMPILE_CXXFLAGS = $(CXXFLAGS) $(APP_CXXFLAGS)

ifdef ENABLE_DEBUG
COMPILE_CPPFLAGS += $(DEBUG_CPPFLAGS)
COMPILE_CXXFLAGS += $(DEBUG_CXXFLAGS)
else
COMPILE_CPPFLAGS += $(OPT_CPPFLAGS)
COMPILE_CXXFLAGS += $(OPT_CXXFLAGS)
endif

all::

# Usage: from within RECURSE_DIRS
# Variables:
#   $(1) = $(curdir)
#   $(2) = subdirectory
# Note: positional parameters passed to $(call) are expanded "early". This is
# the magic that sets and revert $(curdir) and $(srcdir) properly.
define RECURSE_DIR
curdir := $(1)/$(2)
srcdir := $(topsrcdir)/$$(curdir)
include $(topsrcdir)/$$(curdir)/manifest.mk
curdir := $(1)
endef

# Usage: $(call REAL_RECURSE_DIRS,$(DIRS),$(curdir))
define REAL_RECURSE_DIRS
$(foreach dir,$(1),$(eval $(call RECURSE_DIR,$(2),$(dir))))
endef

# Usage: $(call RECURSE_DIRS,$(DIRS)))
define RECURSE_DIRS
$(call REAL_RECURSE_DIRS,$(1),$(curdir))
endef

# Usage: $(eval $(call THING_SRCS,$(thingname)))
define THING_SRCS
$(1)_CPPFLAGS ?= $(COMPILE_CPPFLAGS)
$(1)_CXXFLAGS ?= $(COMPILE_CXXFLAGS)
$(1)_CPPFLAGS += $($(1)_EXTRA_CPPFLAGS)
$(1)_CXXFLAGS += $($(1)_EXTRA_CXXFLAGS)
$(1)_INCLUDES += $(INCLUDES)
$(1)_DEFINES += $(DEFINES)

$(1)_CXXOBJS = $$($(1)_CXXSRCS:%.cpp=%.$(OBJ_SUFFIX))

GARBAGE += \
  $$($(1)_CXXOBJS) \
  $$($(1)_CXXOBJS:.$(OBJ_SUFFIX)=.ii) \
  $$($(1)_CXXOBJS:.$(OBJ_SUFFIX)=.deps) \
  $(NULL)

ifndef PP_PRECIOUS
.INTERMEDIATE: $$($(1)_CXXOBJS:.$(OBJ_SUFFIX)=.ii)
endif

# XXX Add dependency handling!
$$($(1)_CXXOBJS:.$(OBJ_SUFFIX)=.ii): %.ii: %.cpp	
	test -d $$(dir $$@) || mkdir -p $$(dir $$@)
	$(CXX) -E $$($(1)_CPPFLAGS) $$($(1)_CXXFLAGS) $$($(1)_DEFINES) $$($(1)_INCLUDES) \
	  $$< > $$@
	$(PYTHON) $(topsrcdir)/build/dependparser.py $$*.deps < $$@ > /dev/null

$$($(1)_CXXOBJS): %.$(OBJ_SUFFIX): %.ii
	$(CXX) -o $$@ $$($(1)_CPPFLAGS) $$($(1)_CXXFLAGS) $$($(1)_DEFINES) $$($(1)_INCLUDES) -c $$<

$(1).thing.pp: FORCE
	@echo Building $$@
	@$(PYTHON) $(topsrcdir)/build/calcdepends.py $$@ $$($(1)_CXXOBJS:$(OBJ_SUFFIX)=.ii)

include $(1).thing.pp

endef # THINGS_SRCS

# Usage: $(eval $(call STATIC_LIBRARY_RULES,$(static_library_thingname)))
define STATIC_LIBRARY_RULES
  $(1)_BASENAME ?= $(1)
  $(1)_NAME = $(LIB_PREFIX)$$($(1)_BASENAME).$(LIB_SUFFIX)

$$($(1)_DIR)$$($(1)_NAME): $$($(1)_CXXOBJS)
	$(call MKSTATICLIB,$$@) $$($(1)_CXXOBJS)

GARBAGE += $$($(1)_DIR)$$($(1)_NAME)

ifdef $(1)_BUILD_ALL
all:: $$($(1)_DIR)$$($(1)_NAME)
endif

endef

# Usage: $(eval $(call PROGRAM_RULES,$(program_thingnaame)))
define PROGRAM_RULES
  $(1)_BASENAME ?= $(1)
  $(1)_NAME ?= $$($(1)_BASENAME)$(BIN_SUFFIX)
  $(1)_DEPS = \
    $$($(1)_EXTRA_DEPS) \
    $$(foreach lib,$$($(1)_STATIC_LIBRARIES),$$($$(lib)_NAME)) \
    $(NULL)
  $(1)_LDFLAGS = \
    $$(LDFLAGS) \
    $$(foreach lib,$$(OS_LIBS),$(call EXPAND_LIBNAME,$$(lib))) \
    $$(OS_LDFLAGS) \
    $(NULL)

$$($(1)_DIR)$$($(1)_NAME): $$($(1)_CXXOBJS) $$($(1)_DEPS)
	$(call MKPROGRAM,$$@) \
	  $$($(1)_CXXOBJS) \
	  -L. $$(foreach lib,$$($(1)_STATIC_LIBRARIES),-l$$(lib)) \
	  $$($(1)_LDFLAGS)

GARBAGE += $$($(1)_DIR)$$($(1)_NAME)

ifdef $(1)_BUILD_ALL
all:: $$($(1)_DIR)$$($(1)_NAME)
endif

endef

.PHONY: all FORCE
