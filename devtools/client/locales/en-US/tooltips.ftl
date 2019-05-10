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

inactive-css-not-grid-or-flex-container = <strong>{ $property }</strong> has no effect on this element since it’s neither a flex container nor a grid container.

inactive-css-not-grid-or-flex-item = <strong>{ $property }</strong> has no effect on this element since it’s not a grid or flex item.

inactive-css-not-grid-item = <strong>{ $property }</strong> has no effect on this element since it’s not a grid item.

inactive-css-not-grid-container = <strong>{ $property }</strong> has no effect on this element since it’s not a grid container.

inactive-css-not-flex-item = <strong>{ $property }</strong> has no effect on this element since it’s not a flex item.

inactive-css-not-flex-container = <strong>{ $property }</strong> has no effect on this element since it’s not a flex container.

## In the Rule View when a CSS property cannot be successfully applied we display
## an icon. When this icon is hovered this message is displayed to explain how
## the problem can be solved.

inactive-css-not-grid-or-flex-container-fix = Try adding <strong>display:grid</strong> or <strong>display:flex</strong>. { learn-more }

inactive-css-not-grid-or-flex-item-fix = Try adding <strong>display:grid</strong>, <strong>display:flex</strong>, <strong>display:inline-grid</strong> or <strong>display:inline-flex</strong>. { learn-more }

inactive-css-not-grid-item-fix =Try adding <strong>display:grid</strong> or <strong>display:inline-grid</strong> to the item’s parent. { learn-more }

inactive-css-not-grid-container-fix = Try adding <strong>display:grid</strong> or <strong>display:inline-grid</strong>. { learn-more }

inactive-css-not-flex-item-fix = Try adding <strong>display:flex</strong> or <strong>display:inline-flex</strong> to the item’s parent. { learn-more }

inactive-css-not-flex-container-fix = Try adding <strong>display:flex</strong> or <strong>display:inline-flex</strong>. { learn-more }
