# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

toolbar-button-firefox-view-2 =
  .label = { -firefoxview-brand-name }
  .tooltiptext = View recent browsing across windows and devices

menu-tools-firefox-view =
  .label = { -firefoxview-brand-name }
  .accesskey = F

firefoxview-page-title = { -firefoxview-brand-name }

firefoxview-page-label =
  .label = { -firefoxview-brand-name }

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

firefoxview-syncedtabs-signin-header = Grab tabs from anywhere
firefoxview-syncedtabs-signin-description = To see your tabs from wherever you use { -brand-product-name }, sign in to your account. If you don’t have an account, we’ll take you through the steps to sign up.
firefoxview-syncedtabs-signin-primarybutton = Sign in or sign up

firefoxview-tabpickup-adddevice-header = Sync { -brand-product-name } on your phone or tablet
firefoxview-tabpickup-adddevice-description = Download { -brand-product-name } for mobile and sign in there.
firefoxview-tabpickup-adddevice-learn-how = Learn how
firefoxview-tabpickup-adddevice-primarybutton = Get { -brand-product-name } for mobile

firefoxview-syncedtabs-adddevice-header = Sign in to { -brand-product-name } on your other devices
firefoxview-syncedtabs-adddevice-description = To see your tabs from wherever you use { -brand-product-name }, sign in on all your devices. Learn how to <a data-l10n-name="url">connect additional devices</a>.
firefoxview-syncedtabs-adddevice-primarybutton = Try { -brand-product-name } for mobile

firefoxview-tabpickup-synctabs-header = Turn on tab syncing
firefoxview-tabpickup-synctabs-description = Allow { -brand-short-name } to share tabs between devices.
firefoxview-tabpickup-synctabs-learn-how = Learn how
firefoxview-tabpickup-synctabs-primarybutton = Sync open tabs

firefoxview-syncedtabs-synctabs-header = Update your sync settings
firefoxview-syncedtabs-synctabs-description = To see tabs from other devices, you need to sync your open tabs.
firefoxview-syncedtabs-synctabs-checkbox = Allow open tabs to sync

firefoxview-syncedtabs-loading-header = Sync in progress
firefoxview-syncedtabs-loading-description = When it’s done, you’ll see any tabs you have open on other devices. Check back soon.

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

firefoxview-tabpickup-password-locked-header = Enter your Primary Password to view tabs
firefoxview-tabpickup-password-locked-description = To grab your tabs, you’ll need to enter the Primary Password for { -brand-short-name }.
firefoxview-tabpickup-password-locked-link = Learn more
firefoxview-tabpickup-password-locked-primarybutton = Enter Primary Password
firefoxview-syncedtab-password-locked-link = <a data-l10n-name="syncedtab-password-locked-link">Learn more</a>

firefoxview-tabpickup-signed-out-header = Sign in to reconnect
firefoxview-tabpickup-signed-out-description2 = To reconnect and grab your tabs, sign in to your account.
firefoxview-tabpickup-signed-out-primarybutton = Sign in

firefoxview-tabpickup-syncing = Sit tight while your tabs sync. It’ll be just a moment.

firefoxview-mobile-promo-header = Grab tabs from your phone or tablet
firefoxview-mobile-promo-description = To view your latest mobile tabs, sign in to { -brand-product-name } on iOS or Android.
firefoxview-mobile-promo-primarybutton = Get { -brand-product-name } for mobile

firefoxview-mobile-confirmation-header = 🎉 Good to go!
firefoxview-mobile-confirmation-description = Now you can grab your { -brand-product-name } tabs from your tablet or phone.

firefoxview-closed-tabs-placeholder-header = No recently closed tabs
firefoxview-closed-tabs-placeholder-body2 = When you close a tab, you can fetch it from here.

# Variables:
#   $tabTitle (string) - Title of tab being dismissed
firefoxview-closed-tabs-dismiss-tab =
  .title = Dismiss { $tabTitle }

# refers to the last tab that was used
firefoxview-pickup-tabs-badge = Last active

# Variables:
#   $targetURI (string) - URL that will be opened in the new tab
firefoxview-tabs-list-tab-button =
  .title = Open { $targetURI } in a new tab

firefoxview-try-colorways-button = Try colorways
firefoxview-change-colorway-button = Change colorway

# Variables:
#  $intensity (String): Colorway intensity
#  $collection (String): Colorway Collection name
firefoxview-colorway-description = { $intensity } · { $collection }

firefoxview-synced-tabs-placeholder-header = Nothing to see yet
firefoxview-synced-tabs-placeholder-body = The next time you open a page in { -brand-product-name } on another device, grab it here like magic.

firefoxview-collapse-button-show =
  .title = Show list

firefoxview-collapse-button-hide =
  .title = Hide list

firefoxview-overview-nav = Recent browsing
  .title = Recent browsing
firefoxview-overview-header = Recent browsing
  .title = Recent browsing

## History in this context refers to browser history

firefoxview-history-nav = History
  .title = History
firefoxview-history-header = History
firefoxview-history-context-delete = Delete from History
    .accesskey = D

## Open Tabs in this context refers to all open tabs in the browser

firefoxview-opentabs-nav = Open tabs
  .title = Open tabs
firefoxview-opentabs-header = Open tabs

## Recently closed tabs in this context refers to recently closed tabs from all windows

firefoxview-recently-closed-nav = Recently closed tabs
  .title = Recently closed tabs
firefoxview-recently-closed-header = Recently closed tabs

## Tabs from other devices refers in this context refers to synced tabs from other devices

firefoxview-synced-tabs-nav = Tabs from other devices
  .title = Tabs from other devices
firefoxview-synced-tabs-header = Tabs from other devices

##

# Used for a link in collapsible cards, in the ’Recent browsing’ page of Firefox View
firefoxview-view-all-link = View all

# Variables:
#   $winID (Number) - The index of the owner window for this set of tabs
firefoxview-opentabs-window-header =
  .title = Window { $winID }

# Variables:
#   $winID (Number) - The index of the owner window (which is currently focused) for this set of tabs
firefoxview-opentabs-current-window-header =
  .title = Window { $winID } (Current)

firefoxview-opentabs-focus-tab =
  .title = Switch to this tab

firefoxview-show-more = Show more
firefoxview-show-less = Show less

firefoxview-search-text-box-clear-button =
  .title = Clear

# Placeholder for the input field to search in history ("search" is a verb).
firefoxview-search-text-box-history =
  .placeholder = Search history

# "Search" is a noun (as in "Results of the search for")
# Variables:
#   $query (String) - The search query used for searching through browser history.
firefoxview-search-results-header = Search results for “{ $query }”

# Variables:
#   $count (Number) - The number of visits matching the search query.
firefoxview-search-results-count = { $count ->
  [one] { $count } site
 *[other] { $count } sites
}

# Message displayed when a search is performed and no matching results were found.
# Variables:
#   $query (String) - The search query.
firefoxview-search-results-empty = No results for “{ $query }”

firefoxview-sort-history-by-date-label = Sort by date
firefoxview-sort-history-by-site-label = Sort by site

# Variables:
#   $url (string) - URL that will be opened in the new tab
firefoxview-opentabs-tab-row =
  .title = Switch to { $url }

## Variables:
##   $date (string) - Date to be formatted based on locale

firefoxview-history-date-today = Today - { DATETIME($date, dateStyle: "full") }
firefoxview-history-date-yesterday = Yesterday - { DATETIME($date, dateStyle: "full") }
firefoxview-history-date-this-month = { DATETIME($date, dateStyle: "full") }
firefoxview-history-date-prev-month = { DATETIME($date, month: "long", year: "numeric") }

# When history is sorted by site, this heading is used in place of a domain, in
# order to group sites that do not come from an outside host.
# For example, this would be the heading for all file:/// URLs in history.
firefoxview-history-site-localhost = (local files)

##

firefoxview-show-all-history = Show all history

firefoxview-view-more-browsing-history = View more browsing history

## Message displayed in Firefox View when the user has no history data

firefoxview-history-empty-header = Get back to where you’ve been
firefoxview-history-empty-description = As you browse, the pages you visit will be listed here.
firefoxview-history-empty-description-two = Protecting your privacy is at the heart of what we do. It’s why you can control the activity { -brand-short-name } remembers, in your <a data-l10n-name="history-settings-url">history settings</a>.

##

# Button text for choosing a browser within the ’Import history from another browser’ banner
firefoxview-choose-browser-button = Choose browser
  .title = Choose browser

## Message displayed in Firefox View when the user has chosen to never remember History

firefoxview-dont-remember-history-empty-header = Nothing to show
firefoxview-dont-remember-history-empty-description = Protecting your privacy is at the heart of what we do. It’s why you can control the activity { -brand-short-name } remembers.
firefoxview-dont-remember-history-empty-description-two = Based on your current settings, { -brand-short-name } doesn’t remember your activity as you browse. To change that, <a data-l10n-name="history-settings-url-two">change your history settings to remember your history</a>.

##

# This label is read by screen readers when focusing the close button for the "Import history from another browser" banner in Firefox View
firefoxview-import-history-close-button =
  .aria-label = Close
  .title = Close

## Text displayed in a dismissable banner to import bookmarks/history from another browser

firefoxview-import-history-header = Import history from another browser
firefoxview-import-history-description = Make { -brand-short-name } your go-to browser. Import browsing history, bookmarks, and more.

## Message displayed in Firefox View when the user has no recently closed tabs data

firefoxview-recentlyclosed-empty-header = Closed a tab too soon?
firefoxview-recentlyclosed-empty-description = Here you’ll find the tabs you recently closed, so you can reopen any of them quickly.
firefoxview-recentlyclosed-empty-description-two = To find tabs from longer ago, view your <a data-l10n-name="history-url">browsing history</a>.

## This message is displayed below the name of another connected device when it doesn't have any open tabs.

firefoxview-syncedtabs-device-notabs = No tabs open on this device

firefoxview-syncedtabs-connect-another-device = Connect another device
