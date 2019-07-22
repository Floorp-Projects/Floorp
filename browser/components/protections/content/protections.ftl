# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

### This file is not in a locales directory to prevent it from
### being translated as the feature is still in heavy development
### and strings are likely to change often.

# Variables:
#   $count (Number) - Number of tracking events blocked.
graph-week-summary =
  { $count ->
     [one] { -brand-short-name } blocked  { $count } tracker over the past week
    *[other] { -brand-short-name } blocked { $count } trackers over the past week
  }

# Variables:
#   $count (Number) - Number of tracking events blocked.
#   $earliestDate (Number) - Unix timestamp in ms, representing a date. The
# earliest date recorded in the database.
graph-total-summary =
  { $count ->
     [one] { $count } tracker blocked since { DATETIME($earliestDate, day: "numeric", month: "long", year: "numeric") }
    *[other] { $count } trackers blocked since { DATETIME($earliestDate, day: "numeric", month: "long", year: "numeric") }
  }

# The terminology used to refer to categories of Content Blocking is also used in chrome/browser/browser.properties and should be translated consistently.
# The category name in the <b> tag will be bold.
# "Standard" in this case is an adjective, meaning "default" or "normal".
protection-header-details-standard = Protection Level is set to <b>Standard</b>
protection-header-details-strict = Protection Level is set to <b>Strict</b>
protection-header-details-custom = Protection Level is set to <b>Custom</b>
