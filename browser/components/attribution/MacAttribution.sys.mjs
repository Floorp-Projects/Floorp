/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const NUL = 0x0;
const TAB = 0x9;

export var MacAttribution = {
  /**
   * The file path to the `.app` directory.
   */
  get applicationPath() {
    // On macOS, `GreD` is like "App.app/Contents/macOS".  Return "App.app".
    return Services.dirsvc.get("GreD", Ci.nsIFile).parent.parent.path;
  },

  async setAttributionString(aAttrStr, path = this.applicationPath) {
    return IOUtils.setMacXAttr(
      path,
      "com.apple.application-instance",
      new TextEncoder().encode(`__MOZCUSTOM__${aAttrStr}`)
    );
  },

  async getAttributionString(path = this.applicationPath) {
    let promise = IOUtils.getMacXAttr(path, "com.apple.application-instance");
    return promise.then(bytes => {
      // We need to process the extended attribute a little bit to isolate
      // the attribution string:
      // - nul bytes and tabs may be present in raw attribution strings, but are
      //   never part of the attribution data
      // - attribution data is expected to be preceeded by the string `__MOZCUSTOM__`
      let attrStr = new TextDecoder().decode(
        bytes.filter(b => b != NUL && b != TAB)
      );

      if (attrStr.startsWith("__MOZCUSTOM__")) {
        // Return everything after __MOZCUSTOM__
        return attrStr.slice(13);
      }

      throw new Error(`No attribution data found in ${path}`);
    });
  },

  async delAttributionString(path = this.applicationPath) {
    return IOUtils.delMacXAttr(path, "com.apple.application-instance");
  },
};
