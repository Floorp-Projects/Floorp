# -*- makefile -*-
# vim:set ts=8 sw=8 sts=8 noet:
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

ifndef NO_DIST_INSTALL

ifneq (,$(strip $(PROGRAM)$(SIMPLE_PROGRAMS)$(RUST_PROGRAMS)))
PROGRAMS_EXECUTABLES = $(SIMPLE_PROGRAMS) $(PROGRAM) $(RUST_PROGRAMS)
PROGRAMS_DEST ?= $(FINAL_TARGET)
PROGRAMS_TARGET := target
INSTALL_TARGETS += PROGRAMS
endif

ifdef LIBRARY
ifdef DIST_INSTALL
ifdef IS_COMPONENT
$(error Shipping static component libs makes no sense.)
endif
endif # DIST_INSTALL
endif # LIBRARY


ifdef SHARED_LIBRARY
SHARED_LIBRARY_FILES = $(SHARED_LIBRARY)
SHARED_LIBRARY_DEST ?= $(FINAL_TARGET)$(if $(IS_COMPONENT),/components)
SHARED_LIBRARY_TARGET = target
INSTALL_TARGETS += SHARED_LIBRARY
endif # SHARED_LIBRARY

ifneq (,$(strip $(HOST_SIMPLE_PROGRAMS)$(HOST_PROGRAM)))
HOST_PROGRAMS_EXECUTABLES = $(HOST_SIMPLE_PROGRAMS) $(HOST_PROGRAM) $(HOST_RUST_PROGRAMS)
HOST_PROGRAMS_DEST ?= $(DIST)/host/bin
HOST_PROGRAMS_TARGET = host
INSTALL_TARGETS += HOST_PROGRAMS
endif

endif # !NO_DIST_INSTALL

# EOF
