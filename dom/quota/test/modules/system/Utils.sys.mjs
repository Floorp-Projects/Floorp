/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

import {
  getUsageForOrigin,
  resetStorage,
} from "resource://testing-common/dom/quota/test/modules/StorageUtils.sys.mjs";

export const Utils = {
  async getCachedOriginUsage() {
    const principal = Cc["@mozilla.org/systemprincipal;1"].createInstance(
      Ci.nsIPrincipal
    );
    const result = await getUsageForOrigin(principal, true);
    return result.usage;
  },

  async shrinkStorageSize(size) {
    Services.prefs.setIntPref(
      "dom.quotaManager.temporaryStorage.fixedLimit",
      size
    );

    const result = await resetStorage();
    return result;
  },

  async restoreStorageSize() {
    Services.prefs.clearUserPref(
      "dom.quotaManager.temporaryStorage.fixedLimit"
    );

    const result = await resetStorage();
    return result;
  },
};
