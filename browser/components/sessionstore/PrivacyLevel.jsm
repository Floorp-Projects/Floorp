/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["PrivacyLevel"];

const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

const PREF = "browser.sessionstore.privacy_level";

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
 * The external API as exposed by this module.
 */
var PrivacyLevel = Object.freeze({
  /**
   * Returns whether the current privacy level allows saving data for the given
   * |url|.
   *
   * @param url The URL we want to save data for.
   * @return bool
   */
  check: function (url) {
    return PrivacyLevel.canSave({ isHttps: url.startsWith("https:") });
  },

  /**
   * Checks whether we're allowed to save data for a specific site.
   *
   * @param {isHttps: boolean}
   *        An object that must have one property: 'isHttps'.
   *        'isHttps' tells whether the site us secure communication (HTTPS).
   * @return {bool} Whether we can save data for the specified site.
   */
  canSave: function ({isHttps}) {
    let level = Services.prefs.getIntPref(PREF);

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
