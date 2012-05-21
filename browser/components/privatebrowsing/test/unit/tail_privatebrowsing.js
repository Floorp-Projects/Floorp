/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Be neat: clear the pref that we have set
Cc["@mozilla.org/preferences-service;1"].
  getService(Ci.nsIPrefBranch).
  clearUserPref("browser.privatebrowsing.keep_current_session");
