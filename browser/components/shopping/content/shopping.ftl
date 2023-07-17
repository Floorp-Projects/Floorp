# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

### This file is not in a locales directory to prevent it from
### being translated as the feature is still in heavy development
### and strings are likely to change often.

shopping-page-title = { -brand-product-name } Shopping
# Title for page showing where a user can check the
# review quality of online shopping product reviews
shopping-main-container-title = Review quality check
shopping-close-button =
  .title = Close

## Strings for the letter grade component.
## For now, we only support letter grades A, B, C, D and F.
## Letter A indicates the highest grade, and F indicates the lowest grade.
## Letters are hardcoded and cannot be localized.

shopping-letter-grade-description-ab = Reliable reviews
shopping-letter-grade-description-c = Only some reliable reviews
shopping-letter-grade-description-df = Unreliable reviews

# This string is displayed in a tooltip that appears when the user hovers
# over the letter grade component without a visible description.
# It is also used for screen readers.
#  $letter (String) - The letter grade as A, B, C, D or F (hardcoded).
#  $description (String) - The localized letter grade description. See shopping-letter-grade-description-* strings above.
shopping-letter-grade-tooltip =
  .title = { $letter } - { $description }

## Strings for the product review snippets card

shopping-highlights-label =
  .label = Snippets from recent reviews

shopping-highlight-price = Price
shopping-highlight-quality = Quality
shopping-highlight-shipping = Shipping
shopping-highlight-competitiveness = Competitiveness
shopping-highlight-packaging = Packaging

## Strings for the settings card

shopping-settings-label =
  .label = Settings
shopping-settings-recommendations-toggle =
  .label = Show products recommended by { -brand-product-name }
shopping-settings-opt-out-button = Turn off review quality check

## Strings for the adjusted rating component

shopping-adjusted-rating-label =
  .label = Adjusted rating
shopping-adjusted-rating-unreliable-reviews = Unreliable reviews removed

##
