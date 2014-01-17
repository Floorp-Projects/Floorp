/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["PrivacyLevel"];

const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

const PREF_NORMAL = "browser.sessionstore.privacy_level";
const PREF_DEFERRED = "browser.sessionstore.privacy_level_deferred";

// The following constants represent the different possible privacy levels that
// can be set by the user and that we need to consider when collecting text
// data, and cookies.
//
// Collect data from all sites (http and https).
const PRIVACY_NONE = 0;
// Collect data from unencrypted sites (http), only.
const PRIVACY_ENCRYPTED = 1;
// Collect no data.
const PRIVACY_FULL = 2;

/**
 * Returns whether we will resume the session automatically on next startup.
 */
function willResumeAutomatically() {
  return Services.prefs.getIntPref("browser.startup.page") == 3 ||
         Services.prefs.getBoolPref("browser.sessionstore.resume_session_once");
}

/**
 * Determines the current privacy level as set by the user.
 *
 * @param isPinned
 *        Whether to return the privacy level for pinned tabs.
 * @return {int} The privacy level as read from the user's preferences.
 */
function getCurrentLevel(isPinned) {
  let pref = PREF_NORMAL;

  // If we're in the process of quitting and we're not autoresuming the session
  // then we will use the deferred privacy level for non-pinned tabs.
  if (!isPinned && Services.startup.shuttingDown && !willResumeAutomatically()) {
    pref = PREF_DEFERRED;
  }

  return Services.prefs.getIntPref(pref);
}

/**
 * The external API as exposed by this module.
 */
let PrivacyLevel = Object.freeze({
  /**
   * Checks whether we're allowed to save data for a specific site.
   *
   * @param {isHttps: boolean, isPinned: boolean}
   *        An object that must have two properties: 'isHttps' and 'isPinned'.
   *        'isHttps' tells whether the site us secure communication (HTTPS).
   *        'isPinned' tells whether the site is loaded in a pinned tab.
   * @return {bool} Whether we can save data for the specified site.
   */
  canSave: function ({isHttps, isPinned}) {
    let level = getCurrentLevel(isPinned);

    // Never save any data when full privacy is requested.
    if (level == PRIVACY_FULL) {
      return false;
    }

    // Don't save data for encrypted sites when requested.
    if (isHttps && level == PRIVACY_ENCRYPTED) {
      return false;
    }

    return true;
  }
});
