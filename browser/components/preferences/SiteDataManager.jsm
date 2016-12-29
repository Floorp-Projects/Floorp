"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "OfflineAppCacheHelper",
                                  "resource:///modules/offlineAppCache.jsm");

this.EXPORTED_SYMBOLS = [
  "SiteDataManager"
];

this.SiteDataManager = {

  _qms: Services.qms,

  _diskCache: Services.cache2.diskCacheStorage(Services.loadContextInfo.default, false),

  _appCache: Cc["@mozilla.org/network/application-cache-service;1"].getService(Ci.nsIApplicationCacheService),

  // A Map of sites using the persistent-storage API (have requested persistent-storage permission)
  // Key is site's origin.
  // Value is one object holding:
  //   - perm: persistent-storage permision; instance of nsIPermission
  //   - status: the permission granted/rejected status
  //   - quotaUsage: the usage of indexedDB and localStorage.
  //   - appCacheList: an array of app cache; instances of nsIApplicationCache
  //   - diskCacheList: an array. Each element is object holding metadata of http cache:
  //       - dataSize: that http cache size
  //       - idEnhance: the id extension of that http cache
  _sites: new Map(),

  _updateQuotaPromise: null,

  _updateDiskCachePromise: null,

  _quotaUsageRequests: null,

  updateSites() {
    // Clear old data and requests first
    this._sites.clear();
    this._cancelQuotaUpdate();

    // Collect sites granted/rejected with the persistent-storage permission
    let perm = null;
    let status = null;
    let e = Services.perms.enumerator;
    while (e.hasMoreElements()) {
      perm = e.getNext();
      status = Services.perms.testExactPermissionFromPrincipal(perm.principal, "persistent-storage");
      if (status === Ci.nsIPermissionManager.ALLOW_ACTION ||
          status === Ci.nsIPermissionManager.DENY_ACTION) {
        this._sites.set(perm.principal.origin, {
          perm,
          status,
          quotaUsage: 0,
          appCacheList: [],
          diskCacheList: []
        });
      }
    }

    this._updateQuota();
    this._updateAppCache();
    this._updateDiskCache();

    Promise.all([this._updateQuotaPromise, this._updateDiskCachePromise])
           .then(() => {
             Services.obs.notifyObservers(null, "sitedatamanager:sites-updated", null);
           });
  },

  _updateQuota() {
    this._quotaUsageRequests = [];
    let promises = [];
    for (let site of this._sites.values()) {
      promises.push(new Promise(resolve => {
        let callback = {
          onUsageResult(request) {
            site.quotaUsage = request.usage;
            resolve();
          }
        };
        // XXX: The work of integrating localStorage into Quota Manager is in progress.
        //      After the bug 742822 and 1286798 landed, localStorage usage will be included.
        //      So currently only get indexedDB usage.
        this._quotaUsageRequests.push(
          this._qms.getUsageForPrincipal(site.perm.principal, callback));
      }));
    }
    this._updateQuotaPromise = Promise.all(promises);
  },

  _cancelQuotaUpdate() {
    if (this._quotaUsageRequests) {
      for (let request of this._quotaUsageRequests) {
        request.cancel();
      }
      this._quotaUsageRequests = null;
    }
  },

  _updateAppCache() {
    let groups = this._appCache.getGroups();
    for (let site of this._sites.values()) {
      for (let group of groups) {
        let uri = Services.io.newURI(group, null, null);
        if (site.perm.matchesURI(uri, true)) {
          let cache = this._appCache.getActiveCache(group);
          site.appCacheList.push(cache);
        }
      }
    }
  },

  _updateDiskCache() {
    this._updateDiskCachePromise = new Promise(resolve => {
      if (this._sites.size) {
        let sites = this._sites;
        let visitor = {
          onCacheEntryInfo(uri, idEnhance, dataSize) {
            for (let site of sites.values()) {
              if (site.perm.matchesURI(uri, true)) {
                site.diskCacheList.push({
                  dataSize,
                  idEnhance
                });
                break;
              }
            }
          },
          onCacheEntryVisitCompleted() {
            resolve();
          }
        };
        this._diskCache.asyncVisitStorage(visitor, true);
      } else {
        resolve();
      }
    });
  },

  getTotalUsage() {
    return Promise.all([this._updateQuotaPromise, this._updateDiskCachePromise])
                  .then(() => {
                    let usage = 0;
                    for (let site of this._sites.values()) {
                      let cache = null;
                      for (cache of site.appCacheList) {
                        usage += cache.usage;
                      }
                      for (cache of site.diskCacheList) {
                        usage += cache.dataSize;
                      }
                      usage += site.quotaUsage;
                    }
                    return usage;
                  });
  },

  _removePermission(site) {
    Services.perms.removePermission(site.perm);
  },

  _removeQuotaUsage(site) {
    this._qms.clearStoragesForPrincipal(site.perm.principal, null, true);
  },

  removeAll() {
    for (let site of this._sites.values()) {
      this._removePermission(site);
      this._removeQuotaUsage(site);
    }
    Services.cache2.clear();
    Services.cookies.removeAll();
    OfflineAppCacheHelper.clear();
    this.updateSites();
  },

  getSites() {
    return Promise.all([this._updateQuotaPromise, this._updateDiskCachePromise])
                  .then(() => {
                    let list = [];
                    for (let [origin, site] of this._sites) {
                      let cache = null;
                      let usage = site.quotaUsage;
                      for (cache of site.appCacheList) {
                        usage += cache.usage;
                      }
                      for (cache of site.diskCacheList) {
                        usage += cache.dataSize;
                      }
                      list.push({
                        usage,
                        status: site.status,
                        uri: NetUtil.newURI(origin)
                      });
                    }
                    return list;
                  });
  }
};
