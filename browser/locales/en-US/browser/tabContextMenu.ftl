# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

tab-context-new-tab =
    .label = New Tab
    .accesskey = w
reload-tab =
    .label = Reload Tab
    .accesskey = R
select-all-tabs =
    .label = Select All Tabs
    .accesskey = S
tab-context-play-tab =
    .label = Play Tab
    .accesskey = l
tab-context-play-tabs =
    .label = Play Tabs
    .accesskey = y
duplicate-tab =
    .label = Duplicate Tab
    .accesskey = D
duplicate-tabs =
    .label = Duplicate Tabs
    .accesskey = D
# The following string is displayed on a menuitem that will close the tabs from the start of the tabstrip to the currently targeted tab (excluding the currently targeted and any other selected tabs).
# In left-to-right languages this should use "Left" and in right-to-left languages this should use "Right".
close-tabs-to-the-start =
    .label = Close Tabs to Left
    .accesskey = l
# The following string is displayed on a menuitem that will close the tabs from the end of the tabstrip to the currently targeted tab (excluding the currently targeted and any other selected tabs).
# In left-to-right languages this should use "Right" and in right-to-left languages this should use "Left".
close-tabs-to-the-end =
    .label = Close Tabs to Right
    .accesskey = i
close-other-tabs =
    .label = Close Other Tabs
    .accesskey = o
reload-tabs =
    .label = Reload Tabs
    .accesskey = R
pin-tab =
    .label = Pin Tab
    .accesskey = P
unpin-tab =
    .label = Unpin Tab
    .accesskey = p
pin-selected-tabs =
    .label = Pin Tabs
    .accesskey = P
unpin-selected-tabs =
    .label = Unpin Tabs
    .accesskey = p
bookmark-selected-tabs =
    .label = Bookmark Tabs…
    .accesskey = B
tab-context-bookmark-tab =
    .label = Bookmark Tab…
    .accesskey = B
tab-context-open-in-new-container-tab =
    .label = Open in New Container Tab
    .accesskey = e
move-to-start =
    .label = Move to Start
    .accesskey = S
move-to-end =
    .label = Move to End
    .accesskey = E
move-to-new-window =
    .label = Move to New Window
    .accesskey = W
tab-context-close-multiple-tabs =
    .label = Close Multiple Tabs
    .accesskey = M
tab-context-close-duplicate-tabs =
    .label = Close Duplicate Tabs
    .accesskey = u
tab-context-share-url =
    .label = Share
    .accesskey = h

## Variables:
##  $tabCount (Number): the number of tabs that are affected by the action.

tab-context-reopen-closed-tabs =
    .label =
        { $tabCount ->
            [1] Reopen Closed Tab
           *[other] Reopen Closed Tabs
        }
    .accesskey = o
tab-context-close-n-tabs =
    .label =
        { $tabCount ->
            [1] Close Tab
           *[other] Close { $tabCount } Tabs
        }
    .accesskey = C
tab-context-move-tabs =
    .label =
        { $tabCount ->
            [1] Move Tab
           *[other] Move Tabs
        }
    .accesskey = v

tab-context-send-tabs-to-device =
    .label =
        { $tabCount ->
            [1] Send Tab to Device
           *[other] Send { $tabCount } Tabs to Device
        }
    .accesskey = n
