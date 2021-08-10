# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

## App Menu

appmenuitem-update-banner3 =
    .label-update-downloading = Downloading { -brand-shorter-name } update
    .label-update-available = Update available — download now
    .label-update-manual = Update available — download now
    .label-update-unsupported = Unable to update — system incompatible
    .label-update-restart = Update available — restart now

appmenuitem-new-tab =
    .label = New tab
appmenuitem-new-window =
    .label = New window
appmenuitem-new-private-window =
    .label = New private window
appmenuitem-passwords =
    .label = Passwords
appmenuitem-addons-and-themes =
    .label = Add-ons and themes
appmenuitem-find-in-page =
    .label = Find in page…
appmenuitem-more-tools =
    .label = More tools
appmenuitem-exit2 =
    .label =
        { PLATFORM() ->
            [linux] Quit
           *[other] Exit
        }
appmenu-menu-button-closed2 =
    .tooltiptext = Open Application Menu
    .label = { -brand-short-name }
appmenu-menu-button-opened2 =
    .tooltiptext = Close Application Menu
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

appmenuitem-fxa-toolbar-sync-now2 = Sync now
appmenuitem-fxa-manage-account = Manage account
appmenu-fxa-header2 = { -fxaccount-brand-name(capitalization: "sentence") }
# Variables
# $time (string) - Localized relative time since last sync (e.g. 1 second ago,
# 3 hours ago, etc.)
appmenu-fxa-last-sync = Last synced { $time }
    .label = Last synced { $time }
appmenu-fxa-sync-and-save-data2 = Sync and save data
appmenu-fxa-signed-in-label = Sign In
appmenu-fxa-setup-sync =
    .label = Turn On Syncing…
appmenu-fxa-show-more-tabs = Show More Tabs

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

profiler-popup-title =
  .value = { -profiler-brand-name }

profiler-popup-reveal-description-button =
  .aria-label = Reveal more information

profiler-popup-description-title =
  .value = Record, analyze, share

profiler-popup-description =
  Collaborate on performance issues by publishing profiles to share with your team.

profiler-popup-learn-more = Learn more

profiler-popup-settings =
  .value = Settings

# This link takes the user to about:profiling, and is only visible with the Custom preset.
profiler-popup-edit-settings = Edit Settings…

profiler-popup-disabled =
  The profiler is currently disabled, most likely due to a Private Browsing window
  being open.

profiler-popup-recording-screen = Recording…

# The profiler presets list is generated elsewhere, but the custom preset is defined
# here only.
profiler-popup-presets-custom =
  .label = Custom

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

## History panel

appmenu-manage-history =
    .label = Manage history
appmenu-reopen-all-tabs = Reopen all tabs
appmenu-reopen-all-windows = Reopen all windows
appmenu-restore-session =
    .label = Restore previous session
appmenu-clear-history =
    .label = Clear recent history…
appmenu-recent-history-subheader = Recent history
appmenu-recently-closed-tabs =
    .label = Recently closed tabs
appmenu-recently-closed-windows =
    .label = Recently closed windows

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
appmenu-help-feedback-page =
    .label = Submit feedback…
    .accesskey = S

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
