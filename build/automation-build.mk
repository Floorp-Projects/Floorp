# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

include $(MOZILLA_DIR)/build/binary-location.mk

browser_path := '"$(browser_path)"'

_PROFILE_DIR = $(TARGET_DEPTH)/_profile/pgo

ABSOLUTE_TOPSRCDIR = $(abspath $(MOZILLA_DIR))
_CERTS_SRC_DIR = $(ABSOLUTE_TOPSRCDIR)/build/pgo/certs

AUTOMATION_PPARGS = 	\
			-DBROWSER_PATH=$(browser_path) \
			-DXPC_BIN_PATH='"$(LIBXUL_DIST)/bin"' \
			-DBIN_SUFFIX='"$(BIN_SUFFIX)"' \
			-DPROFILE_DIR='"$(_PROFILE_DIR)"' \
			-DCERTS_SRC_DIR='"$(_CERTS_SRC_DIR)"' \
			-DPERL='"$(PERL)"' \
			$(NULL)

ifeq ($(OS_ARCH),Darwin)
AUTOMATION_PPARGS += -DIS_MAC=1
else
AUTOMATION_PPARGS += -DIS_MAC=0
endif

ifeq ($(OS_ARCH),Linux)
AUTOMATION_PPARGS += -DIS_LINUX=1
else
AUTOMATION_PPARGS += -DIS_LINUX=0
endif

ifeq ($(host_os), cygwin)
AUTOMATION_PPARGS += -DIS_CYGWIN=1
endif

ifeq ($(ENABLE_TESTS), 1)
AUTOMATION_PPARGS += -DIS_TEST_BUILD=1
else
AUTOMATION_PPARGS += -DIS_TEST_BUILD=0
endif

ifeq ($(MOZ_DEBUG), 1)
AUTOMATION_PPARGS += -DIS_DEBUG_BUILD=1
else
AUTOMATION_PPARGS += -DIS_DEBUG_BUILD=0
endif

ifdef MOZ_CRASHREPORTER
AUTOMATION_PPARGS += -DCRASHREPORTER=1
else
AUTOMATION_PPARGS += -DCRASHREPORTER=0
endif

ifdef MOZ_ASAN
AUTOMATION_PPARGS += -DIS_ASAN=1
else
AUTOMATION_PPARGS += -DIS_ASAN=0
endif

automation.py: $(MOZILLA_DIR)/build/automation.py.in $(MOZILLA_DIR)/build/automation-build.mk
	$(call py_action,preprocessor, \
	$(AUTOMATION_PPARGS) $(DEFINES) $(ACDEFINES) $< -o $@)

GARBAGE += automation.py automation.pyc
