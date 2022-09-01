# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

## The address and credit card autofill management dialog in browser preferences

autofill-manage-addresses-title = Saved Addresses
autofill-manage-addresses-list-header = Addresses

autofill-manage-credit-cards-title = Saved Credit Cards
autofill-manage-credit-cards-list-header = Credit Cards

autofill-manage-dialog =
    .style = min-width: 560px
autofill-manage-remove-button = Remove
autofill-manage-add-button = Add…
autofill-manage-edit-button = Edit…

# In macOS, this string is preceded by the operating system with "Firefox is trying to ",
# and has a period added to its end. Make sure to test in your locale.
autofill-edit-card-password-prompt = { PLATFORM() ->
    [macos] show credit card information
    [windows] { -brand-short-name } is trying to show credit card information. Confirm access to this Windows account below.
   *[other] { -brand-short-name } is trying to show credit card information.
}
