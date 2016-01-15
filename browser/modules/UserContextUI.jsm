/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["UserContextUI"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm")

XPCOMUtils.defineLazyGetter(this, "gBrowserBundle", function() {
  return Services.strings.createBundle("chrome://browser/locale/browser.properties");
});

this.UserContextUI = {
  getUserContextLabel(userContextId) {
    switch (userContextId) {
      // No UserContext:
      case 0: return "";

      case 1: return gBrowserBundle.GetStringFromName("usercontext.personal.label");
      case 2: return gBrowserBundle.GetStringFromName("usercontext.work.label");
      case 3: return gBrowserBundle.GetStringFromName("usercontext.shopping.label");
      case 4: return gBrowserBundle.GetStringFromName("usercontext.banking.label");

      // Display the context IDs for values outside the pre-defined range.
      // Used for debugging, no localization necessary.
      default: return "Context " + userContextId;
    }
  }
}
