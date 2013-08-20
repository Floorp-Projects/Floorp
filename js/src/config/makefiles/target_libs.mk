# -*- makefile -*-
# vim:set ts=8 sw=8 sts=8 noet:
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

PARALLEL_DIRS_libs = $(addsuffix _libs,$(PARALLEL_DIRS))

.PHONY: libs $(PARALLEL_DIRS_libs)

###############
## TIER targets
###############
libs_tier_%:
	@$(ECHO) "$@"
	$(foreach dir,$(tier_$*_dirs),$(call TIER_DIR_SUBMAKE,libs,$(dir)))

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

libs:: $(SUBMAKEFILES) $(MAKE_DIRS) $(HOST_LIBRARY) $(LIBRARY) $(SHARED_LIBRARY) $(IMPORT_LIBRARY) $(HOST_PROGRAM) $(HOST_SIMPLE_PROGRAMS) $(SIMPLE_PROGRAMS)
ifndef NO_DIST_INSTALL
ifdef SHARED_LIBRARY
ifdef IS_COMPONENT
	$(INSTALL) $(IFLAGS2) $(SHARED_LIBRARY) $(FINAL_TARGET)/components
	$(ELF_DYNSTR_GC) $(FINAL_TARGET)/components/$(SHARED_LIBRARY)
ifndef NO_COMPONENTS_MANIFEST
	@$(PYTHON) $(MOZILLA_DIR)/config/buildlist.py $(FINAL_TARGET)/chrome.manifest "manifest components/components.manifest"
	@$(PYTHON) $(MOZILLA_DIR)/config/buildlist.py $(FINAL_TARGET)/components/components.manifest "binary-component $(SHARED_LIBRARY)"
endif
endif # IS_COMPONENT
endif # SHARED_LIBRARY
endif # !NO_DIST_INSTALL
	$(LOOP_OVER_DIRS)

ifndef NO_DIST_INSTALL

ifneq (,$(strip $(PROGRAM)$(SIMPLE_PROGRAMS)))
PROGRAMS_EXECUTABLES = $(SIMPLE_PROGRAMS) $(PROGRAM)
PROGRAMS_DEST ?= $(FINAL_TARGET)
INSTALL_TARGETS += PROGRAMS
endif

ifdef LIBRARY
ifdef EXPORT_LIBRARY
LIBRARY_FILES = $(LIBRARY)
LIBRARY_DEST ?= $(EXPORT_LIBRARY)
INSTALL_TARGETS += LIBRARY
endif
ifdef DIST_INSTALL
ifdef IS_COMPONENT
$(error Shipping static component libs makes no sense.)
else
DIST_LIBRARY_FILES = $(LIBRARY)
DIST_LIBRARY_DEST ?= $(DIST)/lib
INSTALL_TARGETS += DIST_LIBRARY
endif
endif # DIST_INSTALL
endif # LIBRARY


ifdef SHARED_LIBRARY
ifndef IS_COMPONENT
SHARED_LIBRARY_FILES = $(SHARED_LIBRARY)
SHARED_LIBRARY_DEST ?= $(FINAL_TARGET)
INSTALL_TARGETS += SHARED_LIBRARY

ifneq (,$(filter OS2 WINNT,$(OS_ARCH)))
ifndef NO_INSTALL_IMPORT_LIBRARY
IMPORT_LIB_FILES = $(IMPORT_LIBRARY)
endif # NO_INSTALL_IMPORT_LIBRARY
else
IMPORT_LIB_FILES = $(SHARED_LIBRARY)
endif

IMPORT_LIB_DEST ?= $(DIST)/lib
ifdef IMPORT_LIB_FILES
INSTALL_TARGETS += IMPORT_LIB
endif

endif # ! IS_COMPONENT
endif # SHARED_LIBRARY

ifneq (,$(strip $(HOST_SIMPLE_PROGRAMS)$(HOST_PROGRAM)))
HOST_PROGRAMS_EXECUTABLES = $(HOST_SIMPLE_PROGRAMS) $(HOST_PROGRAM)
HOST_PROGRAMS_DEST ?= $(DIST)/host/bin
INSTALL_TARGETS += HOST_PROGRAMS
endif

ifdef HOST_LIBRARY
HOST_LIBRARY_FILES = $(HOST_LIBRARY)
HOST_LIBRARY_DEST ?= $(DIST)/host/lib
INSTALL_TARGETS += HOST_LIBRARY
endif

endif # !NO_DIST_INSTALL

# EOF
