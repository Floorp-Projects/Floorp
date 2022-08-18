# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

navbar-tooltip-instruction =
    .value = { PLATFORM() ->
        [macos] Pull down to show history
       *[other] Right-click or pull down to show history
    }

## Back

# Variables
#   $shortcut (String) - A keyboard shortcut for the Go Back command.
main-context-menu-back-2 =
    .tooltiptext = Go back one page ({ $shortcut })
    .aria-label = Back
    .accesskey = B

# This menuitem is only visible on macOS
main-context-menu-back-mac =
    .label = Back
    .accesskey = B

navbar-tooltip-back-2 =
    .value = { main-context-menu-back-2.tooltiptext }

toolbar-button-back-2 =
    .label = { main-context-menu-back-2.aria-label }

## Forward

# Variables
#   $shortcut (String) - A keyboard shortcut for the Go Forward command.
main-context-menu-forward-2 =
    .tooltiptext = Go forward one page ({ $shortcut })
    .aria-label = Forward
    .accesskey = F

# This menuitem is only visible on macOS
main-context-menu-forward-mac =
    .label = Forward
    .accesskey = F

navbar-tooltip-forward-2 =
    .value = { main-context-menu-forward-2.tooltiptext }

toolbar-button-forward-2 =
    .label = { main-context-menu-forward-2.aria-label }

## Reload

main-context-menu-reload =
    .aria-label = Reload
    .accesskey = R

# This menuitem is only visible on macOS
main-context-menu-reload-mac =
    .label = Reload
    .accesskey = R

toolbar-button-reload =
    .label = { main-context-menu-reload.aria-label }

## Stop

main-context-menu-stop =
    .aria-label = Stop
    .accesskey = S

# This menuitem is only visible on macOS
main-context-menu-stop-mac =
    .label = Stop
    .accesskey = S

toolbar-button-stop =
    .label = { main-context-menu-stop.aria-label }

## Stop-Reload Button

toolbar-button-stop-reload =
    .title = { main-context-menu-reload.aria-label }

## Firefox Account Button

toolbar-button-fxaccount =
    .label = { -fxaccount-brand-name(capitalization: "sentence") }
    .tooltiptext = { -fxaccount-brand-name(capitalization: "sentence") }

## Save Page

main-context-menu-page-save =
    .label = Save Page As…
    .accesskey = P

## Simple menu items

main-context-menu-bookmark-page =
    .aria-label = Bookmark Page…
    .accesskey = m
    .tooltiptext = Bookmark page

# This menuitem is only visible on macOS
# Cannot be shown at the same time as main-context-menu-edit-bookmark-mac,
# so should probably have the same access key if possible.
main-context-menu-bookmark-page-mac =
    .label = Bookmark Page…
    .accesskey = m

# This menuitem is only visible on macOS
# Cannot be shown at the same time as main-context-menu-bookmark-page-mac,
# so should probably have the same access key if possible.
main-context-menu-edit-bookmark-mac =
    .label = Edit Bookmark…
    .accesskey = m

# Variables
#   $shortcut (String) - A keyboard shortcut for the add bookmark command.
main-context-menu-bookmark-page-with-shortcut =
    .aria-label = Bookmark Page…
    .accesskey = m
    .tooltiptext = Bookmark page ({ $shortcut })

main-context-menu-edit-bookmark =
    .aria-label = Edit Bookmark…
    .accesskey = m
    .tooltiptext = Edit bookmark

# Variables
#   $shortcut (String) - A keyboard shortcut for the edit bookmark command.
main-context-menu-edit-bookmark-with-shortcut =
    .aria-label = Edit Bookmark…
    .accesskey = m
    .tooltiptext = Edit bookmark ({ $shortcut })

main-context-menu-open-link =
    .label = Open Link
    .accesskey = O

main-context-menu-open-link-new-tab =
    .label = Open Link in New Tab
    .accesskey = T

main-context-menu-open-link-container-tab =
    .label = Open Link in New Container Tab
    .accesskey = w

main-context-menu-open-link-new-window =
    .label = Open Link in New Window
    .accesskey = d

main-context-menu-open-link-new-private-window =
    .label = Open Link in New Private Window
    .accesskey = P

main-context-menu-bookmark-link-2 =
    .label = Bookmark Link…
    .accesskey = B

main-context-menu-save-link =
    .label = Save Link As…
    .accesskey = k

main-context-menu-save-link-to-pocket =
    .label = Save Link to { -pocket-brand-name }
    .accesskey = o

## The access keys for "Copy Link" and "Copy Email Address"
## should be the same if possible; the two context menu items
## are mutually exclusive.

main-context-menu-copy-email =
    .label = Copy Email Address
    .accesskey = l

main-context-menu-copy-phone =
    .label = Copy Phone Number
    .accesskey = o

main-context-menu-copy-link-simple =
    .label = Copy Link
    .accesskey = L

## Media (video/audio) controls
##
## The accesskey for "Play" and "Pause" are the
## same because the two context-menu items are
## mutually exclusive.

main-context-menu-media-play =
    .label = Play
    .accesskey = P

main-context-menu-media-pause =
    .label = Pause
    .accesskey = P

##

main-context-menu-media-mute =
    .label = Mute
    .accesskey = M

main-context-menu-media-unmute =
    .label = Unmute
    .accesskey = m

main-context-menu-media-play-speed-2 =
    .label = Speed
    .accesskey = d

main-context-menu-media-play-speed-slow-2 =
    .label = 0.5×

main-context-menu-media-play-speed-normal-2 =
    .label = 1.0×

main-context-menu-media-play-speed-fast-2 =
    .label = 1.25×

main-context-menu-media-play-speed-faster-2 =
    .label = 1.5×

main-context-menu-media-play-speed-fastest-2 =
    .label = 2×

main-context-menu-media-loop =
    .label = Loop
    .accesskey = L

## The access keys for "Show Controls" and "Hide Controls" are the same
## because the two context-menu items are mutually exclusive.

main-context-menu-media-show-controls =
    .label = Show Controls
    .accesskey = C

main-context-menu-media-hide-controls =
    .label = Hide Controls
    .accesskey = C

##

main-context-menu-media-video-fullscreen =
    .label = Full Screen
    .accesskey = F

main-context-menu-media-video-leave-fullscreen =
    .label = Exit Full Screen
    .accesskey = u

# This is used when right-clicking on a video in the
# content area when the Picture-in-Picture feature is enabled.
main-context-menu-media-watch-pip =
    .label = Watch in Picture-in-Picture
    .accesskey = u

main-context-menu-image-reload =
    .label = Reload Image
    .accesskey = R

main-context-menu-image-view-new-tab =
    .label = Open Image in New Tab
    .accesskey = I

main-context-menu-video-view-new-tab =
    .label = Open Video in New Tab
    .accesskey = i

main-context-menu-image-copy =
    .label = Copy Image
    .accesskey = y

main-context-menu-image-copy-link =
    .label = Copy Image Link
    .accesskey = o

main-context-menu-video-copy-link =
    .label = Copy Video Link
    .accesskey = o

main-context-menu-audio-copy-link =
    .label = Copy Audio Link
    .accesskey = o

main-context-menu-image-save-as =
    .label = Save Image As…
    .accesskey = v

main-context-menu-image-email =
    .label = Email Image…
    .accesskey = g

main-context-menu-image-set-image-as-background =
    .label = Set Image as Desktop Background…
    .accesskey = S

main-context-menu-image-copy-text =
    .label = Copy Text From Image
    .accesskey = x

main-context-menu-image-info =
    .label = View Image Info
    .accesskey = f

main-context-menu-image-desc =
    .label = View Description
    .accesskey = D

main-context-menu-video-save-as =
    .label = Save Video As…
    .accesskey = v

main-context-menu-audio-save-as =
    .label = Save Audio As…
    .accesskey = v

main-context-menu-video-take-snapshot =
    .label = Take Snapshot…
    .accesskey = S

main-context-menu-video-email =
    .label = Email Video…
    .accesskey = a

main-context-menu-audio-email =
    .label = Email Audio…
    .accesskey = a

main-context-menu-plugin-play =
    .label = Activate this plugin
    .accesskey = c

main-context-menu-plugin-hide =
    .label = Hide this plugin
    .accesskey = H

main-context-menu-save-to-pocket =
    .label = Save Page to { -pocket-brand-name }
    .accesskey = k

main-context-menu-send-to-device =
    .label = Send Page to Device
    .accesskey = n

## The access keys for "Use Saved Login" and "Use Saved Password"
## should be the same if possible; the two context menu items
## are mutually exclusive.

main-context-menu-use-saved-login =
    .label = Use Saved Login
    .accesskey = o

main-context-menu-use-saved-password =
    .label = Use Saved Password
    .accesskey = o

##

main-context-menu-suggest-strong-password =
    .label = Suggest Strong Password…
    .accesskey = S

main-context-menu-manage-logins2 =
    .label = Manage Logins
    .accesskey = M

main-context-menu-keyword =
    .label = Add a Keyword for this Search…
    .accesskey = K

main-context-menu-link-send-to-device =
    .label = Send Link to Device
    .accesskey = n

main-context-menu-frame =
    .label = This Frame
    .accesskey = h

main-context-menu-frame-show-this =
    .label = Show Only This Frame
    .accesskey = S

main-context-menu-frame-open-tab =
    .label = Open Frame in New Tab
    .accesskey = T

main-context-menu-frame-open-window =
    .label = Open Frame in New Window
    .accesskey = W

main-context-menu-frame-reload =
    .label = Reload Frame
    .accesskey = R

main-context-menu-frame-add-bookmark =
    .label = Bookmark Frame…
    .accesskey = m

main-context-menu-frame-save-as =
    .label = Save Frame As…
    .accesskey = F

main-context-menu-frame-print =
    .label = Print Frame…
    .accesskey = P

main-context-menu-frame-view-source =
    .label = View Frame Source
    .accesskey = V

main-context-menu-frame-view-info =
    .label = View Frame Info
    .accesskey = I

main-context-menu-print-selection-2 =
    .label = Print Selection…
    .accesskey = r

main-context-menu-view-selection-source =
    .label = View Selection Source
    .accesskey = e

main-context-menu-take-screenshot =
    .label = Take Screenshot
    .accesskey = T

main-context-menu-take-frame-screenshot =
    .label = Take Screenshot
    .accesskey = o

main-context-menu-view-page-source =
    .label = View Page Source
    .accesskey = V

main-context-menu-bidi-switch-text =
    .label = Switch Text Direction
    .accesskey = w

main-context-menu-bidi-switch-page =
    .label = Switch Page Direction
    .accesskey = D

main-context-menu-inspect =
    .label = Inspect
    .accesskey = Q

main-context-menu-inspect-a11y-properties =
    .label = Inspect Accessibility Properties

main-context-menu-eme-learn-more =
    .label = Learn more about DRM…
    .accesskey = D

# Variables
#   $containerName (String): The name of the current container
main-context-menu-open-link-in-container-tab =
    .label = Open Link in New { $containerName } Tab
    .accesskey = T

main-context-menu-reveal-password =
    .label = Reveal Password
    .accesskey = v
