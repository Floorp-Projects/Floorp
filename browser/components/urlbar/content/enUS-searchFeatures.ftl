# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

### These strings are related to the Firefox Suggest feature. Firefox Suggest
### shows recommended and sponsored third-party results in the address bar
### panel. It also shows headings/labels above different groups of results. For
### example, a "Firefox Suggest" label is shown above bookmarks and history
### results, and an "{ $engine } Suggestions" label may be shown above search
### suggestion results.

## These terms are defined in this file because the feature is en-US only.
## They should be moved to toolkit/branding/brandings.ftl if the feature is
## exposed for localization.

-mdn-brand-name = MDN Web Docs
-mdn-brand-short-name = MDN

## These strings are used in the urlbar panel.

# A label shown above the Shortcuts aka Top Sites group in the urlbar results
# if there's another result before that group. This should be consistent with
# addressbar-locbar-shortcuts-option.
urlbar-group-shortcuts =
  .label = Shortcuts

# A label shown above the top pick group in the urlbar results.
urlbar-group-best-match =
  .label = Top pick

# Label shown above an extension suggestion in the urlbar results (an
# alternative phrasing is "Extension for Firefox"). It's singular since only one
# suggested extension is displayed.
urlbar-group-addon =
  .label = { -brand-product-name } extension

# Label shown above a MDN suggestion in the urlbar results.
urlbar-group-mdn =
  .label = Recommended resource

# Label shown above a Pocket suggestion in the urlbar results.
urlbar-group-pocket =
  .label = Recommended reads

# Block menu item shown in the result menu of top pick and quick suggest
# results.
urlbar-result-menu-dismiss-firefox-suggest =
    .label = Dismiss this suggestion
    .accesskey = D

# Learn More menu item shown in the result menu of Firefox Suggest results.
urlbar-result-menu-learn-more-about-firefox-suggest =
    .label = Learn more about { -firefox-suggest-brand-name }
    .accesskey = L

# A message shown in a result when the user gives feedback on it.
firefox-suggest-feedback-acknowledgment = Thanks for your feedback

# A message that replaces a result when the user dismisses a single suggestion.
firefox-suggest-dismissal-acknowledgment-one = Thanks for your feedback. You won’t see this suggestion again.

# A message that replaces a result when the user dismisses all suggestions of a
# particular type.
firefox-suggest-dismissal-acknowledgment-all = Thanks for your feedback. You won’t see these suggestions anymore.

# A message that replaces a result when the user dismisses a single MDN
# suggestion.
firefox-suggest-dismissal-acknowledgment-one-mdn = Thanks for your feedback. You won’t see this { -mdn-brand-short-name } suggestion again.

# A message that replaces a result when the user dismisses all MDN suggestions of
# a particular type.
firefox-suggest-dismissal-acknowledgment-all-mdn = Thanks for your feedback. You won’t see { -mdn-brand-short-name } suggestions anymore.

## These strings are used for weather suggestions in the urlbar.

# This string is displayed above the current temperature
firefox-suggest-weather-currently = Currently

# This string displays the current temperature value and unit
# Variables:
#   $value (number) - The temperature value
#   $unit (String) - The unit for the temperature
firefox-suggest-weather-temperature = { $value }°{ $unit }

# This string is the title of the weather summary
# Variables:
#   $city (String) - The name of the city the weather data is for
firefox-suggest-weather-title = Weather for { $city }

# This string displays the weather summary
# Variables:
#   $currentConditions (String) - The current weather conditions summary
#   $forecast (String) - The forecast weather conditions summary
firefox-suggest-weather-summary-text = { $currentConditions }; { $forecast }

# This string displays the high and low temperatures
# Variables:
#   $high (number) - The number for the high temperature
#   $unit (String) - The unit for the temperature
#   $low (number) - The number for the low temperature
firefox-suggest-weather-high-low = High: { $high }°{ $unit } · Low: { $low }°{ $unit }

# This string displays the name of the weather provider
# Variables:
#   $provider (String) - The name of the weather provider
firefox-suggest-weather-sponsored = { $provider } · Sponsored

## These strings are used as labels of menu items in the result menu.

firefox-suggest-command-show-less-frequently =
  .label = Show less frequently
firefox-suggest-command-dont-show-this =
  .label = Don’t show this
firefox-suggest-command-dont-show-mdn =
  .label = Don’t show { -mdn-brand-short-name } suggestions
firefox-suggest-command-not-relevant =
  .label = Not relevant
firefox-suggest-command-not-interested =
  .label = Not interested
firefox-suggest-weather-command-inaccurate-location =
  .label = Report inaccurate location

## These strings are used for add-on suggestions in the urlbar.

# This string explaining that the add-on suggestion is a recommendation.
firefox-suggest-addons-recommended = Recommended

## These strings are used for MDN suggestions in the urlbar.

# This string is shown in MDN suggestions and indicates the suggestion is from
# MDN.
firefox-suggest-mdn-bottom-text = { -mdn-brand-name }

## These strings are used for Pocket suggestions in the urlbar.

# This string is shown in Pocket suggestions and indicates the suggestion is
# from Pocket and is related to a particular keyword that matches the user's
# search string.
# Variables:
#   $keywordSubstringTyped (string) - The part of the suggestion keyword that the user typed
#   $keywordSubstringNotTyped (string) - The part of the suggestion keyword that the user did not yet type
firefox-suggest-pocket-bottom-text = { -pocket-brand-name } · Related to <strong>{ $keywordSubstringTyped }</strong>{ $keywordSubstringNotTyped }

## These strings are used for Yelp suggestions in the urlbar.

# This string is shown in Yelp suggestions and indicates the suggestion is for
# Yelp.
firefox-suggest-yelp-bottom-text = Yelp

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

# First Firefox Suggest checkbox main label and description. This checkbox
# controls non-sponsored suggestions related to the user's search string.
addressbar-firefox-suggest-nonsponsored =
  .label = Suggestions from { -brand-short-name }
addressbar-firefox-suggest-nonsponsored-desc = Get suggestions from the web related to your search.

# Second Firefox Suggest checkbox main label and description. This checkbox
# controls sponsored suggestions related to the user's search string.
addressbar-firefox-suggest-sponsored =
  .label = Suggestions from sponsors
addressbar-firefox-suggest-sponsored-desc = Support { -brand-short-name } with occasional sponsored suggestions.

# An additional toggle button in the Firefox Suggest settings that controls
# whether userdata-based suggestions like history and bookmarks should be
# shown in private windows
addressbar-firefox-suggest-private =
  .label = Show suggestions in Private Windows

# Third Firefox Suggest toggle button main label and description. This toggle
# controls data collection related to the user's search string.
# .description is transferred into a separate paragraph by the moz-toggle
# custom element code.
addressbar-firefox-suggest-data-collection =
  .label = Improve the { -firefox-suggest-brand-name } experience
  .description = Share search query data with { -vendor-short-name } to create a richer search experience.

# The "Learn more" link shown in the Firefox Suggest preferences UI.
addressbar-locbar-firefox-suggest-learn-more = Learn more

## The following addressbar-firefox-suggest-info strings are shown in the
## Firefox Suggest preferences UI in the info box underneath the toggle.
## Each string is shown when a particular checkbox or toggle combination is active.

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

addressbar-dismissed-suggestions-label = Dismissed suggestions
addressbar-restore-dismissed-suggestions-description = Restore dismissed suggestions from sponsors and { -brand-short-name }.
addressbar-restore-dismissed-suggestions-button =
  .label = Restore
addressbar-restore-dismissed-suggestions-learn-more = Learn more

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
firefox-suggest-onboarding-introduction-next-button-3 = Show me how

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
firefox-suggest-onboarding-main-title-9 =
  Find the best of the web, faster

##

firefox-suggest-onboarding-main-description-1 = Allowing { -vendor-short-name } to process your search queries means you’re helping us create smarter, more relevant search suggestions. And, as always, we’ll keep your privacy top of mind.
firefox-suggest-onboarding-main-description-2 = When you allow { -vendor-short-name } to process your search queries, you’re helping build a better { -firefox-suggest-brand-name } for everyone. And, as always, we’ll keep your privacy top of mind.
firefox-suggest-onboarding-main-description-3 = What if your browser helped you zero in on what you’re actually looking for? Allowing { -vendor-short-name } to process your search queries helps us create more relevant search suggestions that still keep your privacy top of mind.
firefox-suggest-onboarding-main-description-4 = You’re trying to get where you’re going on the web and get on with it. When you allow { -vendor-short-name } to process your search queries, we can help you get there faster—while keeping your privacy top of mind.
firefox-suggest-onboarding-main-description-5 = Allowing { -vendor-short-name } to process your search queries will help us create more relevant suggestions for everyone. And, as always, we’ll keep your privacy top of mind.
firefox-suggest-onboarding-main-description-6 = Allowing { -vendor-short-name } to process your search queries will help us create more relevant search suggestions. We’re building { -firefox-suggest-brand-name } to help you get where you’re going on the Internet while keeping your privacy in mind.
firefox-suggest-onboarding-main-description-7 = Allowing { -vendor-short-name } to process your search queries helps us create more relevant search suggestions.
firefox-suggest-onboarding-main-description-8 = Allowing { -vendor-short-name } to process your search queries helps us provide more relevant search suggestions. We don’t use this data to profile you on the web.
firefox-suggest-onboarding-main-description-9 =
  We’re building a better search experience. When you allow { -vendor-short-name } to process your search queries, we can create more relevant search suggestions for you.
  <a data-l10n-name="learn-more-link">Learn more</a>

firefox-suggest-onboarding-main-privacy-first = No user profiling. Privacy-first, always.

firefox-suggest-onboarding-main-accept-option-label = Allow. <a data-l10n-name="learn-more-link">Learn more</a>
firefox-suggest-onboarding-main-accept-option-label-2 = Enable

firefox-suggest-onboarding-main-accept-option-description-1 = Help improve the { -firefox-suggest-brand-name } feature with more relevant suggestions. Your search queries will be processed.
firefox-suggest-onboarding-main-accept-option-description-2 = Recommended for people who support improving the { -firefox-suggest-brand-name } feature.  Your search queries will be processed.
firefox-suggest-onboarding-main-accept-option-description-3 = Help improve the { -firefox-suggest-brand-name } experience. Your search queries will be processed.

firefox-suggest-onboarding-main-reject-option-label = Don’t allow.
firefox-suggest-onboarding-main-reject-option-label-2 = Keep disabled

firefox-suggest-onboarding-main-reject-option-description-1 = Keep the default { -firefox-suggest-brand-name } experience with the strictest data-sharing controls.
firefox-suggest-onboarding-main-reject-option-description-2 = Recommended for people who prefer the strictest data-sharing controls. Keep the default experience.
firefox-suggest-onboarding-main-reject-option-description-3 = Leave the default { -firefox-suggest-brand-name } experience with the strictest data-sharing controls.

firefox-suggest-onboarding-main-submit-button = Save preferences
firefox-suggest-onboarding-main-skip-link = Not now

## Strings for trending suggestions that are currently only used in
## en-US based experiments.

# Shown in preferences to enabled and disable trending suggestions.
search-show-trending-suggestions =
    .label = Show trending search suggestions
    .accesskey = t

# The header shown above trending results.
# Variables:
#  $engine (String): the name of the search engine providing the trending suggestions
urlbar-group-trending =
  .label = Trending on { $engine }

# The result menu labels shown next to trending results.
urlbar-result-menu-trending-dont-show =
    .label = Don’t show trending searches
    .accesskey = D
urlbar-result-menu-trending-why =
    .label = Why am I seeing this?
    .accesskey = W

# A message that replaces a result when the user dismisses all suggestions of a
# particular type.
urlbar-trending-dismissal-acknowledgment = Thanks for your feedback. You won’t see trending searches anymore.

urlbar-firefox-suggest-contextual-opt-in-title-1 =
  Find the best of the web, faster
urlbar-firefox-suggest-contextual-opt-in-title-2 =
  Say hello to smarter suggestions
urlbar-firefox-suggest-contextual-opt-in-description-1 =
  We’re building a better search experience. When you allow { -vendor-short-name } to process your search queries, we can create more relevant suggestions from { -brand-short-name } and our partners. Privacy-first, always.
  <a data-l10n-name="learn-more-link">Learn more</a>
urlbar-firefox-suggest-contextual-opt-in-description-2 =
  { -firefox-suggest-brand-name } uses your search keywords to make contextual suggestions from { -brand-short-name } and our partners while keeping your privacy in mind.
  <a data-l10n-name="learn-more-link">Learn more</a>
urlbar-firefox-suggest-contextual-opt-in-allow = Allow suggestions
urlbar-firefox-suggest-contextual-opt-in-dismiss = Not now
