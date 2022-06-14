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
firefoxview-tabpickup-header = Tab Pickup
firefoxview-tabpickup-description = Pick up where you left off on another device.

firefoxview-tabpickup-recenttabs-description = Recent tabs list would go here

# Variables:
#  $percentValue (Number): the percentage value for setup completion
firefoxview-tabpickup-progress-label = { $percentValue }% complete

firefoxview-tabpickup-step-signin-header = Step 1 of 3: Sign in to { -brand-product-name }
firefoxview-tabpickup-step-signin-description = To get your mobile tabs on this device, sign in to { -brand-product-name } and turn on sync.
firefoxview-tabpickup-step-signin-primarybutton = Continue

# These are placeholders for now..

firefoxview-tabpickup-adddevice-header = Step 2 of 3: Sign in on a mobile device
firefoxview-tabpickup-adddevice-description = Step 2 description.
firefoxview-tabpickup-adddevice-primarybutton = Get { -brand-product-name } for mobile

firefoxview-tabpickup-synctabs-header = Step 3 of 3: Sync open tabs
firefoxview-tabpickup-synctabs-description = Step 3 description.
firefoxview-tabpickup-synctabs-primarybutton = Sync open tabs

firefoxview-tabpickup-setupsuccess-header = Setup Complete!
firefoxview-tabpickup-setupsuccess-description = Step 4 description.
firefoxview-tabpickup-setupsuccess-primarybutton = Get my other tabs

firefoxview-closed-tabs-title = Recently closed
firefoxview-closed-tabs-collapse-button =
  .title = Show or hide recently closed tabs list

firefoxview-closed-tabs-description = Reopen pages you’ve closed on this device.
firefoxview-closed-tabs-placeholder = History is empty <br/> The next time you close a tab, you can reopen it here.

# Variables:
#   $targetURI (string) - URL that will be opened in the new tab
firefoxview-closed-tabs-tab-button =
  .title = Open { $targetURI } in a new tab
