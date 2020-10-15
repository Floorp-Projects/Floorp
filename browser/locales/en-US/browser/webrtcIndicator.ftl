# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Note: This is currently placed under browser/base/content so that we can
# get the strings to appear without having our localization community need
# to go through and translate everything. Once these strings are ready for
# translation, we'll move it to the locales folder.

# This string is used so that the window has a title in tools that enumerate/look for window
# titles. It is not normally visible anywhere.
webrtc-indicator-title = { -brand-short-name } — Sharing Indicator

webrtc-sharing-window = You are sharing another application window.
webrtc-sharing-browser-window = You are sharing { -brand-short-name }.
webrtc-sharing-screen = You are sharing your entire screen.
webrtc-stop-sharing-button = Stop Sharing
webrtc-microphone-unmuted =
  .title = Turn microphone off
webrtc-microphone-muted =
  .title = Turn microphone on
webrtc-camera-unmuted =
  .title = Turn camera off
webrtc-camera-muted =
  .title = Turn camera on
webrtc-minimize =
  .title = Minimize indicator

# This string will display as a tooltip on supported systems where we show
# device sharing state in the OS notification area. We do not use these strings
# on macOS, as global menu bar items do not have native tooltips.
webrtc-camera-system-menu =
  .label = You are sharing your camera. Click to control sharing.
webrtc-microphone-system-menu =
  .label = You are sharing your microphone. Click to control sharing.
webrtc-screen-system-menu =
  .label = You are sharing a window or a screen. Click to control sharing.
