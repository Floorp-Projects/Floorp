# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

## App Menu

appmenuitem-update-banner =
    .label-update-downloading = Downloading { -brand-shorter-name } update
appmenuitem-protection-dashboard-title = Protections Dashboard
appmenuitem-new-window =
    .label = New Window
appmenuitem-new-private-window =
    .label = New Private Window
appmenuitem-passwords =
    .label = Passwords
appmenuitem-extensions-and-themes =
    .label = Extensions and Themes
appmenuitem-find-in-page =
    .label = Find In Page…
appmenuitem-more-tools =
    .label = More Tools
appmenuitem-exit =
    .label = Exit
appmenu-menu-button-closed =
    .tooltiptext = Open Application Menu
    .label = { -brand-shorter-name }
appmenu-menu-button-opened =
    .tooltiptext = Close Application Menu
    .label = { -brand-shorter-name }

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
  .label = Full Screen

## Firefox Account toolbar button and Sync panel in App menu.

appmenuitem-fxa-toolbar-sync-now2 = Sync Now
appmenuitem-fxa-manage-account = Manage Account
appmenu-fxa-header =
    .title = { -fxaccount-brand-name }
# Variables
# $time (string) - Localized relative time since last sync (e.g. 1 second ago,
# 3 hours ago, etc.)
appmenu-fxa-last-sync = Last synced { $time }
    .label = Last synced { $time }
appmenu-fxa-sync-and-save-data =
    .value = Sync and Save Data
appmenu-fxa-signed-in-label = Sign In
appmenu-fxa-setup-sync =
    .label = Turn On Syncing…
appmenu-fxa-show-more-tabs = Show More Tabs

appmenuitem-save-page =
    .label = Save Page As…

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
    .label = Manage History
appmenu-reopen-all-tabs = Reopen All Tabs
appmenu-reopen-all-windows = Reopen All Windows

## Help panel

appmenu-help-header =
    .title = { -brand-shorter-name } Help
appmenu-about =
    .label = About { -brand-shorter-name }
    .accesskey = A
appmenu-get-help =
    .label = Get Help
    .accesskey = H
appmenu-help-more-troubleshooting-info =
    .label = More Troubleshooting Information
    .accesskey = T
appmenu-help-taskmanager =
    .label = Task Manager
appmenu-help-report-site-issue =
    .label = Report Site Issue…
appmenu-help-feedback-page =
    .label = Submit Feedback…
    .accesskey = S

## appmenu-help-enter-troubleshoot-mode and appmenu-help-exit-troubleshoot-mode
## are mutually exclusive, so it's possible to use the same accesskey for both.

appmenu-help-enter-troubleshoot-mode =
    .label = Troubleshoot Mode
    .accesskey = M
appmenu-help-exit-troubleshoot-mode =
    .label = Turn Troubleshoot Mode Off
    .accesskey = M

## appmenu-help-report-deceptive-site and appmenu-help-not-deceptive
## are mutually exclusive, so it's possible to use the same accesskey for both.

appmenu-help-report-deceptive-site =
    .label = Report Deceptive Site…
    .accesskey = D
appmenu-help-not-deceptive =
    .label = This Isn’t a Deceptive Site…
    .accesskey = D

## More Tools

appmenu-customizetoolbar =
    .label = Customize Toolbar…

appmenu-developer-tools-subheader = Browser Tools
