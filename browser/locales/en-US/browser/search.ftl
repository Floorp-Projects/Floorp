# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

## These strings are used for errors when installing OpenSearch engines, e.g.
## via "Add Search Engine" on the address bar or search bar.
## Variables
## $location-url (String) - the URL of the OpenSearch engine that was attempted to be installed.

opensearch-error-duplicate-title = Install Error
opensearch-error-duplicate-desc = { -brand-short-name } could not install the search plugin from “{ $location-url }” because an engine with the same name already exists.

opensearch-error-format-title = Invalid Format
opensearch-error-format-desc = { -brand-short-name } could not install the search engine from: { $location-url }

opensearch-error-download-title = Download Error
opensearch-error-download-desc =
    { -brand-short-name } could not download the search plugin from: { $location-url }

##

searchbar-submit =
    .tooltiptext = Submit search

# This string is displayed in the search box when the input field is empty
searchbar-input =
    .placeholder = Search

searchbar-icon =
    .tooltiptext = Search

## Infobar shown when search engine is removed and replaced.
## Variables
## $oldEngine (String) - the search engine to be removed.
## $newEngine (String) - the search engine to replace the removed search engine.

remove-search-engine-message = <strong>Your default search engine has been changed.</strong> { -brand-short-name } no longer supports { $oldEngine }. { $newEngine } is now your default search engine. To change to another default search engine, go to settings. <label data-l10n-name="remove-search-engine-article">Learn more</label>
remove-search-engine-button = OK
