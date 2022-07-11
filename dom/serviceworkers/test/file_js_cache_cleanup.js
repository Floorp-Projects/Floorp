"use strict";
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

function clearCache() {
  const cacheStorageSrv = Cc[
    "@mozilla.org/netwerk/cache-storage-service;1"
  ].getService(Ci.nsICacheStorageService);
  cacheStorageSrv.clear();
}

addMessageListener("teardown", function() {
  clearCache();
  sendAsyncMessage("teardown-complete");
});
