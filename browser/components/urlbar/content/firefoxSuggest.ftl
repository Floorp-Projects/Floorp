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
addressbar-firefox-suggest-sponsored = Sponsored suggestions
addressbar-firefox-suggest-sponsored-description = Occasionally include sponsored suggestions. These support the continuing development of { -brand-short-name }.

# Third Firefox Suggest toggle button main label and description. This toggle
# controls data collection related to the user's search string.
addressbar-firefox-suggest-data-collection = Make { -firefox-suggest-brand-name } better
addressbar-firefox-suggest-data-collection-description = Allow { -vendor-short-name } to process your search queries to improve the feature, and provide richer suggestions.

# The "Learn more" link shown in the Firefox Suggest info box.
addressbar-locbar-firefox-suggest-learn-more = Learn more

## The following addressbar-firefox-suggest-info strings are shown in the
## Firefox Suggest preferences UI in the info box underneath the toggle buttons.
## Each string is shown when a particular toggle combination is active.

# Non-sponsored suggestions: on
# Sponsored suggestions: on
# Data collection: on
addressbar-firefox-suggest-info-all = Based on your selection, you’ll receive suggestions from the web, including sponsored sites. { -vendor-short-name } will process your search query data to develop the { -firefox-suggest-brand-name } feature.

# Non-sponsored suggestions: on
# Sponsored suggestions: on
# Data collection: off
addressbar-firefox-suggest-info-nonsponsored-sponsored = Based on your selection, you’ll receive suggestions from the web, including sponsored sites. Your search query data won’t be processed.

# Non-sponsored suggestions: on
# Sponsored suggestions: off
# Data collection: on
addressbar-firefox-suggest-info-nonsponsored-data = Based on your selection, you’ll receive suggestions from the web, but no sponsored sites. { -vendor-short-name } will process your search query data to develop the { -firefox-suggest-brand-name } feature.

# Non-sponsored suggestions: on
# Sponsored suggestions: off
# Data collection: off
addressbar-firefox-suggest-info-nonsponsored = Based on your selection, you’ll receive suggestions from the web, but no sponsored sites. Your search query data won’t be processed.

# Non-sponsored suggestions: off
# Sponsored suggestions: on
# Data collection: on
addressbar-firefox-suggest-info-sponsored-data = Based on your selection, you’ll receive sponsored suggestions. { -vendor-short-name } will process your search query data to develop the { -firefox-suggest-brand-name } feature.

# Non-sponsored suggestions: off
# Sponsored suggestions: on
# Data collection: off
addressbar-firefox-suggest-info-sponsored = Based on your selection, you’ll receive sponsored suggestions. { -vendor-short-name } will not process your search query data to develop the { -firefox-suggest-brand-name } feature.

# Non-sponsored suggestions: off
# Sponsored suggestions: off
# Data collection: on
addressbar-firefox-suggest-info-data = Based on your selection, you won’t receive suggestions from the web or sponsored sites. { -vendor-short-name } will process your search query data to develop the { -firefox-suggest-brand-name } feature.
