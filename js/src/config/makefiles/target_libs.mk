# -*- makefile -*-
# vim:set ts=8 sw=8 sts=8 noet:
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
# The Mozilla Foundation
# Portions created by the Initial Developer are Copyright (C) 2011
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Chase Phillips <chase@mozilla.org>
#  Benjamin Smedberg <benjamin@smedbergs.us>
#  Jeff Walden <jwalden+code@mit.edu>
#  Joey Armstrong <joey@mozilla.com>
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

PARALLEL_DIRS_libs = $(addsuffix _libs,$(PARALLEL_DIRS))

.PHONY: libs $(PARALLEL_DIRS_libs)

###############
## TIER targets
###############
libs_tier_%:
	@$(ECHO) "$@"
	@$(MAKE_TIER_SUBMAKEFILES)
	$(foreach dir,$(tier_$*_dirs),$(call SUBMAKE,libs,$(dir)))

#################
## Common targets
#################
ifdef PARALLEL_DIRS
libs:: $(PARALLEL_DIRS_libs)

$(PARALLEL_DIRS_libs): %_libs: %/Makefile
	+@$(call SUBMAKE,libs,$*)
endif


####################
##
####################
ifdef EXPORT_LIBRARY
ifeq ($(EXPORT_LIBRARY),1)
ifdef IS_COMPONENT
EXPORT_LIBRARY = $(DEPTH)/staticlib/components
else
EXPORT_LIBRARY = $(DEPTH)/staticlib
endif
else
# If EXPORT_LIBRARY has a value, we'll be installing there. We also need to cleanup there
GARBAGE += $(foreach lib,$(LIBRARY),$(EXPORT_LIBRARY)/$(lib))
endif
endif # EXPORT_LIBRARY

libs:: $(SUBMAKEFILES) $(MAKE_DIRS) $(HOST_LIBRARY) $(LIBRARY) $(SHARED_LIBRARY) $(IMPORT_LIBRARY) $(HOST_PROGRAM) $(PROGRAM) $(HOST_SIMPLE_PROGRAMS) $(SIMPLE_PROGRAMS) $(JAVA_LIBRARY)
ifndef NO_DIST_INSTALL
ifdef LIBRARY
ifdef EXPORT_LIBRARY # Stage libs that will be linked into a static build
	$(INSTALL) $(IFLAGS1) $(LIBRARY) $(EXPORT_LIBRARY)
endif # EXPORT_LIBRARY
ifdef DIST_INSTALL
ifdef IS_COMPONENT
	$(error Shipping static component libs makes no sense.)
else
	$(INSTALL) $(IFLAGS1) $(LIBRARY) $(DIST)/lib
endif
endif # DIST_INSTALL
endif # LIBRARY
ifdef SHARED_LIBRARY
ifdef IS_COMPONENT
	$(INSTALL) $(IFLAGS2) $(SHARED_LIBRARY) $(FINAL_TARGET)/components
	$(ELF_DYNSTR_GC) $(FINAL_TARGET)/components/$(SHARED_LIBRARY)
ifndef NO_COMPONENTS_MANIFEST
	@$(PYTHON) $(MOZILLA_DIR)/config/buildlist.py $(FINAL_TARGET)/chrome.manifest "manifest components/components.manifest"
	@$(PYTHON) $(MOZILLA_DIR)/config/buildlist.py $(FINAL_TARGET)/components/components.manifest "binary-component $(SHARED_LIBRARY)"
endif
else # ! IS_COMPONENT
ifneq (,$(filter OS2 WINNT,$(OS_ARCH)))
ifndef NO_INSTALL_IMPORT_LIBRARY
	$(INSTALL) $(IFLAGS2) $(IMPORT_LIBRARY) $(DIST)/lib
endif
else
	$(INSTALL) $(IFLAGS2) $(SHARED_LIBRARY) $(DIST)/lib
endif
	$(INSTALL) $(IFLAGS2) $(SHARED_LIBRARY) $(FINAL_TARGET)
endif # IS_COMPONENT
endif # SHARED_LIBRARY
ifdef PROGRAM
	$(INSTALL) $(IFLAGS2) $(PROGRAM) $(FINAL_TARGET)
endif
ifdef SIMPLE_PROGRAMS
	$(INSTALL) $(IFLAGS2) $(SIMPLE_PROGRAMS) $(FINAL_TARGET)
endif
ifdef HOST_PROGRAM
	$(INSTALL) $(IFLAGS2) $(HOST_PROGRAM) $(DIST)/host/bin
endif
ifdef HOST_SIMPLE_PROGRAMS
	$(INSTALL) $(IFLAGS2) $(HOST_SIMPLE_PROGRAMS) $(DIST)/host/bin
endif
ifdef HOST_LIBRARY
	$(INSTALL) $(IFLAGS1) $(HOST_LIBRARY) $(DIST)/host/lib
endif
ifdef JAVA_LIBRARY
ifdef IS_COMPONENT
	$(INSTALL) $(IFLAGS1) $(JAVA_LIBRARY) $(FINAL_TARGET)/components
else
	$(INSTALL) $(IFLAGS1) $(JAVA_LIBRARY) $(FINAL_TARGET)
endif
endif # JAVA_LIBRARY
endif # !NO_DIST_INSTALL
	$(LOOP_OVER_DIRS)

# EOF
