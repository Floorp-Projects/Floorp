/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
this.EXPORTED_SYMBOLS = ["OfflineAppCacheHelper"];

Components.utils.import('resource://gre/modules/LoadContextInfo.jsm');

const Cc = Components.classes;
const Ci = Components.interfaces;

this.OfflineAppCacheHelper = {
  clear: function() {
    var cacheService = Cc["@mozilla.org/netwerk/cache-storage-service;1"].getService(Ci.nsICacheStorageService);
    var appCacheStorage = cacheService.appCacheStorage(LoadContextInfo.default, null);
    try {
      appCacheStorage.asyncEvictStorage(null);
    } catch(er) {}
  }
};
