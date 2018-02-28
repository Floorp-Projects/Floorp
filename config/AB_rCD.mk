# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Turn 'en-US' into '' and general 'ab-CD' into '-ab-rCD' -- note
# leading hyphen.
#
# For example, 'res{AB_rCD}' is 'res' for 'en-US' and 'res-en-rGB' for
# 'en-GB'.
#
# Some locale codes are special, namely 'he' and 'id'.  See
# http://code.google.com/p/android/issues/detail?id=3639.
#
# This is for use in Android resource directories, which uses a
# somewhat non-standard resource naming scheme for localized
# resources: see
# https://developer.android.com/guide/topics/resources/providing-resources.html#AlternativeResources.
# Android 24+ uses the standard BCP-47 naming scheme, and Bug 1441305
# tracks migrating to that naming scheme.
AB_rCD = $(if $(filter en-US,$(AB_CD)),,-$(if $(filter he, $(AB_CD)),iw,$(if $(filter id, $(AB_CD)),in,$(subst -,-r,$(AB_CD)))))
