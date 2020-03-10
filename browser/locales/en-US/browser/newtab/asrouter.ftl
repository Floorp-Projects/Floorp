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
cfr-doorhanger-extension-notification2 = Recommendation
  .tooltiptext = Extension recommendation
  .a11y-announcement = Extension recommendation available

# This is a notification displayed in the address bar.
# When clicked it opens a panel with a message for the user.
cfr-doorhanger-feature-notification = Recommendation
  .tooltiptext = Feature recommendation
  .a11y-announcement = Feature recommendation available

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

# This string is used by screen readers to offer a text based alternative for
# the notification icon
cfr-badge-reader-label-newfeature = New feature:

cfr-whatsnew-button =
  .label = What’s New
  .tooltiptext = What’s New

cfr-whatsnew-panel-header = What’s New

cfr-whatsnew-release-notes-link-text = Read the release notes

cfr-whatsnew-fx70-title = { -brand-short-name } now fights harder for your privacy
cfr-whatsnew-fx70-body =
   The latest update enhances the Tracking Protection feature and makes it
   easier than ever to create secure passwords for every site.

cfr-whatsnew-tracking-protect-title = Protect yourself from trackers
cfr-whatsnew-tracking-protect-body =
   { -brand-short-name } blocks many common social and cross-site trackers that
   follow what you do online.
cfr-whatsnew-tracking-protect-link-text = View Your Report

# This string is displayed before a large numeral that indicates the total
# number of tracking elements blocked. Don’t add $blockedCount to your
# localization, because it would result in the number showing twice.
cfr-whatsnew-tracking-blocked-title =
  { $blockedCount ->
    [one] Tracker blocked
   *[other] Trackers blocked
  }
cfr-whatsnew-tracking-blocked-subtitle =
   Since { DATETIME($earliestDate, month: "long", year: "numeric") }
cfr-whatsnew-tracking-blocked-link-text = View Report

cfr-whatsnew-lockwise-backup-title = Back up your passwords
cfr-whatsnew-lockwise-backup-body =
   Now generate secure passwords you can access anywhere you sign in.
cfr-whatsnew-lockwise-backup-link-text = Turn on backups

cfr-whatsnew-lockwise-take-title = Take your passwords with you
cfr-whatsnew-lockwise-take-body =
   The { -lockwise-brand-short-name } mobile app lets you securely access your
   backed up passwords from anywhere.
cfr-whatsnew-lockwise-take-link-text = Get the app

## Search Bar

cfr-whatsnew-searchbar-title = Type less, find more with the address bar
cfr-whatsnew-searchbar-body-topsites = Now, just select the address bar, and a box will expand with links to your top sites.
cfr-whatsnew-searchbar-icon-alt-text = Magnifying glass icon

## Picture-in-Picture

cfr-whatsnew-pip-header = Watch videos while you browse
cfr-whatsnew-pip-body = Picture-in-picture pops video into a floating window so you can watch while working in other tabs.
cfr-whatsnew-pip-cta = Learn more

## Permission Prompt

cfr-whatsnew-permission-prompt-header = Fewer annoying site pop-ups
cfr-whatsnew-permission-prompt-body = { -brand-shorter-name } now blocks sites from automatically asking to send you pop-up messages.
cfr-whatsnew-permission-prompt-cta = Learn more

## Fingerprinter Counter

# This string is displayed before a large numeral that indicates the total
# number of tracking elements blocked. Don’t add $fingerprinterCount to your
# localization, because it would result in the number showing twice.
cfr-whatsnew-fingerprinter-counter-header =
  { $fingerprinterCount ->
    [one] Fingerprinter blocked
   *[other] Fingerprinters blocked
  }
cfr-whatsnew-fingerprinter-counter-body = { -brand-shorter-name } blocks many fingerprinters that secretly gather information about your device and actions to create an advertising profile of you.

# Message variation when fingerprinters count is less than 10
cfr-whatsnew-fingerprinter-counter-header-alt = Fingerprinters
cfr-whatsnew-fingerprinter-counter-body-alt = { -brand-shorter-name } can block fingerprinters that secretly gather information about your device and actions to create an advertising profile of you.

## Bookmark Sync

cfr-doorhanger-sync-bookmarks-header = Get this bookmark on your phone
cfr-doorhanger-sync-bookmarks-body = Take your bookmarks, passwords, history and more everywhere you’re signed into { -brand-product-name }.
cfr-doorhanger-sync-bookmarks-ok-button = Turn on { -sync-brand-short-name }
  .accesskey = T

## Login Sync

cfr-doorhanger-sync-logins-header = Never Lose a Password Again
cfr-doorhanger-sync-logins-body = Securely store and sync your passwords to all your devices.
cfr-doorhanger-sync-logins-ok-button = Turn on { -sync-brand-short-name }
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

## Social Tracking Protection

cfr-doorhanger-socialtracking-ok-button = See Protections
  .accesskey = P
cfr-doorhanger-socialtracking-close-button = Close
  .accesskey = C
cfr-doorhanger-socialtracking-dont-show-again = Don’t show me messages like this again
  .accesskey = D
cfr-doorhanger-socialtracking-heading = { -brand-short-name } stopped a social network from tracking you here
cfr-doorhanger-socialtracking-description = Your privacy matters. { -brand-short-name } now blocks common social media trackers, limiting how much data they can collect about what you do online.
cfr-doorhanger-fingerprinters-heading = { -brand-short-name } blocked a fingerprinter on this page
cfr-doorhanger-fingerprinters-description = Your privacy matters. { -brand-short-name } now blocks fingerprinters, which collect pieces of uniquely identifiable information about your device to track you.
cfr-doorhanger-cryptominers-heading = { -brand-short-name } blocked a cryptominer on this page
cfr-doorhanger-cryptominers-description = Your privacy matters. { -brand-short-name } now blocks cryptominers, which use your system’s computing power to mine digital money.

## Enhanced Tracking Protection Milestones

# Variables:
#   $blockedCount (Number) - The total count of blocked trackers. This number will always be greater than 1.
#   $date (String) - The date we began recording the count of blocked trackers
cfr-doorhanger-milestone-heading =
  { $blockedCount ->
   *[other] { -brand-short-name } blocked over <b>{ $blockedCount }</b> trackers since { $date }!
  }
cfr-doorhanger-milestone-ok-button = See All
  .accesskey = S
