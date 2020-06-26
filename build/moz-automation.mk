#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

ifndef ENABLE_TESTS
# We can't package tests if they aren't enabled.
MOZ_AUTOMATION_PACKAGE_TESTS = 0
endif

ifdef CROSS_COMPILE
# Narrow the definition of cross compilation to not include win32 builds
# on win64 and linux32 builds on linux64.
ifeq ($(HOST_OS_ARCH),$(OS_TARGET))
ifneq (,$(filter x86%,$(CPU_ARCH)))
FUZZY_CROSS_COMPILE =
else
FUZZY_CROSS_COMPILE = 1
endif
else
FUZZY_CROSS_COMPILE = 1
endif
endif

# Don't run make check when cross compiling, when doing artifact builds
# or when building instrumented builds for PGO.
ifneq (,$(USE_ARTIFACT)$(FUZZY_CROSS_COMPILE)$(MOZ_PROFILE_GENERATE))
MOZ_AUTOMATION_CHECK := 0
endif

ifneq (,$(filter automation/%,$(MAKECMDGOALS)))
ifeq (4.0,$(firstword $(sort 4.0 $(MAKE_VERSION))))
MAKEFLAGS += --output-sync=target
else
.NOTPARALLEL:
endif
endif

ifndef JS_STANDALONE
include $(topsrcdir)/toolkit/mozapps/installer/package-name.mk
include $(topsrcdir)/toolkit/mozapps/installer/upload-files.mk

# Clear out DIST_FILES if it was set by upload-files.mk (for Android builds)
DIST_FILES =
endif

# Helper variables to convert from MOZ_AUTOMATION_* variables to the
# corresponding the make target
tier_MOZ_AUTOMATION_BUILD_SYMBOLS = buildsymbols
tier_MOZ_AUTOMATION_PACKAGE = package
tier_MOZ_AUTOMATION_PACKAGE_TESTS = package-tests
tier_MOZ_AUTOMATION_PACKAGE_GENERATED_SOURCES = package-generated-sources
tier_MOZ_AUTOMATION_UPLOAD_SYMBOLS = uploadsymbols
tier_MOZ_AUTOMATION_UPLOAD = upload
tier_MOZ_AUTOMATION_CHECK = check

# Automation build steps. Everything in MOZ_AUTOMATION_TIERS also gets used in
# TIERS for mach display. As such, the MOZ_AUTOMATION_TIERS are roughly sorted
# here in the order that they will be executed (since mach doesn't know of the
# dependencies between them).
moz_automation_symbols = \
  MOZ_AUTOMATION_PACKAGE_TESTS \
  MOZ_AUTOMATION_BUILD_SYMBOLS \
  MOZ_AUTOMATION_UPLOAD_SYMBOLS \
  MOZ_AUTOMATION_PACKAGE \
  MOZ_AUTOMATION_PACKAGE_GENERATED_SOURCES \
  MOZ_AUTOMATION_UPLOAD \
  MOZ_AUTOMATION_CHECK \
  $(NULL)
MOZ_AUTOMATION_TIERS := $(foreach sym,$(moz_automation_symbols),$(if $(filter 1,$($(sym))),$(tier_$(sym))))

# Dependencies between automation build steps
automation-start/uploadsymbols: automation/buildsymbols

automation-start/upload: automation/package
automation-start/upload: automation/package-tests
automation-start/upload: automation/buildsymbols
automation-start/upload: automation/package-generated-sources

# Run the check tier after everything else.
automation-start/check: $(addprefix automation/,$(filter-out check,$(MOZ_AUTOMATION_TIERS)))

automation/build: $(addprefix automation/,$(MOZ_AUTOMATION_TIERS))
	@echo Automation steps completed.

# Run as many tests as possible, even in case of one of them failing.
AUTOMATION_EXTRA_CMDLINE-check = --keep-going

# The commands only run if the corresponding MOZ_AUTOMATION_* variable is
# enabled. This means, for example, if we enable MOZ_AUTOMATION_UPLOAD, then
# 'buildsymbols' will only run if MOZ_AUTOMATION_BUILD_SYMBOLS is also set.
# However, the target automation/buildsymbols will still be executed in this
# case because it is a prerequisite of automation/upload.
define automation_commands
@+$(PYTHON3) $(topsrcdir)/config/run-and-prefix.py $1 $(MAKE) $1 $(AUTOMATION_EXTRA_CMDLINE-$1)
$(call BUILDSTATUS,TIER_FINISH $1)
endef

# The tier start message is in a separate target so make doesn't buffer it
# until the step completes with output syncing enabled.
automation-start/%:
	$(if $(filter $*,$(MOZ_AUTOMATION_TIERS)),$(call BUILDSTATUS,TIER_START $*))

automation/%: automation-start/%
	$(if $(filter $*,$(MOZ_AUTOMATION_TIERS)),$(call automation_commands,$*))
