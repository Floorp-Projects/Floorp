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

### Set up make flags
ifdef MOZ_AUTOMATION
ifeq (4.0,$(firstword $(sort 4.0 $(MAKE_VERSION))))
MOZ_MAKE_FLAGS += --output-sync=line
endif
endif

MOZ_MAKE = $(MAKE) $(MOZ_MAKE_FLAGS) -C $(OBJDIR)

ifdef MOZBUILD_MANAGE_SCCACHE_DAEMON
# In automation, manage an sccache daemon. The starting of the server
# needs to be in a make file so sccache inherits the jobserver.
SCCACHE_STOP = $(MOZBUILD_MANAGE_SCCACHE_DAEMON) --stop-server

# When a command fails, make is going to abort, but we need to terminate the
# sccache server, otherwise it will prevent make itself from terminating
# because it would still be running and holding a jobserver token.
# However, we also need to preserve the command's exit code, thus the
# gymnastics.
SCCACHE_STOP_ON_FAILURE = || (x=$$?; $(SCCACHE_STOP) || true; exit $$x)
endif

# The default rule is build
build:
ifdef MOZBUILD_MANAGE_SCCACHE_DAEMON
	# Terminate any sccache server that might still be around.
	-$(SCCACHE_STOP) > /dev/null 2>&1
	# Start a new server, ensuring it gets the jobserver file descriptors
	# from make (but don't use the + prefix when make -n is used, so that
	# the command doesn't run in that case)
	mkdir -p $(UPLOAD_PATH)
	$(if $(findstring n,$(filter-out --%, $(MAKEFLAGS))),,+)env SCCACHE_LOG=sccache=debug SCCACHE_ERROR_LOG=$(UPLOAD_PATH)/sccache.log $(MOZBUILD_MANAGE_SCCACHE_DAEMON) --start-server
endif
	### Build it
	+$(MOZ_MAKE) $(SCCACHE_STOP_ON_FAILURE)
ifdef MOZ_AUTOMATION
	+$(MOZ_MAKE) automation/build $(SCCACHE_STOP_ON_FAILURE)
endif
ifdef MOZBUILD_MANAGE_SCCACHE_DAEMON
	# Terminate sccache server. This prints sccache stats.
	-$(SCCACHE_STOP)
endif

.PHONY: \
    build
