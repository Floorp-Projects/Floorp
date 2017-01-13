# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

if test "$MOZ_HORIZON"; then
MOZ_APP_BASENAME=Horizon
else
MOZ_APP_BASENAME=Graphene
fi

MOZ_APP_VENDOR=Mozilla
MOZ_UPDATER=1

MOZ_B2G=1
MOZ_GRAPHENE=1

MOZ_APP_VERSION=$FIREFOX_VERSION
MOZ_APP_UA_NAME=Firefox

MOZ_B2G_VERSION=2.6.0.0-prerelease
MOZ_B2G_OS_NAME=Boot2Gecko

MOZ_BRANDING_DIRECTORY=b2g/branding/unofficial
MOZ_OFFICIAL_BRANDING_DIRECTORY=b2g/branding/official
# MOZ_APP_DISPLAYNAME is set by branding/configure.sh

MOZ_CAPTIVEDETECT=1

MOZ_NO_SMART_CARDS=1
NSS_NO_LIBPKIX=1

if test "$OS_TARGET" = "Android"; then
MOZ_CAPTURE=1
MOZ_RAW=1
MOZ_AUDIO_CHANNEL_MANAGER=1
fi

MOZ_APP_ID={d1bfe7d9-c01e-4237-998b-7b5f960a4314}
MOZ_TIME_MANAGER=1

MOZ_TOOLKIT_SEARCH=
MOZ_PLACES=
MOZ_B2G=1

MOZ_JSDOWNLOADS=1

MOZ_BUNDLED_FONTS=1

export JS_GC_SMALL_CHUNK_SIZE=1

# Include the DevTools client, not just the server (which is the default)
MOZ_DEVTOOLS=all
