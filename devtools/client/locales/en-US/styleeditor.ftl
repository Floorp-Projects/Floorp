# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

styleeditor-new-button =
    .tooltiptext = Create and append a new style sheet to the document
    .accesskey = N
styleeditor-import-button =
    .tooltiptext = Import and append an existing style sheet to the document
    .accesskey = I
styleeditor-filter-input =
    .placeholder = Filter style sheets
styleeditor-visibility-toggle =
    .tooltiptext = Toggle style sheet visibility
    .accesskey = S
styleeditor-visibility-toggle-system =
    .tooltiptext = System style sheets can’t be disabled
styleeditor-save-button = Save
    .tooltiptext = Save this style sheet to a file
    .accesskey = S
styleeditor-options-button =
    .tooltiptext = Style Editor options
styleeditor-at-rules = At-rules
styleeditor-no-stylesheet = This page has no style sheet.
styleeditor-no-stylesheet-tip = Perhaps you’d like to <a data-l10n-name="append-new-stylesheet">append a new style sheet</a>?
styleeditor-open-link-new-tab =
    .label = Open Link in New Tab
styleeditor-copy-url =
    .label = Copy URL
styleeditor-find =
    .label = Find
    .accesskey = F
styleeditor-find-again =
    .label = Find Again
    .accesskey = g
styleeditor-go-to-line =
    .label = Jump to Line…
    .accesskey = J
# Label displayed when searching a term that is not found in any stylesheet path
styleeditor-stylesheet-all-filtered = No matching style sheet has been found.

# This string is shown in the style sheets list
# Variables:
#   $ruleCount (Integer) - The number of rules in the stylesheet.
styleeditor-stylesheet-rule-count =
    { $ruleCount ->
        [one] { $ruleCount } rule.
       *[other] { $ruleCount } rules.
    }

# Title for the pretty print button in the editor footer.
styleeditor-pretty-print-button =
    .title = Pretty print style sheet

# Title for the pretty print button in the editor footer, when it's disabled
styleeditor-pretty-print-button-disabled =
    .title = Can only pretty print CSS files
