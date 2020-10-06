# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

migration-wizard =
    .title = Import Wizard

import-from =
    { PLATFORM() ->
        [windows] Import Options, Bookmarks, History, Passwords and other data from:
       *[other] Import Preferences, Bookmarks, History, Passwords and other data from:
    }

import-from-bookmarks = Import Bookmarks from:
import-from-ie =
    .label = Microsoft Internet Explorer
    .accesskey = M
import-from-edge =
    .label = Microsoft Edge
    .accesskey = E
import-from-edge-legacy =
    .label = Microsoft Edge Legacy
    .accesskey = L
import-from-edge-beta =
    .label = Microsoft Edge Beta
    .accesskey = d
import-from-nothing =
    .label = Don’t import anything
    .accesskey = D
import-from-safari =
    .label = Safari
    .accesskey = S
import-from-canary =
    .label = Chrome Canary
    .accesskey = n
import-from-chrome =
    .label = Chrome
    .accesskey = C
import-from-chrome-beta =
    .label = Chrome Beta
    .accesskey = B
import-from-chrome-dev =
    .label = Chrome Dev
    .accesskey = D
import-from-chromium =
    .label = Chromium
    .accesskey = u
import-from-firefox =
    .label = Firefox
    .accesskey = x
import-from-360se =
    .label = 360 Secure Browser
    .accesskey = 3

no-migration-sources = No programs that contain bookmarks, history or password data could be found.

import-source-page-title = Import Settings and Data
import-items-page-title = Items to Import

import-items-description = Select which items to import:

import-permissions-page-title = Please give { -brand-short-name } permissions

# Do not translate "Bookmarks.plist"; the file name is the same everywhere.
import-permissions-description = macOS requires you to explicitly allow { -brand-short-name } to access Safari’s bookmarks. Click “Continue” and select the “Bookmarks.plist” file in the File Open panel that appears.

import-migrating-page-title = Importing…

import-migrating-description = The following items are currently being imported…

import-select-profile-page-title = Select Profile

import-select-profile-description = The following profiles are available to import from:

import-done-page-title = Import Complete

import-done-description = The following items were successfully imported:

import-close-source-browser = Please ensure the selected browser is closed before continuing.

# Displays which browser the bookmarks are being imported from
#
# Variables:
#   $source (String): The browser the user has chosen to import bookmarks from.
imported-bookmarks-source = From { $source }

source-name-ie = Internet Explorer
source-name-edge = Microsoft Edge
source-name-edge-beta = Microsoft Edge Beta
source-name-safari = Safari
source-name-canary = Google Chrome Canary
source-name-chrome = Google Chrome
source-name-chrome-beta = Google Chrome Beta
source-name-chrome-dev = Google Chrome Dev
source-name-chromium = Chromium
source-name-firefox = Mozilla Firefox
source-name-360se = 360 Secure Browser

imported-safari-reading-list = Reading List (From Safari)
imported-edge-reading-list = Reading List (From Edge)

## Browser data types
## All of these strings get a $browser variable passed in.
## You can use the browser variable to differentiate the name of items,
## which may have different labels in different browsers.
## The supported values for the $browser variable are:
## 360se
## chrome
## edge
## firefox
## ie
## safari
## The various beta and development versions of edge and chrome all get
## normalized to just "edge" and "chrome" for these strings.

browser-data-cookies-checkbox =
  .label = Cookies
browser-data-cookies-label =
  .value = Cookies

browser-data-history-checkbox =
  .label = { $browser ->
     [firefox] Browsing History and Bookmarks
    *[other] Browsing History
  }
browser-data-history-label =
  .value = { $browser ->
     [firefox] Browsing History and Bookmarks
    *[other] Browsing History
  }

browser-data-formdata-checkbox =
  .label = Saved Form History
browser-data-formdata-label =
  .value = Saved Form History

# This string should use the same phrase for "logins and passwords" as the
# label in the main hamburger menu that opens about:logins.
browser-data-passwords-checkbox =
  .label = Saved Logins and Passwords
# This string should use the same phrase for "logins and passwords" as the
# label in the main hamburger menu that opens about:logins.
browser-data-passwords-label =
  .value = Saved Logins and Passwords

browser-data-bookmarks-checkbox =
  .label = { $browser ->
     [ie] Favorites
     [edge] Favorites
    *[other] Bookmarks
  }
browser-data-bookmarks-label =
  .value = { $browser ->
     [ie] Favorites
     [edge] Favorites
    *[other] Bookmarks
  }

browser-data-otherdata-checkbox =
  .label = Other Data
browser-data-otherdata-label =
  .label = Other Data

browser-data-session-checkbox =
  .label = Windows and Tabs
browser-data-session-label =
  .value = Windows and Tabs
