# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

### These strings are related to the Firefox Suggest feature. Firefox Suggest
### shows recommended and sponsored third-party results in the address bar
### panel. It also shows headings/labels above different groups of results. For
### example, a "Firefox Suggest" label is shown above bookmarks and history
### results, and an "{ $engine } Suggestions" label may be shown above search
### suggestion results.

## These strings are used in the urlbar panel.

# Tooltip text for the help button shown in Firefox Suggest urlbar results.
firefox-suggest-urlbar-learn-more =
  .title = Learn more about { -firefox-suggest-brand-name }

## These strings are used in the preferences UI (about:preferences). Their names
## follow the naming conventions of other strings used in the preferences UI.

# When the user is enrolled in a Firefox Suggest rollout, this text replaces
# the usual addressbar-header string and becomes the text of the address bar
# section in the preferences UI.
addressbar-header-firefox-suggest = Address Bar — { -firefox-suggest-brand-name }

# When the user is enrolled in a Firefox Suggest rollout, this text replaces
# the usual addressbar-suggest string and becomes the text of the description of
# the address bar section in the preferences UI.
addressbar-suggest-firefox-suggest = Choose the type of suggestions that appear in the address bar:

# First Firefox Suggest toggle button main label and description. This toggle
# controls non-sponsored suggestions related to the user's search string.
addressbar-firefox-suggest-nonsponsored = Suggestions from the web
addressbar-firefox-suggest-nonsponsored-description = Get suggestions from { -brand-product-name } related to your search.

# Second Firefox Suggest toggle button main label and description. This toggle
# controls sponsored suggestions related to the user's search string.
addressbar-firefox-suggest-sponsored = Suggestions from sponsors
addressbar-firefox-suggest-sponsored-description = Support the development of { -brand-short-name } with occasional sponsored suggestions.

# Third Firefox Suggest toggle button main label and description. This toggle
# controls data collection related to the user's search string.
addressbar-firefox-suggest-data-collection = Improve the { -firefox-suggest-brand-name } experience
addressbar-firefox-suggest-data-collection-description = Help create a richer search experience by allowing { -vendor-short-name } to process your search queries.

# The "Learn more" link shown in the Firefox Suggest preferences UI.
addressbar-locbar-firefox-suggest-learn-more = Learn more

## The following addressbar-firefox-suggest-info strings are shown in the
## Firefox Suggest preferences UI in the info box underneath the toggle buttons.
## Each string is shown when a particular toggle combination is active.

# Non-sponsored suggestions: on
# Sponsored suggestions: on
# Data collection: on
addressbar-firefox-suggest-info-all = Based on your selection, you’ll receive suggestions from the web, including sponsored sites. We will process your search query data to develop the { -firefox-suggest-brand-name } feature.

# Non-sponsored suggestions: on
# Sponsored suggestions: on
# Data collection: off
addressbar-firefox-suggest-info-nonsponsored-sponsored = Based on your selection, you’ll receive suggestions from the web, including sponsored sites. We won’t process your search query data.

# Non-sponsored suggestions: on
# Sponsored suggestions: off
# Data collection: on
addressbar-firefox-suggest-info-nonsponsored-data = Based on your selection, you’ll receive suggestions from the web, but no sponsored sites. We will process your search query data to develop the { -firefox-suggest-brand-name } feature.

# Non-sponsored suggestions: on
# Sponsored suggestions: off
# Data collection: off
addressbar-firefox-suggest-info-nonsponsored = Based on your selection, you’ll receive suggestions from the web, but no sponsored sites. We won’t process your search query data.

# Non-sponsored suggestions: off
# Sponsored suggestions: on
# Data collection: on
addressbar-firefox-suggest-info-sponsored-data = Based on your selection, you’ll receive sponsored suggestions. We will process your search query data to develop the { -firefox-suggest-brand-name } feature.

# Non-sponsored suggestions: off
# Sponsored suggestions: on
# Data collection: off
addressbar-firefox-suggest-info-sponsored = Based on your selection, you’ll receive sponsored suggestions. We won’t process your search query data.

# Non-sponsored suggestions: off
# Sponsored suggestions: off
# Data collection: on
addressbar-firefox-suggest-info-data = Based on your selection, you won’t receive suggestions from the web or sponsored sites. We will process your search query data to develop the { -firefox-suggest-brand-name } feature.

## Used as title on the introduction pane. The text can be formatted to span
## multiple lines as needed (line breaks are significant).

firefox-suggest-onboarding-introduction-title-1 =
  Make sure you’ve got our latest
  search experience
firefox-suggest-onboarding-introduction-title-2 =
  We’re building a better search experience —
  one you can trust
firefox-suggest-onboarding-introduction-title-3 =
  We’re building a better way to find what
  you’re looking for on the web
firefox-suggest-onboarding-introduction-title-4 =
  A faster search experience is in the works
firefox-suggest-onboarding-introduction-title-5 =
  Together, we can create the kind of search
  experience the Internet deserves
firefox-suggest-onboarding-introduction-title-6 =
  Meet { -firefox-suggest-brand-name }, the next
  evolution in search
firefox-suggest-onboarding-introduction-title-7 =
  Find the best of the web, faster.

##

firefox-suggest-onboarding-introduction-close-button =
  .title = Close

firefox-suggest-onboarding-introduction-next-button-1 = Find out how
firefox-suggest-onboarding-introduction-next-button-2 = Find out more

## Used as title on the main pane. The text can be formatted to span
## multiple lines as needed (line breaks are significant).

firefox-suggest-onboarding-main-title-1 =
  We’re building a richer search experience
firefox-suggest-onboarding-main-title-2 =
  Help us guide the way to the
  best of the Internet
firefox-suggest-onboarding-main-title-3 =
  A richer, smarter search experience
firefox-suggest-onboarding-main-title-4 =
  Finding the best of the web, faster
firefox-suggest-onboarding-main-title-5 =
  We’re building a better search experience —
  you can help
firefox-suggest-onboarding-main-title-6 =
  It’s time to think outside the search engine
firefox-suggest-onboarding-main-title-7 =
  We’re building a smarter search experience —
  one you can trust
firefox-suggest-onboarding-main-title-8 =
  Finding the best of the web should be
  simpler and more secure.

##

firefox-suggest-onboarding-main-description-1 = Allowing { -vendor-short-name } to process your search queries means you’re helping us create smarter, more relevant search suggestions. And, as always, we’ll keep your privacy top of mind.
firefox-suggest-onboarding-main-description-2 = When you allow { -vendor-short-name } to process your search queries, you’re helping build a better { -firefox-suggest-brand-name } for everyone. And, as always, we’ll keep your privacy top of mind.
firefox-suggest-onboarding-main-description-3 = What if your browser helped you zero in on what you’re actually looking for? Allowing { -vendor-short-name } to process your search queries helps us create more relevant search suggestions that still keep your privacy top of mind.
firefox-suggest-onboarding-main-description-4 = You’re trying to get where you’re going on the web and get on with it. When you allow { -vendor-short-name } to process your search queries, we can help you get there faster—while keeping your privacy top of mind.
firefox-suggest-onboarding-main-description-5 = Allowing { -vendor-short-name } to process your search queries will help us create more relevant suggestions for everyone. And, as always, we’ll keep your privacy top of mind.
firefox-suggest-onboarding-main-description-6 = Allowing { -vendor-short-name } to process your search queries will help us create more relevant search suggestions. We’re building { -firefox-suggest-brand-name } to help you get where you’re going on the Internet while keeping your privacy in mind.
firefox-suggest-onboarding-main-description-7 = Allowing { -vendor-short-name } to process your search queries helps us create more relevant search suggestions.
firefox-suggest-onboarding-main-description-8 = Allowing { -vendor-short-name } to process your search queries helps us provide more relevant search suggestions. We don’t use this data to profile you on the web.

firefox-suggest-onboarding-main-privacy-first = No user profiling. Privacy-first, always.

firefox-suggest-onboarding-main-accept-option-label = Allow. <a data-l10n-name="learn-more-link">Learn more</a>

firefox-suggest-onboarding-main-accept-option-description-1 = Help improve the { -firefox-suggest-brand-name } feature with more relevant suggestions. Your search queries will be processed.
firefox-suggest-onboarding-main-accept-option-description-2 = Recommended for people who support improving the { -firefox-suggest-brand-name } feature.  Your search queries will be processed.

firefox-suggest-onboarding-main-reject-option-label = Don’t allow.

firefox-suggest-onboarding-main-reject-option-description-1 = Keep the default { -firefox-suggest-brand-name } experience with the strictest data-sharing controls.
firefox-suggest-onboarding-main-reject-option-description-2 = Recommended for people who prefer the strictest data-sharing controls. Keep the default experience.

firefox-suggest-onboarding-main-submit-button = Save preferences
firefox-suggest-onboarding-main-skip-link = Not now
