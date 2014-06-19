#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

ifneq (,$(filter automation/%,$(MAKECMDGOALS)))
ifeq (4.0,$(firstword $(sort 4.0 $(MAKE_VERSION))))
MAKEFLAGS += --output-sync=target
else
.NOTPARALLEL:
endif
endif

include $(topsrcdir)/toolkit/mozapps/installer/package-name.mk

# Log file from the 'make upload' step. We need this to parse out the URLs of
# the uploaded files.
AUTOMATION_UPLOAD_OUTPUT = $(DIST)/automation-upload.txt

# Automation build steps. Everything in MOZ_AUTOMATION_TIERS also gets used in
# TIERS for mach display. As such, the MOZ_AUTOMATION_TIERS are roughly sorted
# here in the order that they will be executed (since mach doesn't know of the
# dependencies between them).

MOZ_AUTOMATION_TIERS += package-tests
MOZ_AUTOMATION_TIERS += buildsymbols

ifdef NIGHTLY_BUILD
MOZ_AUTOMATION_TIERS += uploadsymbols
endif

ifdef MOZ_UPDATE_PACKAGING
MOZ_AUTOMATION_TIERS += update-packaging
automation/upload: automation/update-packaging
automation/update-packaging: automation/package
endif

MOZ_AUTOMATION_TIERS += package
MOZ_AUTOMATION_TIERS += check

ifndef MOZ_ASAN
MOZ_AUTOMATION_TIERS += l10n-check
automation/l10n-check: automation/package
endif

MOZ_AUTOMATION_TIERS += upload

automation/build: $(addprefix automation/,$(MOZ_AUTOMATION_TIERS))
	$(PYTHON) $(topsrcdir)/build/gen_mach_buildprops.py --complete-mar-file $(DIST)/$(COMPLETE_MAR) --upload-output $(AUTOMATION_UPLOAD_OUTPUT)

# Dependencies between automation build steps
automation/uploadsymbols: automation/buildsymbols

automation/upload: automation/package
automation/upload: automation/package-tests
automation/upload: automation/buildsymbols

# automation/package and automation/check should depend on build (which is
# implicit due to the way client.mk invokes automation/build), but buildsymbols
# changes the binaries/libs, and that's what we package/test.
automation/package: automation/buildsymbols
automation/check: automation/buildsymbols

# make check runs with the keep-going flag so we can see all the failures
AUTOMATION_EXTRA_CMDLINE-check = -k

# We need the log from make upload to grep it for urls in order to set
# properties.
AUTOMATION_EXTRA_CMDLINE-upload = 2>&1 | tee $(AUTOMATION_UPLOAD_OUTPUT)

automation/%:
	$(call BUILDSTATUS,TIER_START $*)
	@$(MAKE) $* $(AUTOMATION_EXTRA_CMDLINE-$*)
	$(call BUILDSTATUS,TIER_FINISH $*)
