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
