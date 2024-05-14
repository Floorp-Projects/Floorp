# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

## These strings are used so that the window has a title in tools that
## enumerate/look for window titles. It is not normally visible anywhere.

webrtc-indicator-title = { -brand-short-name } — Sharing Indicator

## Used as list items in sharing menu

webrtc-item-camera = camera
webrtc-item-microphone = microphone
webrtc-item-audio-capture = tab audio
webrtc-item-application = application
webrtc-item-screen = screen
webrtc-item-window = window
webrtc-item-browser = tab

##

# This is used for the website origin for the sharing menu if no readable origin could be deduced from the URL.
webrtc-sharing-menuitem-unknown-host = Unknown origin

# Variables:
#   $origin (String): The website origin (e.g. www.mozilla.org)
#   $itemList (String): A formatted list of items (e.g. "camera, microphone and tab audio")
webrtc-sharing-menuitem =
    .label = { $origin } ({ $itemList })
webrtc-sharing-menu =
    .label = Tabs sharing devices
    .accesskey = d

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

## Variables:
##   $origin (String): the website origin (e.g. www.mozilla.org).

webrtc-allow-share-audio-capture = Allow { $origin } to listen to this tab’s audio?
webrtc-allow-share-camera = Allow { $origin } to use your camera?
webrtc-allow-share-microphone = Allow { $origin } to use your microphone?
webrtc-allow-share-screen = Allow { $origin } to see your screen?
# "Speakers" is used in a general sense that might include headphones or
# another audio output connection.
webrtc-allow-share-speaker = Allow { $origin } to use other speakers?
webrtc-allow-share-camera-and-microphone = Allow { $origin } to use your camera and microphone?
webrtc-allow-share-camera-and-audio-capture = Allow { $origin } to use your camera and listen to this tab’s audio?
webrtc-allow-share-screen-and-microphone = Allow { $origin } to use your microphone and see your screen?
webrtc-allow-share-screen-and-audio-capture = Allow { $origin } to listen to this tab’s audio and see your screen?

## Special phrasing for sharing devices when the origin is a file url.

webrtc-allow-share-audio-capture-with-file = Allow this local file to listen to this tab’s audio?
webrtc-allow-share-camera-with-file = Allow this local file to use your camera?
webrtc-allow-share-microphone-with-file = Allow this local file to use your microphone?
webrtc-allow-share-screen-with-file = Allow this local file to see your screen?
# "Speakers" is used in a general sense that might include headphones or
# another audio output connection.
webrtc-allow-share-speaker-with-file = Allow this local file to use other speakers?
webrtc-allow-share-camera-and-microphone-with-file = Allow this local file to use your camera and microphone?
webrtc-allow-share-camera-and-audio-capture-with-file = Allow this local file to use your camera and listen to this tab’s audio?
webrtc-allow-share-screen-and-microphone-with-file = Allow this local file to use your microphone and see your screen?
webrtc-allow-share-screen-and-audio-capture-with-file = Allow this local file to listen to this tab’s audio and see your screen?

## Variables:
##   $origin (String): the first party origin.
##   $thirdParty (String): the third party origin.

webrtc-allow-share-audio-capture-unsafe-delegation = Allow { $origin } to give { $thirdParty } permission to listen to this tab’s audio?
webrtc-allow-share-camera-unsafe-delegation = Allow { $origin } to give { $thirdParty } access to your camera?
webrtc-allow-share-microphone-unsafe-delegation = Allow { $origin } to give { $thirdParty } access to your microphone?
webrtc-allow-share-screen-unsafe-delegation = Allow { $origin } to give { $thirdParty } permission to see your screen?
# "Speakers" is used in a general sense that might include headphones or
# another audio output connection.
webrtc-allow-share-speaker-unsafe-delegation = Allow { $origin } to give { $thirdParty } access to other speakers?
webrtc-allow-share-camera-and-microphone-unsafe-delegation = Allow { $origin } to give { $thirdParty } access to your camera and microphone?
webrtc-allow-share-camera-and-audio-capture-unsafe-delegation = Allow { $origin } to give { $thirdParty } access to your camera and listen to this tab’s audio?
webrtc-allow-share-screen-and-microphone-unsafe-delegation = Allow { $origin } to give { $thirdParty } access to your microphone and see your screen?
webrtc-allow-share-screen-and-audio-capture-unsafe-delegation = Allow { $origin } to give { $thirdParty } permission to listen to this tab’s audio and see your screen?

##

webrtc-share-screen-warning = Only share screens with sites you trust. Sharing can allow deceptive sites to browse as you and steal your private data.
webrtc-share-browser-warning = Only share { -brand-short-name } with sites you trust. Sharing can allow deceptive sites to browse as you and steal your private data.

webrtc-share-screen-learn-more = Learn more
webrtc-pick-window-or-screen = Select window or screen
webrtc-share-entire-screen = Entire screen
webrtc-share-pipe-wire-portal = Use operating system settings
# Variables:
#   $monitorIndex (String): screen number (digits 1, 2, etc).
webrtc-share-monitor = Screen { $monitorIndex }
# Variables:
#   $windowCount (Number): the number of windows currently displayed by the application.
#   $appName (String): the name of the application.
webrtc-share-application =
    { $windowCount ->
        [one] { $appName } ({ $windowCount } window)
       *[other] { $appName } ({ $windowCount } windows)
    }

## These buttons are the possible answers to the various prompts in the "webrtc-allow-share-*" strings.

webrtc-action-allow =
    .label = Allow
    .accesskey = A
webrtc-action-block =
    .label = Block
    .accesskey = B
webrtc-action-always-block =
    .label = Always block
    .accesskey = w
webrtc-action-not-now =
    .label = Not now
    .accesskey = N

##

webrtc-remember-allow-checkbox = Remember this decision
webrtc-remember-allow-checkbox-camera = Remember for all cameras
webrtc-remember-allow-checkbox-microphone = Remember for all microphones
webrtc-remember-allow-checkbox-camera-and-microphone = Remember for all cameras and microphones
webrtc-mute-notifications-checkbox = Mute website notifications while sharing

webrtc-reason-for-no-permanent-allow-screen = { -brand-short-name } can not allow permanent access to your screen.
webrtc-reason-for-no-permanent-allow-audio = { -brand-short-name } can not allow permanent access to your tab’s audio without asking which tab to share.
webrtc-reason-for-no-permanent-allow-insecure = Your connection to this site is not secure. To protect you, { -brand-short-name } will only allow access for this session.
