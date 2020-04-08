# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


## The title and aria-label attributes are used by screen readers to describe
## the Downloads Panel.

downloads-window =
  .title = Downloads
downloads-panel =
  .aria-label = Downloads

##

# The style attribute has the width of the Downloads Panel expressed using
# a CSS unit. The longest labels that should fit are usually those of 
# in-progress and blocked downloads.
downloads-panel-list =
  .style = width: 70ch

downloads-cmd-pause = 
    .label = Pause
    .accesskey = P
downloads-cmd-resume = 
    .label = Resume
    .accesskey = R
downloads-cmd-cancel = 
    .tooltiptext = Cancel
downloads-cmd-cancel-panel = 
    .aria-label = Cancel

# This message is only displayed on Windows and Linux devices
downloads-cmd-show-menuitem =
  .label = Open Containing Folder
  .accesskey = F
  
# This message is only displayed on macOS devices
downloads-cmd-show-menuitem-mac =
  .label = Show In Finder
  .accesskey = F

downloads-cmd-show-button =
  .tooltiptext = { PLATFORM() ->
      [macos] Show In Finder
     *[other] Open Containing Folder
  }

downloads-cmd-show-panel =
  .aria-label = { PLATFORM() ->
      [macos] Show In Finder
     *[other] Open Containing Folder
  }
downloads-cmd-show-description =
  .value = { PLATFORM() ->
      [macos] Show In Finder
     *[other] Open Containing Folder
  }

downloads-cmd-show-downloads = 
    .label = Show Downloads Folder
downloads-cmd-retry = 
    .tooltiptext = Retry
downloads-cmd-retry-panel = 
    .aria-label = Retry
downloads-cmd-go-to-download-page = 
    .label = Go To Download Page
    .accesskey = G
downloads-cmd-copy-download-link = 
    .label = Copy Download Link
    .accesskey = L
downloads-cmd-remove-from-history = 
    .label = Remove From History
    .accesskey = e
downloads-cmd-clear-list = 
    .label = Clear Preview Panel
    .accesskey = a
downloads-cmd-clear-downloads = 
    .label = Clear Downloads
    .accesskey = D

# This command is shown in the context menu when downloads are blocked.
downloads-cmd-unblock = 
    .label = Allow Download
    .accesskey = o

# This is the tooltip of the action button shown when malware is blocked.
downloads-cmd-remove-file = 
    .tooltiptext = Remove File

downloads-cmd-remove-file-panel = 
    .aria-label = Remove File

# This is the tooltip of the action button shown when potentially unwanted
# downloads are blocked. This opens a dialog where the user can choose
# whether to unblock or remove the download. Removing is the default option.
downloads-cmd-choose-unblock = 
    .tooltiptext = Remove File or Allow Download

downloads-cmd-choose-unblock-panel = 
    .aria-label = Remove File or Allow Download

# This is the tooltip of the action button shown when uncommon downloads are
# blocked.This opens a dialog where the user can choose whether to open the
# file or remove the download. Opening is the default option.
downloads-cmd-choose-open = 
    .tooltiptext = Open or Remove File

downloads-cmd-choose-open-panel = 
    .aria-label = Open or Remove File

# Displayed when hovering a blocked download, indicates that it's possible to
# show more information for user to take the next action.
downloads-show-more-information = 
    .value = Show more information

# Displayed when hovering a complete download, indicates that it's possible to
# open the file using an app available in the system.
downloads-open-file = 
    .value = Open File

# Displayed when hovering a download which is able to be retried by users,
# indicates that it's possible to download this file again.
downloads-retry-download = 
    .value = Retry Download

# Displayed when hovering a download which is able to be cancelled by users,
# indicates that it's possible to cancel and stop the download.
downloads-cancel-download = 
    .value = Cancel Download

# This string is shown at the bottom of the Downloads Panel when all the
# downloads fit in the available space, or when there are no downloads in
# the panel at all.
downloads-history = 
    .label = Show All Downloads
    .accesskey = S

# This string is shown at the top of the Download Details Panel, to indicate
# that we are showing the details of a single download.
downloads-details = 
    .title = Download Details

downloads-clear-downloads-button = 
    .label = Clear Downloads
    .tooltiptext = Clears completed, canceled and failed downloads

# This string is shown when there are no items in the Downloads view, when it
# is displayed inside a browser tab.
downloads-list-empty = 
    .value = There are no downloads.

# This string is shown when there are no items in the Downloads Panel.
downloads-panel-empty = 
    .value = No downloads for this session.
