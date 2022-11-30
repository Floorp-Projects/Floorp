# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

## Credential panel
##  $host (String): the hostname of the site that is being displayed.
##  $provider (String): the hostname of another website you are using to log in to the site being displayed

identity-credential-header-providers = Sign in to { $host }
identity-credential-header-accounts = Pick a { $host } account
# Identity providers are websites you use to log into another website, for example: Google when you Log in with Google.
identity-credential-description-provider-explanation = These are the identity providers that would like to help you log in.
identity-credential-description-account-explanation = Picking an account here shares that identity with { $host }.
identity-credential-urlbar-anchor =
    .tooltiptext = Open federated login panel
identity-credential-cancel-button =
    .label = Cancel
    .accesskey = C
identity-credential-accept-button =
    .label = Okay
    .accesskey = O
identity-credential-policy-title = Legal information
identity-credential-policy-description = Logging into { $host } with an account from { $provider } is controlled by these legal policies. This is optional and you can cancel this and try to log in again using another method.
identity-credential-privacy-policy = Privacy Policy
identity-credential-terms-of-service = Terms of Service
