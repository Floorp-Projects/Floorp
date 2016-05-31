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

ifndef JS_STANDALONE
include $(topsrcdir)/toolkit/mozapps/installer/package-name.mk
include $(topsrcdir)/toolkit/mozapps/installer/upload-files.mk

# Clear out DIST_FILES if it was set by upload-files.mk (for Android builds)
DIST_FILES =
endif

# Helper variables to convert from MOZ_AUTOMATION_* variables to the
# corresponding the make target
tier_MOZ_AUTOMATION_BUILD_SYMBOLS = buildsymbols
tier_MOZ_AUTOMATION_L10N_CHECK = l10n-check
tier_MOZ_AUTOMATION_PRETTY_L10N_CHECK = pretty-l10n-check
tier_MOZ_AUTOMATION_INSTALLER = installer
tier_MOZ_AUTOMATION_PRETTY_INSTALLER = pretty-installer
tier_MOZ_AUTOMATION_PACKAGE = package
tier_MOZ_AUTOMATION_PRETTY_PACKAGE = pretty-package
tier_MOZ_AUTOMATION_PACKAGE_TESTS = package-tests
tier_MOZ_AUTOMATION_PRETTY_PACKAGE_TESTS = pretty-package-tests
tier_MOZ_AUTOMATION_UPDATE_PACKAGING = update-packaging
tier_MOZ_AUTOMATION_PRETTY_UPDATE_PACKAGING = pretty-update-packaging
tier_MOZ_AUTOMATION_UPLOAD_SYMBOLS = uploadsymbols
tier_MOZ_AUTOMATION_UPLOAD = upload
tier_MOZ_AUTOMATION_SDK = sdk

# Automation build steps. Everything in MOZ_AUTOMATION_TIERS also gets used in
# TIERS for mach display. As such, the MOZ_AUTOMATION_TIERS are roughly sorted
# here in the order that they will be executed (since mach doesn't know of the
# dependencies between them).
moz_automation_symbols = \
  MOZ_AUTOMATION_PACKAGE_TESTS \
  MOZ_AUTOMATION_PRETTY_PACKAGE_TESTS \
  MOZ_AUTOMATION_BUILD_SYMBOLS \
  MOZ_AUTOMATION_UPLOAD_SYMBOLS \
  MOZ_AUTOMATION_PACKAGE \
  MOZ_AUTOMATION_PRETTY_PACKAGE \
  MOZ_AUTOMATION_INSTALLER \
  MOZ_AUTOMATION_PRETTY_INSTALLER \
  MOZ_AUTOMATION_UPDATE_PACKAGING \
  MOZ_AUTOMATION_PRETTY_UPDATE_PACKAGING \
  MOZ_AUTOMATION_L10N_CHECK \
  MOZ_AUTOMATION_PRETTY_L10N_CHECK \
  MOZ_AUTOMATION_UPLOAD \
  MOZ_AUTOMATION_SDK \
  $(NULL)
MOZ_AUTOMATION_TIERS := $(foreach sym,$(moz_automation_symbols),$(if $(filter 1,$($(sym))),$(tier_$(sym))))

# Dependencies between automation build steps
automation/uploadsymbols: automation/buildsymbols

automation/update-packaging: automation/package
automation/update-packaging: automation/installer
automation/pretty-update-packaging: automation/pretty-package
automation/pretty-update-packaging: automation/pretty-installer

automation/l10n-check: automation/package
automation/l10n-check: automation/installer
automation/pretty-l10n-check: automation/pretty-package
automation/pretty-l10n-check: automation/pretty-installer

automation/upload: automation/installer
automation/upload: automation/package
automation/upload: automation/package-tests
automation/upload: automation/buildsymbols
automation/upload: automation/update-packaging
automation/upload: automation/sdk

# automation/{pretty-}package should depend on build (which is implicit due to
# the way client.mk invokes automation/build), but buildsymbols changes the
# binaries/libs, and that's what we package/test.
automation/pretty-package: automation/buildsymbols

# The installer, sdk and packager all run stage-package, and may conflict
# with each other.
automation/installer: automation/package
automation/sdk: automation/installer automation/package

# The 'pretty' versions of targets run before the regular ones to avoid
# conflicts in writing to the same files.
automation/installer: automation/pretty-installer
automation/package: automation/pretty-package
automation/package-tests: automation/pretty-package-tests
automation/l10n-check: automation/pretty-l10n-check
automation/update-packaging: automation/pretty-update-packaging

automation/build: $(addprefix automation/,$(MOZ_AUTOMATION_TIERS))
	@echo Automation steps completed.

# Note: We have to force -j1 here, at least until bug 1036563 is fixed.
AUTOMATION_EXTRA_CMDLINE-l10n-check = -j1
AUTOMATION_EXTRA_CMDLINE-pretty-l10n-check = -j1

# The commands only run if the corresponding MOZ_AUTOMATION_* variable is
# enabled. This means, for example, if we enable MOZ_AUTOMATION_UPLOAD, then
# 'buildsymbols' will only run if MOZ_AUTOMATION_BUILD_SYMBOLS is also set.
# However, the target automation/buildsymbols will still be executed in this
# case because it is a prerequisite of automation/upload.
define automation_commands
@+$(MAKE) $1 $(AUTOMATION_EXTRA_CMDLINE-$1)
$(call BUILDSTATUS,TIER_FINISH $1)
endef

# The tier start message is in a separate target so make doesn't buffer it
# until the step completes with output syncing enabled.
automation-start/%:
	$(if $(filter $*,$(MOZ_AUTOMATION_TIERS)),$(call BUILDSTATUS,TIER_START $*))

automation/%: automation-start/%
	$(if $(filter $*,$(MOZ_AUTOMATION_TIERS)),$(call automation_commands,$*))
