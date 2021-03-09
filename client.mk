# -*- makefile -*-
# vim:set ts=8 sw=8 sts=8 noet:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Defines main targets for driving the Firefox build system.
#
# This make file should not be invoked directly. Instead, use
# `mach` (likely `mach build`) for invoking the build system.
#
# Options:
#   MOZ_OBJDIR           - Destination object directory
#   MOZ_MAKE_FLAGS       - Flags to pass to $(MAKE)
#
#######################################################################

ifndef MACH
$(error client.mk must be used via `mach`. Try running \
`./mach $(firstword $(MAKECMDGOALS) $(.DEFAULT_GOAL))`)
endif

### Load mozconfig options
include $(OBJDIR)/.mozconfig-client-mk

ifdef MOZ_PARALLEL_BUILD
  MOZ_MAKE_FLAGS := $(filter-out -j%,$(MOZ_MAKE_FLAGS))
  MOZ_MAKE_FLAGS += -j$(MOZ_PARALLEL_BUILD)
endif

# Automatically add -jN to make flags if not defined. N defaults to number of cores.
ifeq (,$(findstring -j,$(MOZ_MAKE_FLAGS)))
  cores=$(shell python3 -c 'import multiprocessing; print(multiprocessing.cpu_count())')
  MOZ_MAKE_FLAGS += -j$(cores)
endif

### Set up make flags
ifdef MOZ_AUTOMATION
ifeq (4.0,$(firstword $(sort 4.0 $(MAKE_VERSION))))
MOZ_MAKE_FLAGS += --output-sync=line
endif
endif

MOZ_MAKE = $(MAKE) $(MOZ_MAKE_FLAGS) -C $(OBJDIR)

### Rules
# The default rule is build
build::

# In automation, manage an sccache daemon. The starting of the server
# needs to be in a make file so sccache inherits the jobserver.
ifdef MOZBUILD_MANAGE_SCCACHE_DAEMON
build::
	# Terminate any sccache server that might still be around.
	-$(MOZBUILD_MANAGE_SCCACHE_DAEMON) --stop-server > /dev/null 2>&1
	# Start a new server, ensuring it gets the jobserver file descriptors
	# from make (but don't use the + prefix when make -n is used, so that
	# the command doesn't run in that case)
	mkdir -p $(UPLOAD_PATH)
	$(if $(findstring n,$(filter-out --%, $(MAKEFLAGS))),,+)env SCCACHE_LOG=sccache=debug SCCACHE_ERROR_LOG=$(UPLOAD_PATH)/sccache.log $(MOZBUILD_MANAGE_SCCACHE_DAEMON) --start-server
endif

### Build it
build::
	+$(MOZ_MAKE)

ifdef MOZ_AUTOMATION
build::
	+$(MOZ_MAKE) automation/build
endif

# This makefile doesn't support parallel execution. It does pass
# MOZ_MAKE_FLAGS to sub-make processes, so they will correctly execute
# in parallel.
.NOTPARALLEL:

.PHONY: \
    build
