# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

preflight_all:
	# Terminate any sccache server that might still be around
	-$(TOPSRCDIR)/sccache2/sccache --stop-server > /dev/null 2>&1
	# Start a new server, ensuring it gets the jobserver file descriptors
	# from make (but don't use the + prefix when make -n is used, so that
	# the command doesn't run in that case)
	$(if $(findstring n,$(filter-out --%, $(MAKEFLAGS))),,+)env RUST_LOG=sccache::compiler=debug SCCACHE_ERROR_LOG=$(OBJDIR)/dist/sccache.log $(TOPSRCDIR)/sccache2/sccache --start-server

postflight_all:
	# Terminate sccache server. This prints sccache stats.
	-$(TOPSRCDIR)/sccache2/sccache --stop-server
