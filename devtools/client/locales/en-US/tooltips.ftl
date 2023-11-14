# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

### Localization for Developer Tools tooltips.

learn-more = <span data-l10n-name="link">Learn more</span>

## In the Rule View when a CSS property cannot be successfully applied we display
## an icon. When this icon is hovered this message is displayed to explain why
## the property is not applied.
## Variables:
##   $property (string) - A CSS property name e.g. "color".
##   $display (string) - A CSS display value e.g. "inline-block".

inactive-css-not-grid-or-flex-container = <strong>{ $property }</strong> has no effect on this element since it’s neither a flex container nor a grid container.

inactive-css-not-grid-or-flex-container-or-multicol-container = <strong>{ $property }</strong> has no effect on this element since it’s not a flex container, a grid container, or a multi-column container.

inactive-css-not-multicol-container = <strong>{ $property }</strong> has no effect on this element since it’s not a multi-column container.

inactive-css-not-grid-or-flex-item = <strong>{ $property }</strong> has no effect on this element since it’s not a grid or flex item.

inactive-css-not-grid-item = <strong>{ $property }</strong> has no effect on this element since it’s not a grid item.

inactive-css-not-grid-container = <strong>{ $property }</strong> has no effect on this element since it’s not a grid container.

inactive-css-not-flex-item = <strong>{ $property }</strong> has no effect on this element since it’s not a flex item.

inactive-css-not-flex-container = <strong>{ $property }</strong> has no effect on this element since it’s not a flex container.

inactive-css-not-inline-or-tablecell = <strong>{ $property }</strong> has no effect on this element since it’s not an inline or table-cell element.

inactive-css-first-line-pseudo-element-not-supported = <strong>{ $property }</strong> is not supported on ::first-line pseudo-elements.

inactive-css-first-letter-pseudo-element-not-supported = <strong>{ $property }</strong> is not supported on ::first-letter pseudo-elements.

inactive-css-placeholder-pseudo-element-not-supported = <strong>{ $property }</strong> is not supported on ::placeholder pseudo-elements.

inactive-css-property-because-of-display = <strong>{ $property }</strong> has no effect on this element since it has a display of <strong>{ $display }</strong>.

inactive-css-not-display-block-on-floated = The <strong>display</strong> value has been changed by the engine to <strong>block</strong> because the element is <strong>floated</strong>.

inactive-css-property-is-impossible-to-override-in-visited = It’s impossible to override <strong>{ $property }</strong> due to <strong>:visited</strong> restriction.

inactive-css-position-property-on-unpositioned-box = <strong>{ $property }</strong> has no effect on this element since it’s not a positioned element.

inactive-text-overflow-when-no-overflow = <strong>{ $property }</strong> has no effect on this element since <strong>overflow:hidden</strong> is not set.

inactive-css-not-for-internal-table-elements = <strong>{ $property }</strong> has no effect on internal table elements.

inactive-css-not-for-internal-table-elements-except-table-cells = <strong>{ $property }</strong> has no effect on internal table elements except table cells.

inactive-css-not-table = <strong>{ $property }</strong> has no effect on this element since it’s not a table.

inactive-css-not-table-cell = <strong>{ $property }</strong> has no effect on this element since it’s not a table cell.

inactive-scroll-padding-when-not-scroll-container = <strong>{ $property }</strong> has no effect on this element since it doesn’t scroll.

inactive-css-border-image = <strong>{ $property }</strong> has no effect on this element since it cannot be applied to internal table elements where <strong>border-collapse</strong> is set to <strong>collapse</strong> on the parent table element.

inactive-css-ruby-element = <strong>{ $property }</strong> has no effect on this element since it is a ruby element. Its size is determined by the font size of the ruby text.

inactive-css-highlight-pseudo-elements-not-supported = <strong>{ $property }</strong> is not supported on highlight pseudo-elements.

inactive-css-cue-pseudo-element-not-supported = <strong>{ $property }</strong> is not supported on ::cue pseudo-elements.

# Variables:
#   $lineCount (integer) - The number of lines the element has.
inactive-css-text-wrap-balance-lines-exceeded =
    { $lineCount ->
         [one] <strong>{ $property }</strong> has no effect on this element because it has more than { $lineCount } line.
        *[other] <strong>{ $property }</strong> has no effect on this element because it has more than { $lineCount } lines.
     }

inactive-css-text-wrap-balance-fragmented = <strong>{ $property }</strong> has no effect on this element because it is fragmented, i.e. its content is split across multiple columns or pages.

## In the Rule View when a CSS property cannot be successfully applied we display
## an icon. When this icon is hovered this message is displayed to explain how
## the problem can be solved.

inactive-css-not-grid-or-flex-container-fix = Try adding <strong>display:grid</strong> or <strong>display:flex</strong>. { learn-more }

inactive-css-not-grid-or-flex-container-or-multicol-container-fix = Try adding either <strong>display:grid</strong>, <strong>display:flex</strong>, or <strong>columns:2</strong>. { learn-more }

inactive-css-not-multicol-container-fix = Try adding either <strong>column-count</strong> or <strong>column-width</strong>. { learn-more }

inactive-css-not-grid-or-flex-item-fix-3 = Try adding <strong>display:grid</strong>, <strong>display:flex</strong>, <strong>display:inline-grid</strong>, or <strong>display:inline-flex</strong> to the element’s parent. { learn-more }

inactive-css-not-grid-item-fix-2 =Try adding <strong>display:grid</strong> or <strong>display:inline-grid</strong> to the element’s parent. { learn-more }

inactive-css-not-grid-container-fix = Try adding <strong>display:grid</strong> or <strong>display:inline-grid</strong>. { learn-more }

inactive-css-not-flex-item-fix-2 = Try adding <strong>display:flex</strong> or <strong>display:inline-flex</strong> to the element’s parent. { learn-more }

inactive-css-not-flex-container-fix = Try adding <strong>display:flex</strong> or <strong>display:inline-flex</strong>. { learn-more }

inactive-css-not-inline-or-tablecell-fix = Try adding <strong>display:inline</strong> or <strong>display:table-cell</strong>. { learn-more }

inactive-css-non-replaced-inline-or-table-row-or-row-group-fix = Try adding <strong>display:inline-block</strong> or <strong>display:block</strong>. { learn-more }

inactive-css-non-replaced-inline-or-table-column-or-column-group-fix = Try adding <strong>display:inline-block</strong>. { learn-more }

inactive-css-not-display-block-on-floated-fix = Try removing <strong>float</strong> or adding <strong>display:block</strong>. { learn-more }

inactive-css-position-property-on-unpositioned-box-fix = Try setting its <strong>position</strong> property to something other than <strong>static</strong>. { learn-more }

inactive-text-overflow-when-no-overflow-fix = Try adding <strong>overflow:hidden</strong>. { learn-more }

inactive-css-not-for-internal-table-elements-fix = Try setting its <strong>display</strong> property to something else than <strong>table-cell</strong>, <strong>table-column</strong>, <strong>table-row</strong>, <strong>table-column-group</strong>, <strong>table-row-group</strong>, or <strong>table-footer-group</strong>. { learn-more }

inactive-css-not-for-internal-table-elements-except-table-cells-fix = Try setting its <strong>display</strong> property to something else than <strong>table-column</strong>, <strong>table-row</strong>, <strong>table-column-group</strong>, <strong>table-row-group</strong>, or <strong>table-footer-group</strong>. { learn-more }

inactive-css-not-table-fix = Try adding <strong>display:table</strong> or <strong>display:inline-table</strong>. { learn-more }

inactive-css-not-table-cell-fix = Try adding <strong>display:table-cell</strong>. { learn-more }

inactive-scroll-padding-when-not-scroll-container-fix = Try adding <strong>overflow:auto</strong>, <strong>overflow:scroll</strong>, or <strong>overflow:hidden</strong>. { learn-more }

inactive-css-border-image-fix = On the parent table element, remove the property or change the value of <strong>border-collapse</strong> to a value other than <strong>collapse</strong>. { learn-more }

inactive-css-ruby-element-fix = Try changing the <strong>font-size</strong> of the ruby text. { learn-more }

inactive-css-text-wrap-balance-lines-exceeded-fix = Try to reduce the number of lines. { learn-more }

inactive-css-text-wrap-balance-fragmented-fix = Avoid splitting the element’s content e.g. by removing the columns or by using <strong>page-break-inside:avoid</strong>. { learn-more }

## In the Rule View when a CSS property may have compatibility issues with other browsers
## we display an icon. When this icon is hovered this message is displayed to explain why
## the property is incompatible and the platforms it is incompatible on.
## Variables:
##   $property (string) - A CSS declaration name e.g. "-moz-user-select" that can be a platform specific alias.
##   $rootProperty (string) - A raw CSS property name e.g. "user-select" that is not a platform specific alias.

css-compatibility-default-message = <strong>{ $property }</strong> is not supported in the following browsers:

css-compatibility-deprecated-experimental-message = <strong>{ $property }</strong> was an experimental property that is now deprecated by W3C standards. It is not supported in the following browsers:

css-compatibility-deprecated-experimental-supported-message = <strong>{ $property }</strong> was an experimental property that is now deprecated by W3C standards.

css-compatibility-deprecated-message = <strong>{ $property }</strong> is deprecated by W3C standards. It is not supported in the following browsers:

css-compatibility-deprecated-supported-message = <strong>{ $property }</strong> is deprecated by W3C standards.

css-compatibility-experimental-message = <strong>{ $property }</strong> is an experimental property. It is not supported in the following browsers:

css-compatibility-experimental-supported-message = <strong>{ $property }</strong> is an experimental property.

css-compatibility-learn-more-message = <span data-l10n-name="link">Learn more</span> about <strong>{ $rootProperty }</strong>

## In the Rule View when a rule selector can causes issues, we display an icon.
## When this icon is hovered one or more of those messages are displayed to explain what
## the issue are.

# :has() should not be translated
css-selector-warning-unconstrained-has = This selector uses unconstrained <strong>:has()</strong>, which can be slow
