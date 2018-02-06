"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(this, "OfflineAppCacheHelper",
                               "resource:///modules/offlineAppCache.jsm");
ChromeUtils.defineModuleGetter(this, "ContextualIdentityService",
                               "resource://gre/modules/ContextualIdentityService.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "serviceWorkerManager",
                                   "@mozilla.org/serviceworkers/manager;1",
                                   "nsIServiceWorkerManager");

this.EXPORTED_SYMBOLS = [
  "SiteDataManager"
];

XPCOMUtils.defineLazyGetter(this, "gStringBundle", function() {
  return Services.strings.createBundle("chrome://browser/locale/siteData.properties");
});

XPCOMUtils.defineLazyGetter(this, "gBrandBundle", function() {
  return Services.strings.createBundle("chrome://branding/locale/brand.properties");
});

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

  _getCacheSizeObserver: null,

  _getCacheSizePromise: null,

  _getQuotaUsagePromise: null,

  _quotaUsageRequest: null,

  async updateSites() {
    Services.obs.notifyObservers(null, "sitedatamanager:updating-sites");
    await this._getQuotaUsage();
    this._updateAppCache();
    Services.obs.notifyObservers(null, "sitedatamanager:sites-updated");
  },

  /**
   * Retrieves the amount of space currently used by disk cache.
   *
   * You can use DownloadUtils.convertByteUnits to convert this to
   * a user-understandable size/unit combination.
   *
   * @returns a Promise that resolves with the cache size on disk in bytes.
   */
  getCacheSize() {
    if (this._getCacheSizePromise) {
      return this._getCacheSizePromise;
    }

    this._getCacheSizePromise = new Promise((resolve, reject) => {
      // Needs to root the observer since cache service keeps only a weak reference.
      this._getCacheSizeObserver = {
        onNetworkCacheDiskConsumption: consumption => {
          resolve(consumption);
          this._getCacheSizePromise = null;
          this._getCacheSizeObserver = null;
        },

        QueryInterface: XPCOMUtils.generateQI([
          Components.interfaces.nsICacheStorageConsumptionObserver,
          Components.interfaces.nsISupportsWeakReference
        ])
      };

      try {
        Services.cache2.asyncGetDiskConsumption(this._getCacheSizeObserver);
      } catch (e) {
        reject(e);
        this._getCacheSizePromise = null;
        this._getCacheSizeObserver = null;
      }
    });

    return this._getCacheSizePromise;
  },

  _getQuotaUsage() {
    // Clear old data and requests first
    this._sites.clear();
    this._cancelGetQuotaUsage();
    this._getQuotaUsagePromise = new Promise(resolve => {
      let onUsageResult = request => {
        if (request.resultCode == Cr.NS_OK) {
          let items = request.result;
          for (let item of items) {
            if (!item.persisted && item.usage <= 0) {
              // An non-persistent-storage site with 0 byte quota usage is redundant for us so skip it.
              continue;
            }
            let principal =
              Services.scriptSecurityManager.createCodebasePrincipalFromOrigin(item.origin);
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
              if (item.persisted) {
                site.persisted = true;
              }
              site.principals.push(principal);
              site.quotaUsage += item.usage;
              this._sites.set(uri.host, site);
            }
          }
        }
        resolve();
      };
      // XXX: The work of integrating localStorage into Quota Manager is in progress.
      //      After the bug 742822 and 1286798 landed, localStorage usage will be included.
      //      So currently only get indexedDB usage.
      this._quotaUsageRequest = this._qms.getUsage(onUsageResult);
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
      let cache = this._appCache.getActiveCache(group);
      if (cache.usage <= 0) {
        // A site with 0 byte appcache usage is redundant for us so skip it.
        continue;
      }
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

      Services.obs.notifyObservers(null, "browser:purge-domain-data", principal.URI.host);
    }
  },

  _unregisterServiceWorker(serviceWorker) {
    return new Promise(resolve => {
      let unregisterCallback = {
        unregisterSucceeded: resolve,
        unregisterFailed: resolve, // We don't care about failures.
        QueryInterface: XPCOMUtils.generateQI([Ci.nsIServiceWorkerUnregisterCallback])
      };
      serviceWorkerManager.propagateUnregister(serviceWorker.principal, unregisterCallback, serviceWorker.scope);
    });
  },

  _removeServiceWorkersForSites(sites) {
    let promises = [];
    let targetHosts = sites.map(s => s.principals[0].URI.host);
    let serviceWorkers = serviceWorkerManager.getAllRegistrations();
    for (let i = 0; i < serviceWorkers.length; i++) {
      let sw = serviceWorkers.queryElementAt(i, Ci.nsIServiceWorkerRegistrationInfo);
      // Sites are grouped and removed by host so we unregister service workers by the same host as well
      if (targetHosts.includes(sw.principal.URI.host)) {
        promises.push(this._unregisterServiceWorker(sw));
      }
    }
    return Promise.all(promises);
  },

  remove(hosts) {
    let unknownHost = "";
    let targetSites = [];
    for (let host of hosts) {
      let site = this._sites.get(host);
      if (site) {
        this._removePermission(site);
        this._removeAppCache(site);
        this._removeCookie(site);
        targetSites.push(site);
      } else {
        unknownHost = host;
        break;
      }
    }

    if (targetSites.length > 0) {
      this._removeServiceWorkersForSites(targetSites)
          .then(() => {
            let promises = targetSites.map(s => this._removeQuotaUsage(s));
            return Promise.all(promises);
          })
          .then(() => this.updateSites());
    }
    if (unknownHost) {
      throw `SiteDataManager: removing unknown site of ${unknownHost}`;
    }
  },

  /**
   * In the specified window, shows a prompt for removing
   * all site data, warning the user that this may log them
   * out of websites.
   *
   * @param {mozIDOMWindowProxy} a parent DOM window to host the dialog.
   * @returns a boolean whether the user confirmed the prompt.
   */
  promptSiteDataRemoval(win) {
    let brandName = gBrandBundle.GetStringFromName("brandShortName");
    let flags =
      Services.prompt.BUTTON_TITLE_IS_STRING * Services.prompt.BUTTON_POS_0 +
      Services.prompt.BUTTON_TITLE_CANCEL * Services.prompt.BUTTON_POS_1 +
      Services.prompt.BUTTON_POS_0_DEFAULT;
    let title = gStringBundle.GetStringFromName("clearSiteDataPromptTitle");
    let text = gStringBundle.formatStringFromName("clearSiteDataPromptText", [brandName], 1);
    let btn0Label = gStringBundle.GetStringFromName("clearSiteDataNow");

    let result = Services.prompt.confirmEx(
      win, title, text, flags, btn0Label, null, null, null, {});
    return result == 0;
  },

  /**
   * Clears all site data and cache
   *
   * @returns a Promise that resolves when the data is cleared.
   */
  async removeAll() {
    this.removeCache();
    return this.removeSiteData();
  },

  /**
   * Clears the entire network cache.
   */
  removeCache() {
    Services.cache2.clear();
  },

  /**
   * Clears all site data, which currently means
   *   - Cookies
   *   - AppCache
   *   - ServiceWorkers
   *   - Quota Managed Storage
   *   - persistent-storage permissions
   *
   * @returns a Promise that resolves with the cache size on disk in bytes
   */
  async removeSiteData() {
    Services.cookies.removeAll();
    OfflineAppCacheHelper.clear();

    // Iterate through the service workers and remove them.
    let promises = [];
    let serviceWorkers = serviceWorkerManager.getAllRegistrations();
    for (let i = 0; i < serviceWorkers.length; i++) {
      let sw = serviceWorkers.queryElementAt(i, Ci.nsIServiceWorkerRegistrationInfo);
      promises.push(this._unregisterServiceWorker(sw));
    }
    await Promise.all(promises);

    // Refresh sites using quota usage again.
    // This is for the case:
    //   1. User goes to the about:preferences Site Data section.
    //   2. With the about:preferences opened, user visits another website.
    //   3. The website saves to quota usage, like indexedDB.
    //   4. User goes back to the Site Data section and commands to clear all site data.
    // For this case, we should refresh the site list so not to miss the website in the step 3.
    // We don't do "Clear All" on the quota manager like the cookie, appcache, http cache above
    // because that would clear browser data as well too,
    // see https://bugzilla.mozilla.org/show_bug.cgi?id=1312361#c9
    await this._getQuotaUsage();
    promises = [];
    for (let site of this._sites.values()) {
      this._removePermission(site);
      promises.push(this._removeQuotaUsage(site));
    }
    return Promise.all(promises).then(() => this.updateSites());
  },

  isPrivateCookie(cookie) {
    let { userContextId } = cookie.originAttributes;
    // A private cookie is when its userContextId points to a private identity.
    return userContextId && !ContextualIdentityService.getPublicIdentityFromId(userContextId);
  }
};
