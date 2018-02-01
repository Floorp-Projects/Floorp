/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["OfflineAppCacheHelper"];

ChromeUtils.import("resource://gre/modules/Services.jsm");

this.OfflineAppCacheHelper = {
  clear() {
    var appCacheStorage = Services.cache2.appCacheStorage(Services.loadContextInfo.default, null);
    try {
      appCacheStorage.asyncEvictStorage(null);
    } catch (er) {}
  }
};
