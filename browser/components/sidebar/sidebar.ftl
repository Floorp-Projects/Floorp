# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

sidebar-main-insights =
    .title = Insights

## Variables:
##   $date (string) - Date to be formatted based on locale

sidebar-history-date-today =
  .heading = Today — { DATETIME($date, dateStyle: "full") }
sidebar-history-date-yesterday =
  .heading = Yesterday — { DATETIME($date, dateStyle: "full") }
sidebar-history-date-this-month =
  .heading = { DATETIME($date, dateStyle: "full") }
sidebar-history-date-prev-month =
  .heading = { DATETIME($date, month: "long", year: "numeric") }

##

# "Search" is a noun (as in "Results of the search for")
# Variables:
#   $query (String) - The search query used for searching through browser history.
sidebar-search-results-header =
  .heading = Search results for “{ $query }”

sidebar-menu-customize =
  .title = Customize sidebar
sidebar-customize-header = Customize sidebar
sidebar-customize-firefox-tools = { -brand-product-name } tools
sidebar-customize-history = History
sidebar-customize-synced-tabs = Tabs from other devices

## Labels for sidebar context menu items

sidebar-context-menu-manage-extension =
  .label = Manage extension
sidebar-context-menu-remove-extension =
  .label = Remove extension
sidebar-context-menu-report-extension =
  .label = Report extension
