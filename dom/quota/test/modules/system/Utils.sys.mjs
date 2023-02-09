/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

import { resetStorage } from "resource://testing-common/dom/quota/test/modules/StorageUtils.sys.mjs";

export const Utils = {
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
