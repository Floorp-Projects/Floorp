# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

## Messages used as headers in the main pane

compatibility-selected-element-header = Selected Element
compatibility-all-elements-header = All Issues

## Message used as labels for the type of issue

compatibility-issue-deprecated = (deprecated)
compatibility-issue-experimental = (experimental)
compatibility-issue-prefixneeded = (prefix needed)
compatibility-issue-deprecated-experimental = (deprecated, experimental)
compatibility-issue-deprecated-prefixneeded = (deprecated, prefix needed)
compatibility-issue-experimental-prefixneeded = (experimental, prefix needed)
compatibility-issue-deprecated-experimental-prefixneeded = (deprecated, experimental, prefix needed)

## Messages used as labels and titles for buttons in the footer

compatibility-settings-button-label = Settings
compatibility-settings-button-title =
    .title = Settings

## Messages used as headers in settings pane

compatibility-settings-header = Settings
compatibility-target-browsers-header = Target Browsers

##

# Text used as the label for the number of nodes where the issue occurred
# Variables:
#   $number (Number) - The number of nodes where the issue occurred
compatibility-issue-occurrences =
    { $number ->
        [one] { $number } occurrence
       *[other] { $number } occurrences
    }

compatibility-no-issues-found = No compatibility issues found.
compatibility-close-settings-button =
    .title = Close settings

# Text used in the element containing the browser icons for a given compatibility issue.
# Line breaks are significant.
# Variables:
#   $browsers (String) - A line-separated list of browser information (e.g. Firefox 98\nChrome 99).
compatibility-issue-browsers-list =
    .title = Compatibility issues in:
    { $browsers }
