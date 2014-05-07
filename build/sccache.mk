# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

ifdef OBJDIR
BASE_DIR = $(OBJDIR)
else
# OSX Universal builds only do upload in the first MOZ_BUILD_PROJECTS
BASE_DIR = $(MOZ_OBJDIR)/$(firstword $(MOZ_BUILD_PROJECTS))
endif

preflight_all:
	# Terminate any sccache server that might still be around
	-python2.7 $(TOPSRCDIR)/sccache/sccache.py > /dev/null 2>&1

postflight_all:
	# Terminate sccache server. This prints sccache stats.
	-python2.7 $(TOPSRCDIR)/sccache/sccache.py 2>&1 | gzip > $(BASE_DIR)/dist/sccache.log.gz
