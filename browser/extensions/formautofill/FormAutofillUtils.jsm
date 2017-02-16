/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["FormAutofillUtils"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

this.FormAutofillUtils = {
  defineLazyLogGetter(scope, logPrefix) {
    XPCOMUtils.defineLazyGetter(scope, "log", () => {
      let ConsoleAPI = Cu.import("resource://gre/modules/Console.jsm", {}).ConsoleAPI;
      return new ConsoleAPI({
        maxLogLevelPref: "browser.formautofill.loglevel",
        prefix: logPrefix,
      });
    });
  },

  generateFullName(firstName, lastName, middleName) {
    // TODO: The implementation should depend on the L10N spec, but a simplified
    // rule is used here.
    let fullName = firstName;
    if (middleName) {
      fullName += " " + middleName;
    }
    if (lastName) {
      fullName += " " + lastName;
    }
    return fullName;
  },
};
