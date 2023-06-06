# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

## Credential panel
##
## Identity providers are websites you use to log in to another website, for
## example: Google when you Log in with Google.
##
## Variables:
##  $host (String): the hostname of the site that is being displayed.
##  $provider (String): the hostname of another website you are using to log in to the site being displayed

identity-credential-header-providers = Sign in with a login provider
identity-credential-header-accounts = Sign in with { $provider }
identity-credential-urlbar-anchor =
    .tooltiptext = Open login panel
identity-credential-cancel-button =
    .label = Cancel
    .accesskey = n
identity-credential-accept-button =
    .label = Continue
    .accesskey = C
identity-credential-sign-in-button =
    .label = Sign In
    .accesskey = S
identity-credential-policy-title = Use { $provider } as a login provider
identity-credential-policy-description = Logging in to { $host } with a { $provider } account is subject to their <label data-l10n-name="privacy-url">Privacy Policy</label> and <label data-l10n-name="tos-url">Terms of Service</label>.
