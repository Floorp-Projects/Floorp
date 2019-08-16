# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

## These messages are used as headings in the recommendation doorhanger

cfr-doorhanger-extension-heading = Recommended Extension
cfr-doorhanger-feature-heading = Recommended Feature
cfr-doorhanger-pintab-heading = Try This: Pin Tab
##

cfr-doorhanger-extension-sumo-link =
  .tooltiptext = Why am I seeing this

cfr-doorhanger-extension-cancel-button = Not Now
  .accesskey = N

cfr-doorhanger-extension-ok-button = Add Now
  .accesskey = A
cfr-doorhanger-pintab-ok-button = Pin This Tab
  .accesskey = P

cfr-doorhanger-extension-manage-settings-button = Manage Recommendation Settings
  .accesskey = M

cfr-doorhanger-extension-never-show-recommendation = Don’t Show Me This Recommendation
  .accesskey = S

cfr-doorhanger-extension-learn-more-link = Learn more

# This string is used on a new line below the add-on name
# Variables:
#   $name (String) - Add-on author name
cfr-doorhanger-extension-author = by { $name }

# This is a notification displayed in the address bar.
# When clicked it opens a panel with a message for the user.
cfr-doorhanger-extension-notification = Recommendation

## Add-on statistics
## These strings are used to display the total number of
## users and rating for an add-on. They are shown next to each other.

# Variables:
#   $total (Number) - The rating of the add-on from 1 to 5
cfr-doorhanger-extension-rating =
  .tooltiptext =
    { $total ->
        [one] { $total } star
       *[other] { $total } stars
    }
# Variables:
#   $total (Number) - The total number of users using the add-on
cfr-doorhanger-extension-total-users =
  { $total ->
      [one] { $total } user
     *[other] { $total } users
  }

cfr-doorhanger-pintab-description = Get easy access to your most-used sites. Keep sites open in a tab (even when you restart).

## These messages are steps on how to use the feature and are shown together.

cfr-doorhanger-pintab-step1 = <b>Right-click</b> on the tab you want to pin.
cfr-doorhanger-pintab-step2 = Select <b>Pin Tab</b> from the menu.
cfr-doorhanger-pintab-step3 = If the site has an update you’ll see a blue dot on your pinned tab.

cfr-doorhanger-pintab-animation-pause = Pause
cfr-doorhanger-pintab-animation-resume = Resume


## Firefox Accounts Message

cfr-doorhanger-bookmark-fxa-header = Sync your bookmarks everywhere.
cfr-doorhanger-bookmark-fxa-body = Great find! Now don’t be left without this bookmark on your mobile devices. Get Started with a { -fxaccount-brand-name }.
cfr-doorhanger-bookmark-fxa-link-text = Sync bookmarks now…
cfr-doorhanger-bookmark-fxa-close-btn-tooltip =
  .aria-label = Close button
  .title = Close

## Protections panel

cfr-protections-panel-header = Browse without being followed
cfr-protections-panel-body = Keep your data to yourself. { -brand-short-name } protects you from many of the most common trackers that follow what you do online.
cfr-protections-panel-link-text = Learn more

## What's New toolbar button and panel

cfr-whatsnew-button =
  .label = What’s New
  .tooltiptext = What’s New

cfr-whatsnew-panel-header = What’s New

## Bookmark Sync

cfr-doorhanger-sync-bookmarks-header = Get this bookmark on your phone
cfr-doorhanger-sync-bookmarks-body = Take your bookmarks, passwords, history and more everywhere you’re signed into { -brand-product-name }.
cfr-doorhanger-sync-bookmarks-ok-button = Turn on { -sync-brand-short-name }
  .accesskey = T

## Send Tab

cfr-doorhanger-send-tab-header = Read this on the go
cfr-doorhanger-send-tab-recipe-header = Take this recipe to the kitchen
cfr-doorhanger-send-tab-body = Send Tab lets you easily share this link to your phone or anywhere you’re signed in to { -brand-product-name }.
cfr-doorhanger-send-tab-ok-button = Try Send Tab
  .accesskey = T

## Firefox Send

cfr-doorhanger-firefox-send-header = Share this PDF securely
cfr-doorhanger-firefox-send-body = Keep your sensitive documents safe from prying eyes with end-to-end encryption and a link that disappears when you’re done.
cfr-doorhanger-firefox-send-ok-button = Try { -send-brand-name }
  .accesskey = T
