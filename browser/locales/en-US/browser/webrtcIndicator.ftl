# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

## These strings are used so that the window has a title in tools that
## enumerate/look for window titles. It is not normally visible anywhere.

webrtc-indicator-title = { -brand-short-name } — Sharing Indicator
webrtc-indicator-window =
    .title = { -brand-short-name } — Sharing Indicator

##

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

## These strings will display as a tooltip on supported systems where we show
## device sharing state in the OS notification area. We do not use these strings
## on macOS, as global menu bar items do not have native tooltips.

webrtc-camera-system-menu =
    .label = You are sharing your camera. Click to control sharing.
webrtc-microphone-system-menu =
    .label = You are sharing your microphone. Click to control sharing.
webrtc-screen-system-menu =
    .label = You are sharing a window or a screen. Click to control sharing.

## Tooltips used by the legacy global sharing indicator

webrtc-indicator-sharing-camera-and-microphone =
    .tooltiptext = Your camera and microphone are being shared. Click to control sharing.
webrtc-indicator-sharing-camera =
    .tooltiptext = Your camera is being shared. Click to control sharing.
webrtc-indicator-sharing-microphone =
    .tooltiptext = Your microphone is being shared. Click to control sharing.
webrtc-indicator-sharing-application =
    .tooltiptext = An application is being shared. Click to control sharing.
webrtc-indicator-sharing-screen =
    .tooltiptext = Your screen is being shared. Click to control sharing.
webrtc-indicator-sharing-window =
    .tooltiptext = A window is being shared. Click to control sharing.
webrtc-indicator-sharing-browser =
    .tooltiptext = A tab is being shared. Click to control sharing.

## These strings are only used on Mac for menus attached to icons
## near the clock on the mac menubar.
## Variables:
##   $streamTitle (String): the title of the tab using the share.
##   $tabCount (Number): the title of the tab using the share.

webrtc-indicator-menuitem-control-sharing =
    .label = Control Sharing
webrtc-indicator-menuitem-control-sharing-on =
    .label = Control Sharing on “{ $streamTitle }”

webrtc-indicator-menuitem-sharing-camera-with =
    .label = Sharing Camera with “{ $streamTitle }”
webrtc-indicator-menuitem-sharing-camera-with-n-tabs =
    .label =
        { $tabCount ->
            [one] Sharing Camera with { $tabCount } tab
           *[other] Sharing Camera with { $tabCount } tabs
        }

webrtc-indicator-menuitem-sharing-microphone-with =
    .label = Sharing Microphone with “{ $streamTitle }”
webrtc-indicator-menuitem-sharing-microphone-with-n-tabs =
    .label =
        { $tabCount ->
            [one] Sharing Microphone with { $tabCount } tab
           *[other] Sharing Microphone with { $tabCount } tabs
        }

webrtc-indicator-menuitem-sharing-application-with =
    .label = Sharing an Application with “{ $streamTitle }”
webrtc-indicator-menuitem-sharing-application-with-n-tabs =
    .label =
        { $tabCount ->
            [one] Sharing an Application with { $tabCount } tab
           *[other] Sharing Applications with { $tabCount } tabs
        }

webrtc-indicator-menuitem-sharing-screen-with =
    .label = Sharing Screen with “{ $streamTitle }”
webrtc-indicator-menuitem-sharing-screen-with-n-tabs =
    .label =
        { $tabCount ->
            [one] Sharing Screen with { $tabCount } tab
           *[other] Sharing Screen with { $tabCount } tabs
        }

webrtc-indicator-menuitem-sharing-window-with =
    .label = Sharing a Window with “{ $streamTitle }”
webrtc-indicator-menuitem-sharing-window-with-n-tabs =
    .label =
        { $tabCount ->
            [one] Sharing a Window with { $tabCount } tab
           *[other] Sharing Windows with { $tabCount } tabs
        }

webrtc-indicator-menuitem-sharing-browser-with =
    .label = Sharing a Tab with “{ $streamTitle }”
# This message is shown when the contents of a tab is shared during a WebRTC
# session, which currently is only possible with Loop/Hello.
webrtc-indicator-menuitem-sharing-browser-with-n-tabs =
    .label =
        { $tabCount ->
            [one] Sharing a Tab with { $tabCount } tab
           *[other] Sharing Tabs with { $tabCount } tabs
        }
