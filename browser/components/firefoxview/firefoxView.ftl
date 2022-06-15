# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

-firefoxview-brand-name = Firefox View

toolbar-button-firefox-view =
  .label = { -firefoxview-brand-name }
  .tooltiptext = { -firefoxview-brand-name }

firefoxview-page-title = { -firefoxview-brand-name }

# Used instead of the localized relative time when a timestamp is within a minute or so of now
firefoxview-just-now-timestamp = Just now

# This is a headline for an area in the product where users can resume and re-open tabs they have previously viewed on other devices.
firefoxview-tabpickup-header = Tab pickup
firefoxview-tabpickup-description = Open pages from other devices.

firefoxview-tabpickup-recenttabs-description = Recent tabs list would go here

# Variables:
#  $percentValue (Number): the percentage value for setup completion
firefoxview-tabpickup-progress-label = { $percentValue }% complete

firefoxview-tabpickup-step-signin-header = Switch seamlessly between devices
firefoxview-tabpickup-step-signin-description = To grab your phone tabs here, first sign in or create an account.
firefoxview-tabpickup-step-signin-primarybutton = Continue

firefoxview-tabpickup-adddevice-header = Sync { -brand-product-name } on your phone or tablet
firefoxview-tabpickup-adddevice-description = Download { -brand-product-name } for mobile and sign in there.
firefoxview-tabpickup-adddevice-learn-how = Learn how
firefoxview-tabpickup-adddevice-primarybutton = Get { -brand-product-name } for mobile

firefoxview-tabpickup-synctabs-header = Turn on tab syncing
firefoxview-tabpickup-synctabs-description = Allow { -brand-short-name } to share tabs between devices.
firefoxview-tabpickup-synctabs-learn-how = Learn how
firefoxview-tabpickup-synctabs-primarybutton = Sync open tabs

firefoxview-tabpickup-syncing = Sit tight while your tabs sync. It’ll be just a moment.

firefoxview-closed-tabs-title = Recently closed
firefoxview-closed-tabs-collapse-button =
  .title = Show or hide recently closed tabs list

firefoxview-closed-tabs-description = Reopen pages you’ve closed on this device.
firefoxview-closed-tabs-placeholder = History is empty <br/> The next time you close a tab, you can reopen it here.

# Variables:
#   $targetURI (string) - URL that will be opened in the new tab
firefoxview-closed-tabs-tab-button =
  .title = Open { $targetURI } in a new tab
