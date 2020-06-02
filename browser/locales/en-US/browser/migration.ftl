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

import-source =
    .label = Import Settings and Data
import-items-title =
    .label = Items to Import

import-items-description = Select which items to import:

import-migrating-title =
    .label = Importing…

import-migrating-description = The following items are currently being imported…

import-select-profile-title =
    .label = Select Profile

import-select-profile-description = The following profiles are available to import from:

import-done-title =
    .label = Import Complete

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

# Import Sources
# Note: When adding an import source for profile reset, add the string name to
# resetProfile.js if it should be listed in the reset dialog.

browser-data-ie-1 =
    .label = Internet Options
    .value = Internet Options
browser-data-edge-1 =
    .label = Settings
    .value = Settings
browser-data-safari-1 =
    .label = Preferences
    .value = Preferences
browser-data-chrome-1 =
    .label = Preferences
    .value = Preferences
browser-data-canary-1 =
    .label = Preferences
    .value = Preferences
browser-data-360se-1 =
    .label = Preferences
    .value = Preferences

browser-data-ie-2 =
    .label = Cookies
    .value = Cookies
browser-data-edge-2 =
    .label = Cookies
    .value = Cookies
browser-data-safari-2 =
    .label = Cookies
    .value = Cookies
browser-data-chrome-2 =
    .label = Cookies
    .value = Cookies
browser-data-canary-2 =
    .label = Cookies
    .value = Cookies
browser-data-firefox-2 =
    .label = Cookies
    .value = Cookies
browser-data-360se-2 =
    .label = Cookies
    .value = Cookies

browser-data-ie-4 =
    .label = Browsing History
    .value = Browsing History
browser-data-edge-4 =
    .label = Browsing History
    .value = Browsing History
browser-data-safari-4 =
    .label = Browsing History
    .value = Browsing History
browser-data-chrome-4 =
    .label = Browsing History
    .value = Browsing History
browser-data-canary-4 =
    .label = Browsing History
    .value = Browsing History
browser-data-firefox-history-and-bookmarks-4 =
    .label = Browsing History and Bookmarks
    .value = Browsing History and Bookmarks
browser-data-360se-4 =
    .label = Browsing History
    .value = Browsing History

browser-data-ie-8 =
    .label = Saved Form History
    .value = Saved Form History
browser-data-edge-8 =
    .label = Saved Form History
    .value = Saved Form History
browser-data-safari-8 =
    .label = Saved Form History
    .value = Saved Form History
browser-data-chrome-8 =
    .label = Saved Form History
    .value = Saved Form History
browser-data-canary-8 =
    .label = Saved Form History
    .value = Saved Form History
browser-data-firefox-8 =
    .label = Saved Form History
    .value = Saved Form History
browser-data-360se-8 =
    .label = Saved Form History
    .value = Saved Form History

browser-data-ie-16 =
    .label = Saved Passwords
    .value = Saved Passwords
browser-data-edge-16 =
    .label = Saved Passwords
    .value = Saved Passwords
browser-data-safari-16 =
    .label = Saved Passwords
    .value = Saved Passwords
browser-data-chrome-16 =
    .label = Saved Passwords
    .value = Saved Passwords
browser-data-canary-16 =
    .label = Saved Passwords
    .value = Saved Passwords
browser-data-firefox-16 =
    .label = Saved Passwords
    .value = Saved Passwords
browser-data-360se-16 =
    .label = Saved Passwords
    .value = Saved Passwords

browser-data-ie-32 =
    .label = Favorites
    .value = Favorites
browser-data-edge-32 =
    .label = Favorites
    .value = Favorites
browser-data-safari-32 =
    .label = Bookmarks
    .value = Bookmarks
browser-data-chrome-32 =
    .label = Bookmarks
    .value = Bookmarks
browser-data-canary-32 =
    .label = Bookmarks
    .value = Bookmarks
browser-data-360se-32 =
    .label = Bookmarks
    .value = Bookmarks

browser-data-ie-64 =
    .label = Other Data
    .value = Other Data
browser-data-edge-64 =
    .label = Other Data
    .value = Other Data
browser-data-safari-64 =
    .label = Other Data
    .value = Other Data
browser-data-chrome-64 =
    .label = Other Data
    .value = Other Data
browser-data-canary-64 =
    .label = Other Data
    .value = Other Data
browser-data-firefox-other-64 =
    .label = Other Data
    .value = Other Data
browser-data-360se-64 =
    .label = Other Data
    .value = Other Data

browser-data-firefox-128 =
    .label = Windows and Tabs
    .value = Windows and Tabs
