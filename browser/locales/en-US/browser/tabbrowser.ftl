# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

tabbrowser-empty-tab-title = New Tab
tabbrowser-empty-private-tab-title = New Private Tab

tabbrowser-menuitem-close-tab =
    .label = Close Tab
tabbrowser-menuitem-close =
    .label = Close

# Displayed as a tooltip on container tabs
# Variables:
#   $title (String): the title of the current tab.
#   $containerName (String): the name of the current container.
tabbrowser-container-tab-title = { $title } — { $containerName }

# Variables:
#   $tabCount (Number): The number of tabs that will be closed.
tabbrowser-close-tabs-tooltip =
    .label =
        { $tabCount ->
            [one] Close tab
           *[other] Close { $tabCount } tabs
        }

## Tooltips for tab audio control
## Variables:
##   $tabCount (Number): The number of tabs that will be affected.

# Variables:
#   $shortcut (String): The keyboard shortcut for "Mute tab".
tabbrowser-mute-tab-audio-tooltip =
    .label =
        { $tabCount ->
            [one] Mute tab ({ $shortcut })
           *[other] Mute { $tabCount } tabs ({ $shortcut })
        }
# Variables:
#   $shortcut (String): The keyboard shortcut for "Unmute tab".
tabbrowser-unmute-tab-audio-tooltip =
    .label =
        { $tabCount ->
            [one] Unmute tab ({ $shortcut })
           *[other] Unmute { $tabCount } tabs ({ $shortcut })
        }
tabbrowser-mute-tab-audio-background-tooltip =
    .label =
        { $tabCount ->
            [one] Mute tab
           *[other] Mute { $tabCount } tabs
        }
tabbrowser-unmute-tab-audio-background-tooltip =
    .label =
        { $tabCount ->
            [one] Unmute tab
           *[other] Unmute { $tabCount } tabs
        }
tabbrowser-unblock-tab-audio-tooltip =
    .label =
        { $tabCount ->
            [one] Play tab
           *[other] Play { $tabCount } tabs
        }

## Confirmation dialog when closing a window with more than one tab open,
## or when quitting when only one window is open.

# The singular form is not considered since this string is used only for multiple tabs.
# Variables:
#   $tabCount (Number): The number of tabs that will be closed.
tabbrowser-confirm-close-tabs-title =
    { $tabCount ->
       *[other] Close { $tabCount } tabs?
    }
tabbrowser-confirm-close-tabs-button = Close tabs
tabbrowser-confirm-close-tabs-checkbox = Confirm before closing multiple tabs

## Confirmation dialog when quitting using the menu and multiple windows are open.

# The forms for 0 or 1 items are not considered since this string is used only for
# multiple windows.
# Variables:
#   $windowCount (Number): The number of windows that will be closed.
tabbrowser-confirm-close-windows-title =
    { $windowCount ->
       *[other] Close { $windowCount } windows?
    }
tabbrowser-confirm-close-windows-button =
    { PLATFORM() ->
        [windows] Close and exit
       *[other] Close and quit
    }

## Confirmation dialog when quitting using the keyboard shortcut (Ctrl/Cmd+Q)
## Windows does not show a prompt on quit when using the keyboard shortcut by default.

tabbrowser-confirm-close-tabs-with-key-title = Close window and quit { -brand-short-name }?
tabbrowser-confirm-close-tabs-with-key-button = Quit { -brand-short-name }
# Variables:
#   $quitKey (String): the text of the keyboard shortcut for quitting.
tabbrowser-confirm-close-tabs-with-key-checkbox = Confirm before quitting with { $quitKey }

## Confirmation dialog when opening multiple tabs simultaneously

tabbrowser-confirm-open-multiple-tabs-title = Confirm open
# Variables:
#   $tabCount (Number): The number of tabs that will be opened.
tabbrowser-confirm-open-multiple-tabs-message =
    { $tabCount ->
       *[other] You are about to open { $tabCount } tabs. This might slow down { -brand-short-name } while the pages are loading. Are you sure you want to continue?
    }
tabbrowser-confirm-open-multiple-tabs-button = Open tabs
tabbrowser-confirm-open-multiple-tabs-checkbox = Warn me when opening multiple tabs might slow down { -brand-short-name }

## Confirmation dialog for enabling caret browsing

tabbrowser-confirm-caretbrowsing-title = Caret Browsing
tabbrowser-confirm-caretbrowsing-message = Pressing F7 turns Caret Browsing on or off. This feature places a moveable cursor in web pages, allowing you to select text with the keyboard. Do you want to turn Caret Browsing on?
tabbrowser-confirm-caretbrowsing-checkbox = Do not show me this dialog box again.

##

# Variables:
#   $domain (String): URL of the page that is trying to steal focus.
tabbrowser-allow-dialogs-to-get-focus =
    .label = Allow notifications like this from { $domain } to take you to their tab

tabbrowser-customizemode-tab-title = Customize { -brand-short-name }

## Context menu buttons, of which only one will be visible at a time

tabbrowser-context-mute-tab =
    .label = Mute Tab
    .accesskey = M
tabbrowser-context-unmute-tab =
    .label = Unmute Tab
    .accesskey = m
# The accesskey should match the accesskey for tabbrowser-context-mute-tab
tabbrowser-context-mute-selected-tabs =
    .label = Mute Tabs
    .accesskey = M
# The accesskey should match the accesskey for tabbrowser-context-unmute-tab
tabbrowser-context-unmute-selected-tabs =
    .label = Unmute Tabs
    .accesskey = m

# This string is used as an additional tooltip and accessibility description for tabs playing audio
tabbrowser-tab-audio-playing-description = Playing audio

## Ctrl-Tab dialog

# Variables:
#   $tabCount (Number): The number of tabs in the current browser window. It will always be 2 at least.
tabbrowser-ctrl-tab-list-all-tabs =
    .label =
        { $tabCount ->
           *[other] List All { $tabCount } Tabs
        }

## Tab manager menu buttons

tabbrowser-manager-mute-tab =
  .tooltiptext = Mute tab
tabbrowser-manager-unmute-tab =
  .tooltiptext = Unmute tab
tabbrowser-manager-close-tab =
  .tooltiptext = Close tab
