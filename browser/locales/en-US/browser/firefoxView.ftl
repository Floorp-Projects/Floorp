# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

toolbar-button-firefox-view =
  .label = { -firefoxview-brand-name }
  .tooltiptext = { -firefoxview-brand-name }

menu-tools-firefox-view =
  .label = { -firefoxview-brand-name }
  .accesskey = F

firefoxview-page-title = { -firefoxview-brand-name }

firefoxview-close-button =
  .title = Close
  .aria-label = Close

# Used instead of the localized relative time when a timestamp is within a minute or so of now
firefoxview-just-now-timestamp = Just now

# This is a headline for an area in the product where users can resume and re-open tabs they have previously viewed on other devices.
firefoxview-tabpickup-header = Tab pickup
firefoxview-tabpickup-description = Open pages from other devices.

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

firefoxview-tabpickup-fxa-admin-disabled-header = Your organization has disabled sync
firefoxview-tabpickup-fxa-admin-disabled-description = { -brand-short-name } is not able to sync tabs between devices because your administrator has disabled syncing.

firefoxview-tabpickup-network-offline-header = Check your internet connection
firefoxview-tabpickup-network-offline-description = If you’re using a firewall or proxy, check that { -brand-short-name } has permission to access the web.
firefoxview-tabpickup-network-offline-primarybutton = Try again

firefoxview-tabpickup-sync-error-header = We’re having trouble syncing
firefoxview-tabpickup-generic-sync-error-description = { -brand-short-name } can’t reach the syncing service right now. Try again in a few moments.
firefoxview-tabpickup-sync-error-primarybutton = Try again

firefoxview-tabpickup-sync-disconnected-header = Turn on syncing to continue
firefoxview-tabpickup-sync-disconnected-description = To grab your tabs, you’ll need to allow syncing in { -brand-short-name }.
firefoxview-tabpickup-sync-disconnected-primarybutton = Turn on sync in settings

firefoxview-tabpickup-syncing = Sit tight while your tabs sync. It’ll be just a moment.

firefoxview-mobile-promo-header = Grab tabs from your phone or tablet
firefoxview-mobile-promo-description = To view your latest mobile tabs, sign in to { -brand-product-name } on iOS or Android.
firefoxview-mobile-promo-primarybutton = Get { -brand-product-name } for mobile

firefoxview-mobile-confirmation-header = 🎉 Good to go!
firefoxview-mobile-confirmation-description = Now you can grab your { -brand-product-name } tabs from your tablet or phone.

firefoxview-closed-tabs-title = Recently closed

firefoxview-closed-tabs-description2 = Reopen pages you’ve closed in this window.
firefoxview-closed-tabs-placeholder = <strong>No recently closed pages</strong><br/>Never fear losing a closed tab again. You can always retrieve it here.

# refers to the last tab that was used
firefoxview-pickup-tabs-badge = Last active

# Variables:
#   $targetURI (string) - URL that will be opened in the new tab
firefoxview-tabs-list-tab-button =
  .title = Open { $targetURI } in a new tab

firefoxview-try-colorways-button = Try colorways
firefoxview-no-current-colorway-collection = New colorways are on the way
firefoxview-change-colorway-button = Change colorway

# Variables:
#  $intensity (String): Colorway intensity
#  $collection (String): Colorway Collection name
firefoxview-colorway-description = { $intensity } · { $collection }

firefoxview-synced-tabs-placeholder = <strong>Nothing to see yet</strong><br/>The next time you open a page in { -brand-product-name } on another device, grab it here like magic.

firefoxview-collapse-button-show =
  .title = Show list

firefoxview-collapse-button-hide =
  .title = Hide list
