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
# This string is for notifying screen reader users that the
# sidebar is still loading data.
shopping-a11y-loading =
  .aria-label = Loading…

more-to-consider-ad-label =
  .label = More to consider

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

## Strings for the shopping message-bar

shopping-message-bar-warning-stale-analysis-title = Updates available
shopping-message-bar-warning-stale-analysis-message = Re-analyze the reviews for this product, so you have the latest info.
shopping-message-bar-warning-stale-analysis-link = Re-analyze reviews

shopping-message-bar-generic-error-title = Something went wrong
shopping-message-bar-generic-error-message = Couldn’t load information. Please try again later.

shopping-message-bar-warning-not-enough-reviews-title = Not enough reviews yet
shopping-message-bar-warning-not-enough-reviews-message = When this product has more reviews, we’ll be able to analyze them.

shopping-message-bar-warning-product-not-available-title = Product is not available
shopping-message-bar-warning-product-not-available-message = If you see that this product is back in stock, report it to us and we’ll work on updating the analysis.
shopping-message-bar-warning-product-not-available-button = Report this product is back in stock

shopping-message-bar-thanks-for-reporting-title = Thanks for reporting!
shopping-message-bar-thanks-for-reporting-message = We should have an updated analysis within 24 hours. Please check back.

shopping-message-bar-warning-product-not-available-reported-title = Analysis coming soon
shopping-message-bar-warning-product-not-available-reported-message = An updated analysis should be ready within 24 hours. Please check back.

shopping-message-bar-warning-offline-title = No network connection
shopping-message-bar-warning-offline-message = Check your network connection. Then, try reloading the page.

shopping-message-bar-analysis-in-progress-title = Analysis coming soon
shopping-message-bar-analysis-in-progress-message = When it’s done, we’ll automatically show the updated info here.

## Strings for the product review snippets card

shopping-highlights-label =
  .label = Snippets from recent reviews

shopping-highlight-price = Price
shopping-highlight-quality = Quality
shopping-highlight-shipping = Shipping
shopping-highlight-competitiveness = Competitiveness
shopping-highlight-packaging = Packaging

## Strings for show more card

shopping-show-more-button = Show more
shopping-show-less-button = Show less

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

## Strings for the review reliability component

shopping-review-reliability-label =
  .label = How reliable are these reviews?

## Strings for the analysis explainer component

shopping-analysis-explainer-label =
  .label = How we determine review quality

shopping-analysis-explainer-intro =
  { -brand-product-name } uses AI technology from { -fakespot-brand-name } to analyze the quality and reliability of product reviews.
  This is only provided to help you assess review quality, not product quality.

shopping-analysis-explainer-grades-intro =
  We assign each product’s reviews a <strong>letter grade</strong> from A to F.
shopping-analysis-explainer-higher-grade-description =
  A higher grade means we believe the reviews are likely from real customers who left honest, unbiased reviews.
shopping-analysis-explainer-lower-grade-description =
  A lower grade means we believe the reviews are likely from paid or biased reviewers.

shopping-analysis-explainer-adjusted-rating-description =
  The <strong>adjusted rating</strong> is based on review quality, with unreliable reviews removed.
shopping-analysis-explainer-highlights-description =
  <strong>Highlights</strong> are pulled from recent Amazon reviews (from the last 80 days), that we believe to be reliable.

shopping-analysis-explainer-learn-more =
  Learn more about <a data-l10n-name="review-quality-url">how { -fakespot-brand-name } determines review quality</a>.

shopping-analysis-explainer-review-grading-scale = Review grading scale:
shopping-analysis-explainer-review-grading-scale-reliable = We believe the reviews to be reliable
shopping-analysis-explainer-review-grading-scale-mixed = We believe there’s a mix of reliable and unreliable reviews
shopping-analysis-explainer-review-grading-scale-unreliable = We believe the reviews are unreliable

## Strings for UrlBar button

shopping-sidebar-open-button =
  .tooltiptext = Open shopping sidebar
shopping-sidebar-close-button =
  .tooltiptext = Close shopping sidebar

## Strings for the unanalyzed product card.

shopping-unanalyzed-product-header = No analysis for these reviews yet
shopping-unanalyzed-product-message = You’re about 60 seconds away from knowing whether you can trust the reviews for this product.
shopping-unanalyzed-product-analyze-link = Analyze reviews

## Onboarding message strings.

shopping-onboarding-headline = Shop based on <b>real</b> reviews
# Future localization note - "Yes, try it" will need to be directly referenced when the button is added to fluent so it maintains parity with this copy
shopping-onboarding-opt-in-privacy-policy-and-terms-of-use = By selecting “Yes, try it” you agree to the <a data-l10n-name="privacy_policy">privacy policy</a> and <a data-l10n-name="terms_of_use">terms of use</a> of { -fakespot-brand-name }, a { -vendor-short-name } company.
