# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

## App Menu

appmenuitem-update-banner =
    .label-update-downloading = Downloading { -brand-shorter-name } update
appmenuitem-protection-dashboard-title = Protections Dashboard
appmenuitem-customize-mode =
    .label = Customize…
appmenuitem-new-window =
    .label = New Window
appmenuitem-new-private-window =
    .label = New Private Window

## Zoom and Fullscreen Controls

appmenuitem-zoom-enlarge =
  .label = Zoom in
appmenuitem-zoom-reduce =
  .label = Zoom out
appmenuitem-fullscreen =
  .label = Full Screen

## Firefox Account toolbar button and Sync panel in App menu.

fxa-toolbar-sync-now =
    .label = Sync Now

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

## Help panel

appmenu-about =
    .label = About { -brand-shorter-name }
    .accesskey = A
appmenu-help-product =
    .label = { -brand-shorter-name } Help
    .accesskey = H
appmenu-help-show-tour =
    .label = { -brand-shorter-name } Tour
    .accesskey = o
appmenu-help-import-from-another-browser =
    .label = Import From Another Browser…
    .accesskey = I
appmenu-help-keyboard-shortcuts =
    .label = Keyboard Shortcuts
    .accesskey = K
appmenu-help-troubleshooting-info =
    .label = Troubleshooting Information
    .accesskey = T
appmenu-help-feedback-page =
    .label = Submit Feedback…
    .accesskey = S

## appmenu-help-safe-mode-without-addons and appmenu-help-safe-mode-without-addons
## are mutually exclusive, so it's possible to use the same accesskey for both.

appmenu-help-safe-mode-without-addons =
    .label = Restart With Add-ons Disabled…
    .accesskey = R
appmenu-help-safe-mode-with-addons =
    .label = Restart With Add-ons Enabled
    .accesskey = R

## appmenu-help-report-deceptive-site and appmenu-help-not-deceptive
## are mutually exclusive, so it's possible to use the same accesskey for both.

appmenu-help-report-deceptive-site =
    .label = Report Deceptive Site…
    .accesskey = D
appmenu-help-not-deceptive =
    .label = This Isn’t a Deceptive Site…
    .accesskey = D

##

appmenu-help-check-for-update =
    .label = Check for Updates…
