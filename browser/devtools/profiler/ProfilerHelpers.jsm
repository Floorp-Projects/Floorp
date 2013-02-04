/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;
const ProfilerProps = "chrome://browser/locale/devtools/profiler.properties";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

this.EXPORTED_SYMBOLS = ["L10N"];

/**
 * Localization helper methods.
 */
let L10N = {
  /**
   * Returns a simple localized string.
   *
   * @param string name
   * @return string
   */
  getStr: function L10N_getStr(name) {
    return this.stringBundle.GetStringFromName(name);
  },

  /**
   * Returns formatted localized string.
   *
   * @param string name
   * @param array params
   * @return string
   */
  getFormatStr: function L10N_getFormatStr(name, params) {
    return this.stringBundle.formatStringFromName(name, params, params.length);
  }
};

XPCOMUtils.defineLazyGetter(L10N, "stringBundle", function () {
  return Services.strings.createBundle(ProfilerProps);
});