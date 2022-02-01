#! /bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

MOZ_APP_VENDOR=Mozilla

if test "$OS_ARCH" = "WINNT"; then
  if ! test "$HAVE_64BIT_BUILD"; then
    if test "$MOZ_UPDATE_CHANNEL" = "nightly" -o \
            "$MOZ_UPDATE_CHANNEL" = "nightly-try" -o \
            "$MOZ_UPDATE_CHANNEL" = "aurora" -o \
            "$MOZ_UPDATE_CHANNEL" = "beta" -o \
            "$MOZ_UPDATE_CHANNEL" = "release"; then
      if ! test "$MOZ_DEBUG"; then
        if ! test "$USE_STUB_INSTALLER"; then
          # Expect USE_STUB_INSTALLER from taskcluster for downstream task consistency
          echo "ERROR: STUB installer expected to be enabled but"
          echo "ERROR: USE_STUB_INSTALLER is not specified in the environment"
          exit 1
        fi
        MOZ_STUB_INSTALLER=1
      fi
    fi
  fi
fi

BROWSER_CHROME_URL=chrome://browser/content/browser.xhtml

# MOZ_APP_DISPLAYNAME will be set by branding/configure.sh
# MOZ_BRANDING_DIRECTORY is the default branding directory used when none is
# specified. It should never point to the "official" branding directory.
# For mozilla-beta, mozilla-release, or mozilla-central repositories, use
# "unofficial" branding.
# For the mozilla-aurora repository, use "aurora".
MOZ_BRANDING_DIRECTORY=browser/branding/unofficial
MOZ_OFFICIAL_BRANDING_DIRECTORY=browser/branding/official
MOZ_APP_ID={ec8030f7-c20a-464f-9b0e-13a3a9e97384}

MOZ_PROFILE_MIGRATOR=1

# Include the DevTools client, not just the server (which is the default)
MOZ_DEVTOOLS=all
