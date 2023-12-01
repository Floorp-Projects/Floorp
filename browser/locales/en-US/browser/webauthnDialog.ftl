# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Variables:
#  $retriesLeft (Number): number of tries left
webauthn-pin-invalid-long-prompt =
    { $retriesLeft ->
        [one] Incorrect PIN. You have { $retriesLeft } attempt left before you permanently lose access to the credentials on this device.
       *[other] Incorrect PIN. You have { $retriesLeft } attempts left before you permanently lose access to the credentials on this device.
    }
webauthn-pin-invalid-short-prompt = Incorrect PIN. Try again.
webauthn-pin-required-prompt = Please enter the PIN for your device.

webauthn-select-sign-result-unknown-account = Unknown account

webauthn-a-passkey-label = Use a passkey
webauthn-another-passkey-label = Use another passkey

# Variables:
#   $domain (String): the domain of the site.
webauthn-specific-passkey-label = Passkey for { $domain }

# Variables:
#  $retriesLeft (Number): number of tries left
webauthn-uv-invalid-long-prompt =
    { $retriesLeft ->
        [one] User verification failed. You have { $retriesLeft } attempt left. Try again.
       *[other] User verification failed. You have { $retriesLeft } attempts left. Try again.
    }
webauthn-uv-invalid-short-prompt = User verification failed. Try again.
