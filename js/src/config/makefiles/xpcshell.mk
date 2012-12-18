# -*- makefile -*-
# vim:set ts=8 sw=8 sts=8 noet:
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
#

ifndef INCLUDED_TESTS_XPCSHELL_MK #{

ifdef XPCSHELL_TESTS #{

ifndef relativesrcdir
$(error Must define relativesrcdir when defining XPCSHELL_TESTS.)
endif

define _INSTALL_TESTS
$(call install_cmd, $(filter-out %~,$(wildcard $(srcdir)/$(dir)/*)) $(testxpcobjdir)/$(relativesrcdir)/$(dir))

endef # do not remove the blank line!

SOLO_FILE ?= $(error Specify a test filename in SOLO_FILE when using check-interactive or check-one)

testxpcsrcdir = $(topsrcdir)/testing/xpcshell

libs:: libs-xpcshell-tests

###########################################################################
libs-xpcshell-tests:
	$(foreach dir,$(XPCSHELL_TESTS),$(_INSTALL_TESTS))
ifndef NO_XPCSHELL_MANIFEST_CHECK #{
	$(PYTHON) $(MOZILLA_DIR)/build/xpccheck.py \
	  $(topsrcdir) \
	  $(topsrcdir)/testing/xpcshell/xpcshell.ini \
	  $(addprefix $(MOZILLA_DIR)/$(relativesrcdir)/,$(XPCSHELL_TESTS))
endif #} NO_XPCSHELL_MANIFEST_CHECK 

###########################################################################
# Execute all tests in the $(XPCSHELL_TESTS) directories.
# See also testsuite-targets.mk 'xpcshell-tests' target for global execution.
xpcshell-tests:
	$(PYTHON) -u $(topsrcdir)/config/pythonpath.py \
	  -I$(topsrcdir)/build \
      -I$(DEPTH)/_tests/mozbase/mozinfo \
	  $(testxpcsrcdir)/runxpcshelltests.py \
	  --symbols-path=$(DIST)/crashreporter-symbols \
	  --build-info-json=$(DEPTH)/mozinfo.json \
	  --tests-root-dir=$(testxpcobjdir) \
	  --testing-modules-dir=$(DEPTH)/_tests/modules \
	  --xunit-file=$(testxpcobjdir)/$(relativesrcdir)/results.xml \
	  --xunit-suite-name=xpcshell \
	  --test-plugin-path=$(DIST)/plugins \
	  $(EXTRA_TEST_ARGS) \
	  $(LIBXUL_DIST)/bin/xpcshell \
	  $(foreach dir,$(XPCSHELL_TESTS),$(testxpcobjdir)/$(relativesrcdir)/$(dir))

xpcshell-tests-remote: DM_TRANS?=adb
xpcshell-tests-remote:
	$(PYTHON) -u $(topsrcdir)/config/pythonpath.py \
	  -I$(topsrcdir)/build \
	  -I$(topsrcdir)/build/mobile \
	  -I$(topsrcdir)/testing/mozbase/mozdevice/mozdevice \
	  $(topsrcdir)/testing/xpcshell/remotexpcshelltests.py \
	  --symbols-path=$(DIST)/crashreporter-symbols \
	  --build-info-json=$(DEPTH)/mozinfo.json \
	  --testing-modules-dir=$(DEPTH)/_tests/modules \
	  $(EXTRA_TEST_ARGS) \
	  --dm_trans=$(DM_TRANS) \
	  --deviceIP=${TEST_DEVICE} \
	  --objdir=$(DEPTH) \
	  $(foreach dir,$(XPCSHELL_TESTS),$(testxpcobjdir)/$(relativesrcdir)/$(dir))

###########################################################################
# Execute a single test, specified in $(SOLO_FILE), but don't automatically
# start the test. Instead, present the xpcshell prompt so the user can
# attach a debugger and then start the test.
check-interactive:
	$(PYTHON) -u $(topsrcdir)/config/pythonpath.py \
	  -I$(topsrcdir)/build \
      -I$(DEPTH)/_tests/mozbase/mozinfo \
	  $(testxpcsrcdir)/runxpcshelltests.py \
	  --symbols-path=$(DIST)/crashreporter-symbols \
	  --build-info-json=$(DEPTH)/mozinfo.json \
	  --test-path=$(SOLO_FILE) \
	  --testing-modules-dir=$(DEPTH)/_tests/modules \
	  --profile-name=$(MOZ_APP_NAME) \
	  --test-plugin-path=$(DIST)/plugins \
	  --interactive \
	  $(LIBXUL_DIST)/bin/xpcshell \
	  $(foreach dir,$(XPCSHELL_TESTS),$(testxpcobjdir)/$(relativesrcdir)/$(dir))

# Execute a single test, specified in $(SOLO_FILE)
check-one:
	$(PYTHON) -u $(topsrcdir)/config/pythonpath.py \
	  -I$(topsrcdir)/build \
      -I$(DEPTH)/_tests/mozbase/mozinfo \
	  $(testxpcsrcdir)/runxpcshelltests.py \
	  --symbols-path=$(DIST)/crashreporter-symbols \
	  --build-info-json=$(DEPTH)/mozinfo.json \
	  --test-path=$(SOLO_FILE) \
	  --testing-modules-dir=$(DEPTH)/_tests/modules \
	  --profile-name=$(MOZ_APP_NAME) \
	  --test-plugin-path=$(DIST)/plugins \
	  --verbose \
	  $(EXTRA_TEST_ARGS) \
	  $(LIBXUL_DIST)/bin/xpcshell \
	  $(foreach dir,$(XPCSHELL_TESTS),$(testxpcobjdir)/$(relativesrcdir)/$(dir))

check-one-remote: DM_TRANS?=adb
check-one-remote:
	$(PYTHON) -u $(topsrcdir)/config/pythonpath.py \
	  -I$(topsrcdir)/build \
	  -I$(topsrcdir)/build/mobile \
	  -I$(topsrcdir)/testing/mozbase/mozdevice/mozdevice \
	  $(testxpcsrcdir)/remotexpcshelltests.py \
	  --symbols-path=$(DIST)/crashreporter-symbols \
	  --build-info-json=$(DEPTH)/mozinfo.json \
	  --test-path=$(SOLO_FILE) \
	  --testing-modules-dir=$(DEPTH)/_tests/modules \
	  --profile-name=$(MOZ_APP_NAME) \
	  --verbose \
	  $(EXTRA_TEST_ARGS) \
	  --dm_trans=$(DM_TRANS) \
	  --deviceIP=${TEST_DEVICE} \
	  --objdir=$(DEPTH) \
          --noSetup \
	  $(foreach dir,$(XPCSHELL_TESTS),$(testxpcobjdir)/$(relativesrcdir)/$(dir))


.PHONY: xpcshell-tests check-interactive check-one libs-xpcshell-tests

endif #} XPCSHELL_TESTS

INCLUDED_TESTS_XPCSHELL_MK = 1
endif #} INCLUDED_TESTS_XPCSHELL_MK
