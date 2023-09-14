# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

## Error messages for failed HTTP web requests.
## https://developer.mozilla.org/en-US/docs/Web/HTTP/Status#client_error_responses
## Variables:
##   $status (Number) - HTTP status code, for example 403

firefox-relay-mask-generation-failed = { -relay-brand-name } could not generate a new mask. HTTP error code: { $status }.
firefox-relay-get-reusable-masks-failed = { -relay-brand-name } could not find reusable masks. HTTP error code: { $status }.

##

firefox-relay-must-login-to-account = Sign in to your account to use your { -relay-brand-name } email masks.
firefox-relay-get-unlimited-masks =
    .label = Manage masks
    .accesskey = M
# This is followed, on a new line, by firefox-relay-opt-in-subtitle-1
firefox-relay-opt-in-title-1 = Protect your email address:
# This is preceded by firefox-relay-opt-in-title-1 (on a different line), which
# ends with a colon. You might need to adapt the capitalization of this string.
firefox-relay-opt-in-subtitle-1 = Use { -relay-brand-name } email mask
firefox-relay-use-mask-title = Use { -relay-brand-name } email mask
firefox-relay-opt-in-confirmation-enable-button =
    .label = Use email mask
    .accesskey = U
firefox-relay-opt-in-confirmation-disable =
    .label = Don’t show me this again
    .accesskey = D
firefox-relay-opt-in-confirmation-postpone =
    .label = Not now
    .accesskey = N
