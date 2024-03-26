# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

sidebar-launcher-insights =
    .title = Insights

## Variables:
##   $date (string) - Date to be formatted based on locale

sidebar-history-date-today =
  .heading = Today - { DATETIME($date, dateStyle: "full") }
sidebar-history-date-yesterday =
  .heading = Yesterday - { DATETIME($date, dateStyle: "full") }
sidebar-history-date-this-month =
  .heading = { DATETIME($date, dateStyle: "full") }
sidebar-history-date-prev-month =
  .heading = { DATETIME($date, month: "long", year: "numeric") }

# "Search" is a noun (as in "Results of the search for")
# Variables:
#   $query (String) - The search query used for searching through browser history.
sidebar-search-results-header =
  .heading = Search results for “{ $query }”
