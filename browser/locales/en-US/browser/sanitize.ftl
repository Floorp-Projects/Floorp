# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

sanitize-prefs =
    .title = Settings for Clearing History
    .style = width: 34em

sanitize-prefs-style =
    .style = width: 17em

dialog-title =
    .title = Clear Recent History
    .style = width: 34em

# When "Time range to clear" is set to "Everything", this message is used for the
# title instead of dialog-title.
dialog-title-everything =
    .title = Clear All History
    .style = width: 34em

clear-data-settings-label = When closed, { -brand-short-name } should automatically clear all

## clear-time-duration-prefix is followed by a dropdown list, with
## values localized using clear-time-duration-value-* messages.
## clear-time-duration-suffix is left empty in English, but can be
## used in other languages to change the structure of the message.
##
## This results in English:
## Time range to clear: (Last Hour, Today, etc.)

clear-time-duration-prefix =
    .value = Time range to clear:{ " " }
    .accesskey = T

clear-time-duration-value-last-hour =
    .label = Last hour

clear-time-duration-value-last-2-hours =
    .label = Last two hours

clear-time-duration-value-last-4-hours =
    .label = Last four hours

clear-time-duration-value-today =
    .label = Today

clear-time-duration-value-everything =
    .label = Everything

clear-time-duration-suffix =
    .value = { "" }

## These strings are used as section comments and checkboxes
## to select the items to remove

history-section-label = History

item-history-and-downloads =
    .label = Browsing & download history
    .accesskey = B

item-cookies =
    .label = Cookies
    .accesskey = C

item-active-logins =
    .label = Active logins
    .accesskey = l

item-cache =
    .label = Cache
    .accesskey = a

item-form-search-history =
    .label = Form & search history
    .accesskey = F

data-section-label = Data

item-site-settings =
    .label = Site settings
    .accesskey = S

item-offline-apps =
    .label = Offline website data
    .accesskey = O

sanitize-everything-undo-warning = This action cannot be undone.

window-close =
    .key = w

sanitize-button-ok =
    .label = Clear Now

# The label for the default button between the user clicking it and the window
# closing.  Indicates the items are being cleared.
sanitize-button-clearing =
    .label = Clearing

# Warning that appears when "Time range to clear" is set to "Everything" in Clear
# Recent History dialog, provided that the user has not modified the default set
# of history items to clear.
sanitize-everything-warning = All history will be cleared.

# Warning that appears when "Time range to clear" is set to "Everything" in Clear
# Recent History dialog, provided that the user has modified the default set of
# history items to clear.
sanitize-selected-warning = All selected items will be cleared.
