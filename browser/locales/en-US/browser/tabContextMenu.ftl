# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

reload-tab =
    .label = Reload Tab
    .accesskey = R
select-all-tabs =
    .label = Select All Tabs
    .accesskey = S
duplicate-tab =
    .label = Duplicate Tab
    .accesskey = D
duplicate-tabs =
    .label = Duplicate Tabs
    .accesskey = D
close-tabs-to-the-end =
    .label = Close Tabs to the Right
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
bookmark-tab =
    .label = Bookmark Tab
    .accesskey = B
reopen-in-container =
    .label = Reopen in Container
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

## Variables:
##  $tabCount (Number): the number of tabs that are affected by the action.
tab-context-undo-close-tabs =
    .label =
        { $tabCount ->
            [1] Undo Close Tab
           *[other] Undo Close Tabs
        }
    .accesskey = U
tab-context-close-tabs =
    .label =
        { $tabCount ->
            [1] Close Tab
           *[other] Close Tabs
        }
    .accesskey = C
tab-context-move-tabs =
    .label =
        { $tabCount ->
            [1] Move Tab
           *[other] Move Tabs
        }
    .accesskey = v
