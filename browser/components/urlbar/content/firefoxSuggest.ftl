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

# Action text shown in Firefox Suggest urlbar results, appended after the result
# title like "Result Title - Action Text". Used for Firefox Suggest results that
# are non-sponsored.
firefox-suggest-urlbar-nonsponsored-action = { -firefox-suggest-brand-name }

# Tooltip text for the help button shown in Firefox Suggest urlbar results.
firefox-suggest-urlbar-learn-more =
  .title = Learn more about { -firefox-suggest-brand-name }

## These strings are used in the preferences UI (about:preferences).

# Label for the checkbox that controls whether Firefox Suggest results in the
# urlbar are enabled.
firefox-suggest-preferences-enable-urlbar-results =
  .label = Show { -firefox-suggest-brand-name } in the address bar (suggested and sponsored results)

# Learn-more link text for the checkbox.
firefox-suggest-preferences-enable-urlbar-results-learn-more =
  .value = Learn more about { -firefox-suggest-brand-name }
