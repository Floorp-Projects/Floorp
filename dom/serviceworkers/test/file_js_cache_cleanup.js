"use strict";
const { classes: Cc, interfaces: Ci, utils: Cu } = Components;
const { XPCOMUtils } = Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function clearCache() {
  const cacheStorageSrv = Cc["@mozilla.org/netwerk/cache-storage-service;1"].
                          getService(Ci.nsICacheStorageService);
  cacheStorageSrv.clear();
}

addMessageListener("teardown", function() {
  clearCache();
  sendAsyncMessage("teardown-complete");
});
