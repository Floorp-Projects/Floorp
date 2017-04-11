"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "OfflineAppCacheHelper",
                                  "resource:///modules/offlineAppCache.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ContextualIdentityService",
                                  "resource://gre/modules/ContextualIdentityService.jsm");

this.EXPORTED_SYMBOLS = [
  "SiteDataManager"
];

this.SiteDataManager = {

  _qms: Services.qms,

  _appCache: Cc["@mozilla.org/network/application-cache-service;1"].getService(Ci.nsIApplicationCacheService),

  // A Map of sites and their disk usage according to Quota Manager and appcache
  // Key is host (group sites based on host across scheme, port, origin atttributes).
  // Value is one object holding:
  //   - principals: instances of nsIPrincipal.
  //   - persisted: the persistent-storage status.
  //   - quotaUsage: the usage of indexedDB and localStorage.
  //   - appCacheList: an array of app cache; instances of nsIApplicationCache
  _sites: new Map(),

  _getQuotaUsagePromise: null,

  _quotaUsageRequest: null,

  updateSites() {
    Services.obs.notifyObservers(null, "sitedatamanager:updating-sites");

    // Clear old data and requests first
    this._sites.clear();
    this._cancelGetQuotaUsage();

    this._getQuotaUsage()
        .then(results => {
          for (let result of results) {
            let principal =
              Services.scriptSecurityManager.createCodebasePrincipalFromOrigin(result.origin);
            let uri = principal.URI;
            if (uri.scheme == "http" || uri.scheme == "https") {
              let site = this._sites.get(uri.host);
              if (!site) {
                site = {
                  persisted: false,
                  quotaUsage: 0,
                  principals: [],
                  appCacheList: [],
                };
              }
              // Assume 3 sites:
              //   - Site A (not persisted): https://www.foo.com
              //   - Site B (not persisted): https://www.foo.com^userContextId=2
              //   - Site C (persisted):     https://www.foo.com:1234
              // Although only C is persisted, grouping by host, as a result,
              // we still mark as persisted here under this host group.
              if (result.persisted) {
                site.persisted = true;
              }
              site.principals.push(principal);
              site.quotaUsage += result.usage;
              this._sites.set(uri.host, site);
            }
          }
          this._updateAppCache();
          Services.obs.notifyObservers(null, "sitedatamanager:sites-updated", null);
        });
  },

  _getQuotaUsage() {
    this._getQuotaUsagePromise = new Promise(resolve => {
      let callback = {
        onUsageResult(request) {
          resolve(request.result);
        }
      };
      // XXX: The work of integrating localStorage into Quota Manager is in progress.
      //      After the bug 742822 and 1286798 landed, localStorage usage will be included.
      //      So currently only get indexedDB usage.
      this._quotaUsageRequest = this._qms.getUsage(callback);
    });
    return this._getQuotaUsagePromise;
  },

  _cancelGetQuotaUsage() {
    if (this._quotaUsageRequest) {
      this._quotaUsageRequest.cancel();
      this._quotaUsageRequest = null;
    }
  },

  _updateAppCache() {
    let groups = this._appCache.getGroups();
    for (let group of groups) {
      let principal = Services.scriptSecurityManager.createCodebasePrincipalFromOrigin(group);
      let uri = principal.URI;
      let site = this._sites.get(uri.host);
      if (!site) {
        site = {
          persisted: false,
          quotaUsage: 0,
          principals: [ principal ],
          appCacheList: [],
        };
        this._sites.set(uri.host, site);
      } else if (!site.principals.some(p => p.origin == principal.origin)) {
        site.principals.push(principal);
      }
      let cache = this._appCache.getActiveCache(group);
      site.appCacheList.push(cache);
    }
  },

  getTotalUsage() {
    return this._getQuotaUsagePromise.then(() => {
      let usage = 0;
      for (let site of this._sites.values()) {
        for (let cache of site.appCacheList) {
          usage += cache.usage;
        }
        usage += site.quotaUsage;
      }
      return usage;
    });
  },

  getSites() {
    return this._getQuotaUsagePromise.then(() => {
      let list = [];
      for (let [host, site] of this._sites) {
        let usage = site.quotaUsage;
        for (let cache of site.appCacheList) {
          usage += cache.usage;
        }
        list.push({
          host,
          usage,
          persisted: site.persisted
        });
      }
      return list;
    });
  },

  _removePermission(site) {
    let removals = new Set();
    for (let principal of site.principals) {
      let { originNoSuffix } = principal;
      if (removals.has(originNoSuffix)) {
        // In case of encountering
        //   - https://www.foo.com
        //   - https://www.foo.com^userContextId=2
        // because setting/removing permission is across OAs already so skip the same origin without suffix
        continue;
      }
      removals.add(originNoSuffix);
      Services.perms.removeFromPrincipal(principal, "persistent-storage");
    }
  },

  _removeQuotaUsage(site) {
    let promises = [];
    let removals = new Set();
    for (let principal of site.principals) {
      let { originNoSuffix } = principal;
      if (removals.has(originNoSuffix)) {
        // In case of encountering
        //   - https://www.foo.com
        //   - https://www.foo.com^userContextId=2
        // below we have already removed across OAs so skip the same origin without suffix
        continue;
      }
      removals.add(originNoSuffix);
      promises.push(new Promise(resolve => {
        // We are clearing *All* across OAs so need to ensure a principal without suffix here,
        // or the call of `clearStoragesForPrincipal` would fail.
        principal = Services.scriptSecurityManager.createCodebasePrincipalFromOrigin(originNoSuffix);
        let request = this._qms.clearStoragesForPrincipal(principal, null, true);
        request.callback = resolve;
      }));
    }
    return Promise.all(promises);
  },

  _removeAppCache(site) {
    for (let cache of site.appCacheList) {
      cache.discard();
    }
  },

  _removeCookie(site) {
    for (let principal of site.principals) {
      // Although `getCookiesFromHost` can get cookies across hosts under the same base domain, OAs matter.
      // We still need OAs here.
      let e = Services.cookies.getCookiesFromHost(principal.URI.host, principal.originAttributes);
      while (e.hasMoreElements()) {
        let cookie = e.getNext();
        if (cookie instanceof Components.interfaces.nsICookie) {
          if (this.isPrivateCookie(cookie)) {
            continue;
          }
          Services.cookies.remove(
            cookie.host, cookie.name, cookie.path, false, cookie.originAttributes);
        }
      }
    }
  },

  remove(hosts) {
    let promises = [];
    let unknownHost = "";
    for (let host of hosts) {
      let site = this._sites.get(host);
      if (site) {
        this._removePermission(site);
        this._removeAppCache(site);
        this._removeCookie(site);
        promises.push(this._removeQuotaUsage(site));
      } else {
        unknownHost = host;
        break;
      }
    }
    if (promises.length > 0) {
      Promise.all(promises).then(() => this.updateSites());
    }
    if (unknownHost) {
      throw `SiteDataManager: removing unknown site of ${unknownHost}`;
    }
  },

  removeAll() {
    let promises = [];
    for (let site of this._sites.values()) {
      this._removePermission(site);
      promises.push(this._removeQuotaUsage(site));
    }
    Services.cache2.clear();
    Services.cookies.removeAll();
    OfflineAppCacheHelper.clear();
    Promise.all(promises).then(() => this.updateSites());
  },

  isPrivateCookie(cookie) {
    let { userContextId } = cookie.originAttributes;
    // A private cookie is when its userContextId points to a private identity.
    return userContextId && !ContextualIdentityService.getPublicIdentityFromId(userContextId);
  }
};
