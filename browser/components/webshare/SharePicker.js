/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

class SharePicker {
  constructor() {}

  get classDescription() {
    return "Web Share Picker";
  }

  get classID() {
    return Components.ID("{1201d357-8417-4926-a694-e6408fbedcf8}");
  }

  get contractID() {
    return "@mozilla.org/sharepicker;1";
  }

  get QueryInterface() {
    return ChromeUtils.generateQI([Ci.nsISharePicker]);
  }

  /**
   * The data being shared by the Document.
   *
   * @param {String?} title - title of the share
   * @param {String?} text - text shared
   * @param {nsIURI?} url - a URI shared
   */
  async share(title, text, url) {
    // If anything goes wrong, always throw a real DOMException.
    // e.g., throw new DOMException(someL10nMsg, "AbortError");
    //
    // The possible conditions are:
    //   - User cancels or timeout: "AbortError"
    //   - Data error:  "DataError"
    //   - Anything else, please file a bug on the spec:
    //     https://github.com/w3c/web-share/issues/
    //
    // Returning without throwing is success.
    //
    // This mock implementation just rejects - it's just here
    // as a guide to do actual platform integration.
    throw new DOMException("Not supported.", "AbortError");
  }

  __init() {}
}

const NSGetFactory = XPCOMUtils.generateNSGetFactory([SharePicker]);
