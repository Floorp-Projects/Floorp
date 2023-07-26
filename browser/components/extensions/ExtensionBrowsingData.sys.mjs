/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "makeRange", () => {
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs"
  );
  // Defined in ext-browsingData.js
  return ExtensionParent.apiManager.global.makeRange;
});

ChromeUtils.defineESModuleGetters(lazy, {
  Sanitizer: "resource:///modules/Sanitizer.sys.mjs",
});

export class BrowsingDataDelegate {
  // Unused for now
  constructor(extension) {}

  // This method returns undefined for all data types that are _not_ handled by
  // this delegate.
  handleRemoval(dataType, options) {
    // TODO (Bug 1803799): Use Sanitizer.sanitize() instead of internal cleaners.
    let o = { progress: {} };
    switch (dataType) {
      case "downloads":
        return lazy.Sanitizer.items.downloads.clear(lazy.makeRange(options), o);
      case "formData":
        return lazy.Sanitizer.items.formdata.clear(lazy.makeRange(options), o);
      case "history":
        return lazy.Sanitizer.items.history.clear(lazy.makeRange(options), o);

      default:
        return undefined;
    }
  }

  settings() {
    const PREF_DOMAIN = "privacy.cpd.";
    // The following prefs are the only ones in Firefox that match corresponding
    // values used by Chrome when rerturning settings.
    const PREF_LIST = ["cache", "cookies", "history", "formdata", "downloads"];

    // since will be the start of what is returned by Sanitizer.getClearRange
    // divided by 1000 to convert to ms.
    // If Sanitizer.getClearRange returns undefined that means the range is
    // currently "Everything", so we should set since to 0.
    let clearRange = lazy.Sanitizer.getClearRange();
    let since = clearRange ? clearRange[0] / 1000 : 0;
    let options = { since };

    let dataToRemove = {};
    let dataRemovalPermitted = {};

    for (let item of PREF_LIST) {
      // The property formData needs a different case than the
      // formdata preference.
      const name = item === "formdata" ? "formData" : item;
      // We assume the pref exists, and throw if it doesn't.
      dataToRemove[name] = Services.prefs.getBoolPref(`${PREF_DOMAIN}${item}`);
      // Firefox doesn't have the same concept of dataRemovalPermitted
      // as Chrome, so it will always be true.
      dataRemovalPermitted[name] = true;
    }

    return Promise.resolve({
      options,
      dataToRemove,
      dataRemovalPermitted,
    });
  }
}
