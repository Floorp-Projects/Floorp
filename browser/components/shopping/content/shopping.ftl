# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

### This file is not in a locales directory to prevent it from
### being translated as the feature is still in heavy development
### and strings are likely to change often.

shopping-settings-auto-open-toggle =
  .label = Automatically open Review Checker

# Description text for regions where we support three sites. Sites are limited to Amazon, Walmart and Best Buy.
# Variables:
#   $firstSite (String) - The first shopping page name
#   $secondSite (String) - The second shopping page name
#   $thirdSite (String) - The third shopping page name
shopping-settings-auto-open-description-three-sites = When you view products on { $firstSite }, { $secondSite }, and { $thirdSite }

# Description text for regions where we support only one site (e.g. currently used in FR/DE with Amazon).
# Variables:
#   $currentSite (String) - The current shopping page name
shopping-settings-auto-open-description-single-site = When you view products on { $currentSite }

shopping-settings-sidebar-enabled-state = Review Checker is <strong>On</strong>

shopping-message-bar-keep-closed-header =
  .heading = Keep closed?
  .message = You can update your settings to keep Review Checker closed by default. Right now, it opens automatically.
shopping-message-bar-keep-closed-dismiss-button = No thanks
shopping-message-bar-keep-closed-accept-button = Yes, keep closed

## Shopping Feature Callout strings.
## "price tag" refers to the price tag icon displayed in the address bar to
## access the feature.

shopping-callout-closed-not-opted-in-revised-title = One click to trustworthy reviews
shopping-callout-closed-not-opted-in-revised-subtitle = Just click the price tag icon in the address bar to get back to Review Checker.
shopping-callout-closed-not-opted-in-revised-button = Got it

shopping-callout-not-opted-in-reminder-title = Shop with confidence
shopping-callout-not-opted-in-reminder-subtitle = Not sure if a product’s reviews are real or fake? Review Checker from { -brand-product-name } can help.
shopping-callout-not-opted-in-reminder-open-button = Open Review Checker
shopping-callout-not-opted-in-reminder-close-button = Dismiss
shopping-callout-not-opted-in-reminder-ignore-checkbox = Don’t show again
shopping-callout-not-opted-in-reminder-img-alt =
  .aria-label = Abstract illustration of three product reviews. One has a warning symbol indicating it may not be trustworthy.

shopping-callout-disabled-auto-open-title = Review Checker is now closed by default
shopping-callout-disabled-auto-open-subtitle = Click the price tag icon in the address bar whenever you want to see if you can trust a product’s reviews.
shopping-callout-disabled-auto-open-button = Got it

shopping-callout-opted-out-title = Review Checker is off
shopping-callout-opted-out-subtitle = To turn it back on, click the price tag icon in the address bar and follow the prompts.
shopping-callout-opted-out-button = Got it
