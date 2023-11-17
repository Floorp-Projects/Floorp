# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

## App Menu

appmenuitem-banner-update-downloading =
    .label = Downloading { -brand-shorter-name } update

appmenuitem-banner-update-available =
    .label = Update available — download now

appmenuitem-banner-update-manual =
    .label = Update available — download now

appmenuitem-banner-update-unsupported =
    .label = Unable to update — system incompatible

appmenuitem-banner-update-restart =
    .label = Update available — restart now

appmenuitem-new-tab =
    .label = New tab
appmenuitem-new-window =
    .label = New window
appmenuitem-new-private-window =
    .label = New private window
appmenuitem-history =
  .label = History
appmenuitem-downloads =
  .label = Downloads
appmenuitem-passwords =
    .label = Passwords
appmenuitem-addons-and-themes =
    .label = Add-ons and themes
appmenuitem-print =
  .label = Print…
appmenuitem-find-in-page =
    .label = Find in page…
appmenuitem-translate =
    .label = Translate page…
appmenuitem-zoom =
    .value = Zoom
appmenuitem-more-tools =
    .label = More tools
appmenuitem-help =
    .label = Help
appmenuitem-exit2 =
    .label =
        { PLATFORM() ->
            [linux] Quit
           *[other] Exit
        }
appmenu-menu-button-closed2 =
    .tooltiptext = Open application menu
    .label = { -brand-short-name }
appmenu-menu-button-opened2 =
    .tooltiptext = Close application menu
    .label = { -brand-short-name }

# Settings is now used to access the browser settings across all platforms,
# instead of Options or Preferences.
appmenuitem-settings =
    .label = Settings

## Zoom and Fullscreen Controls

appmenuitem-zoom-enlarge =
  .label = Zoom in
appmenuitem-zoom-reduce =
  .label = Zoom out
appmenuitem-fullscreen =
  .label = Full screen

## Firefox Account toolbar button and Sync panel in App menu.

appmenu-remote-tabs-sign-into-sync =
  .label = Sign in to sync…
appmenu-remote-tabs-turn-on-sync =
  .label = Turn on sync…

# This is shown after the tabs list if we can display more tabs by clicking on the button
appmenu-remote-tabs-showmore =
  .label = Show More Tabs
  .tooltiptext = Show more tabs from this device

# This is shown beneath the name of a device when that device has no open tabs
appmenu-remote-tabs-notabs = No open tabs

# This is shown when Sync is configured but syncing tabs is disabled.
appmenu-remote-tabs-tabsnotsyncing = Turn on tab syncing to view a list of tabs from your other devices.

appmenu-remote-tabs-opensettings =
  .label = Settings

# This is shown when Sync is configured but this appears to be the only device attached to
# the account. We also show links to download Firefox for android/ios.
appmenu-remote-tabs-noclients = Want to see your tabs from other devices here?

appmenu-remote-tabs-connectdevice =
  .label = Connect Another Device
appmenu-remote-tabs-welcome = View a list of tabs from your other devices.
appmenu-remote-tabs-unverified = Your account needs to be verified.

appmenuitem-fxa-toolbar-sync-now2 = Sync now
appmenuitem-fxa-sign-in = Sign in to { -brand-product-name }
appmenuitem-fxa-manage-account = Manage account
appmenu-account-header = Account
# Variables
# $time (string) - Localized relative time since last sync (e.g. 1 second ago,
# 3 hours ago, etc.)
appmenu-fxa-last-sync = Last synced { $time }
    .label = Last synced { $time }
appmenu-fxa-sync-and-save-data2 = Sync and save data
appmenu-fxa-signed-in-label = Sign In
appmenu-fxa-setup-sync =
    .label = Turn On Syncing…

appmenuitem-save-page =
    .label = Save page as…

## What's New panel in App menu.

whatsnew-panel-header = What’s New

# Checkbox displayed at the bottom of the What's New panel, allowing users to
# enable/disable What's New notifications.
whatsnew-panel-footer-checkbox =
  .label = Notify about new features
  .accesskey = f

## The Firefox Profiler – The popup is the UI to turn on the profiler, and record
## performance profiles. To enable it go to profiler.firefox.com and click
## "Enable Profiler Menu Button".

profiler-popup-button-idle =
  .label = Profiler
  .tooltiptext = Record a performance profile

profiler-popup-button-recording =
  .label = Profiler
  .tooltiptext = The profiler is recording a profile

profiler-popup-button-capturing =
  .label = Profiler
  .tooltiptext = The profiler is capturing a profile

profiler-popup-header-text = { -profiler-brand-name }

profiler-popup-reveal-description-button =
  .aria-label = Reveal more information

profiler-popup-description-title =
  .value = Record, analyze, share

profiler-popup-description =
  Collaborate on performance issues by publishing profiles to share with your team.

profiler-popup-learn-more-button =
  .label = Learn more

profiler-popup-settings =
  .value = Settings

# This link takes the user to about:profiling, and is only visible with the Custom preset.
profiler-popup-edit-settings-button =
  .label = Edit Settings…

profiler-popup-recording-screen = Recording…

profiler-popup-start-recording-button =
  .label = Start Recording

profiler-popup-discard-button =
  .label = Discard

profiler-popup-capture-button =
  .label = Capture

profiler-popup-start-shortcut =
  { PLATFORM() ->
      [macos] ⌃⇧1
     *[other] Ctrl+Shift+1
  }

profiler-popup-capture-shortcut =
  { PLATFORM() ->
      [macos] ⌃⇧2
     *[other] Ctrl+Shift+2
  }

## Profiler presets
## They are shown in the popup's select box.

# Presets and their l10n IDs are defined in the file
# devtools/client/performance-new/shared/background.jsm.js
# Please take care that the same values are also defined in devtools' perftools.ftl.

profiler-popup-presets-web-developer-description = Recommended preset for most web app debugging, with low overhead.
profiler-popup-presets-web-developer-label =
  .label = Web Developer

profiler-popup-presets-firefox-description = Recommended preset for profiling { -brand-shorter-name }.
profiler-popup-presets-firefox-label =
  .label = { -brand-shorter-name }

profiler-popup-presets-graphics-description = Preset for investigating graphics bugs in { -brand-shorter-name }.
profiler-popup-presets-graphics-label =
  .label = Graphics

profiler-popup-presets-media-description2 = Preset for investigating audio and video bugs in { -brand-shorter-name }.
profiler-popup-presets-media-label =
  .label = Media

profiler-popup-presets-networking-description = Preset for investigating networking bugs in { -brand-shorter-name }.
profiler-popup-presets-networking-label =
  .label = Networking

profiler-popup-presets-power-description = Preset for investigating power use bugs in { -brand-shorter-name }, with low overhead.
# "Power" is used in the sense of energy (electricity used by the computer).
profiler-popup-presets-power-label =
  .label = Power

profiler-popup-presets-custom-label =
  .label = Custom

## History panel

appmenu-manage-history =
    .label = Manage history
appmenu-restore-session =
    .label = Restore previous session
appmenu-clear-history =
    .label = Clear recent history…
appmenu-recent-history-subheader = Recent history
appmenu-recently-closed-tabs =
    .label = Recently closed tabs
appmenu-recently-closed-windows =
    .label = Recently closed windows
# This allows to search through the browser's history.
appmenu-search-history =
    .label = Search history

## Help panel

appmenu-help-header =
    .title = { -brand-shorter-name } help
appmenu-about =
    .label = About { -brand-shorter-name }
    .accesskey = A
appmenu-get-help =
    .label = Get help
    .accesskey = h
appmenu-help-more-troubleshooting-info =
    .label = More troubleshooting information
    .accesskey = t
appmenu-help-report-site-issue =
    .label = Report site issue…
appmenu-help-share-ideas =
    .label = Share ideas and feedback…
    .accesskey = S
appmenu-help-switch-device =
    .label = Switching to a new device

## appmenu-help-enter-troubleshoot-mode and appmenu-help-exit-troubleshoot-mode
## are mutually exclusive, so it's possible to use the same accesskey for both.

appmenu-help-enter-troubleshoot-mode2 =
    .label = Troubleshoot Mode…
    .accesskey = M
appmenu-help-exit-troubleshoot-mode =
    .label = Turn Troubleshoot Mode off
    .accesskey = M

## appmenu-help-report-deceptive-site and appmenu-help-not-deceptive
## are mutually exclusive, so it's possible to use the same accesskey for both.

appmenu-help-report-deceptive-site =
    .label = Report deceptive site…
    .accesskey = d
appmenu-help-not-deceptive =
    .label = This isn’t a deceptive site…
    .accesskey = d

## More Tools

appmenu-customizetoolbar =
    .label = Customize toolbar…

appmenu-developer-tools-subheader = Browser tools
appmenu-developer-tools-extensions =
    .label = Extensions for developers
appmenuitem-report-broken-site =
  .label = Report broken site
