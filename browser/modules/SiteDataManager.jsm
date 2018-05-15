"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(this, "OfflineAppCacheHelper",
                               "resource://gre/modules/offlineAppCache.jsm");
ChromeUtils.defineModuleGetter(this, "ServiceWorkerCleanUp",
                               "resource://gre/modules/ServiceWorkerCleanUp.jsm");

var EXPORTED_SYMBOLS = [
  "SiteDataManager"
];

XPCOMUtils.defineLazyGetter(this, "gStringBundle", function() {
  return Services.strings.createBundle("chrome://browser/locale/siteData.properties");
});

XPCOMUtils.defineLazyGetter(this, "gBrandBundle", function() {
  return Services.strings.createBundle("chrome://branding/locale/brand.properties");
});

var SiteDataManager = {

  _qms: Services.qms,

  _appCache: Cc["@mozilla.org/network/application-cache-service;1"].getService(Ci.nsIApplicationCacheService),

  // A Map of sites and their disk usage according to Quota Manager and appcache
  // Key is host (group sites based on host across scheme, port, origin atttributes).
  // Value is one object holding:
  //   - principals: instances of nsIPrincipal (only when the site has
  //     quota storage or AppCache).
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
    // Clear old data and requests first
    this._sites.clear();
    this._getAllCookies();
    await this._getQuotaUsage();
    this._updateAppCache();
    Services.obs.notifyObservers(null, "sitedatamanager:sites-updated");
  },

  getBaseDomainFromHost(host) {
    let result = host;
    try {
      result = Services.eTLD.getBaseDomainFromHost(host);
    } catch (e) {
      if (e.result == Cr.NS_ERROR_HOST_IS_IP_ADDRESS ||
          e.result == Cr.NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS) {
        // For these 2 expected errors, just take the host as the result.
        // - NS_ERROR_HOST_IS_IP_ADDRESS: the host is in ipv4/ipv6.
        // - NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS: not enough domain parts to extract.
        result = host;
      } else {
        throw e;
      }
    }
    return result;
  },

  _getOrInsertSite(host) {
    let site = this._sites.get(host);
    if (!site) {
      site = {
        baseDomain: this.getBaseDomainFromHost(host),
        cookies: [],
        persisted: false,
        quotaUsage: 0,
        lastAccessed: 0,
        principals: [],
        appCacheList: [],
      };
      this._sites.set(host, site);
    }
    return site;
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

        QueryInterface: ChromeUtils.generateQI([
          Ci.nsICacheStorageConsumptionObserver,
          Ci.nsISupportsWeakReference
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
              let site = this._getOrInsertSite(uri.host);
              // Assume 3 sites:
              //   - Site A (not persisted): https://www.foo.com
              //   - Site B (not persisted): https://www.foo.com^userContextId=2
              //   - Site C (persisted):     https://www.foo.com:1234
              // Although only C is persisted, grouping by host, as a result,
              // we still mark as persisted here under this host group.
              if (item.persisted) {
                site.persisted = true;
              }
              if (site.lastAccessed < item.lastAccessed) {
                site.lastAccessed = item.lastAccessed;
              }
              site.principals.push(principal);
              site.quotaUsage += item.usage;
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

  _getAllCookies() {
    let cookiesEnum = Services.cookies.enumerator;
    while (cookiesEnum.hasMoreElements()) {
      let cookie = cookiesEnum.getNext().QueryInterface(Ci.nsICookie2);
      let site = this._getOrInsertSite(cookie.rawHost);
      site.cookies.push(cookie);
      if (site.lastAccessed < cookie.lastAccessed) {
        site.lastAccessed = cookie.lastAccessed;
      }
    }
  },

  _cancelGetQuotaUsage() {
    if (this._quotaUsageRequest) {
      this._quotaUsageRequest.cancel();
      this._quotaUsageRequest = null;
    }
  },

  _updateAppCache() {
    let groups;
    try {
      groups = this._appCache.getGroups();
    } catch (e) {
      // NS_ERROR_NOT_AVAILABLE means that appCache is not initialized,
      // which probably means the user has disabled it. Otherwise, log an
      // error. Either way, there's nothing we can do here.
      if (e.result != Cr.NS_ERROR_NOT_AVAILABLE) {
        Cu.reportError(e);
      }
      return;
    }

    for (let group of groups) {
      let cache = this._appCache.getActiveCache(group);
      if (cache.usage <= 0) {
        // A site with 0 byte appcache usage is redundant for us so skip it.
        continue;
      }
      let principal = Services.scriptSecurityManager.createCodebasePrincipalFromOrigin(group);
      let uri = principal.URI;
      let site = this._getOrInsertSite(uri.host);
      if (!site.principals.some(p => p.origin == principal.origin)) {
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

  /**
   * Gets all sites that are currently storing site data.
   *
   * The list is not automatically up-to-date.
   * You need to call SiteDataManager.updateSites() before you
   * can use this method for the first time (and whenever you want
   * to get an updated set of list.)
   *
   * @param {String} [optional] baseDomain - if specified, it will
   *                            only return data for sites with
   *                            the specified base domain.
   *
   * @returns a Promise that resolves with the list of all sites.
   */
  getSites(baseDomain) {
    return this._getQuotaUsagePromise.then(() => {
      let list = [];
      for (let [host, site] of this._sites) {
        if (baseDomain && site.baseDomain != baseDomain) {
          continue;
        }

        let usage = site.quotaUsage;
        for (let cache of site.appCacheList) {
          usage += cache.usage;
        }
        list.push({
          baseDomain: site.baseDomain,
          cookies: site.cookies,
          host,
          usage,
          persisted: site.persisted,
          lastAccessed: new Date(site.lastAccessed / 1000),
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

  _removeCookies(site) {
    for (let cookie of site.cookies) {
      Services.cookies.remove(
        cookie.host, cookie.name, cookie.path, false, cookie.originAttributes);
    }
    site.cookies = [];
  },

  /**
   * Removes all site data for the specified list of hosts.
   *
   * @param {Array} a list of hosts to match for removal.
   * @returns a Promise that resolves when data is removed and the site data
   *          manager has been updated.
   */
  async remove(hosts) {
    // Make sure we have up-to-date information.
    await this._getQuotaUsage();
    this._updateAppCache();

    let unknownHost = "";
    let promises = [];
    for (let host of hosts) {
      let site = this._sites.get(host);
      if (site) {
        // Clear localstorage.
        Services.obs.notifyObservers(null, "browser:purge-domain-data", host);
        this._removePermission(site);
        this._removeAppCache(site);
        this._removeCookies(site);
        promises.push(ServiceWorkerCleanUp.removeFromHost(host));
        promises.push(this._removeQuotaUsage(site));
      } else {
        unknownHost = host;
        break;
      }
    }

    await Promise.all(promises);

    if (unknownHost) {
      throw `SiteDataManager: removing unknown site of ${unknownHost}`;
    }

    return this.updateSites();
  },

  /**
   * In the specified window, shows a prompt for removing
   * all site data or the specified list of hosts, warning the
   * user that this may log them out of websites.
   *
   * @param {mozIDOMWindowProxy} a parent DOM window to host the dialog.
   * @param {Array} [optional] an array of host name strings that will be removed.
   * @returns a boolean whether the user confirmed the prompt.
   */
  promptSiteDataRemoval(win, removals) {
    if (removals) {
      let args = {
        hosts: removals,
        allowed: false
      };
      let features = "centerscreen,chrome,modal,resizable=no";
      win.openDialog("chrome://browser/content/preferences/siteDataRemoveSelected.xul", "", features, args);
      return args.allowed;
    }

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
   *   - LocalStorage
   *   - ServiceWorkers
   *   - Quota Managed Storage
   *   - persistent-storage permissions
   *
   * @returns a Promise that resolves with the cache size on disk in bytes
   */
  async removeSiteData() {
    // LocalStorage
    Services.obs.notifyObservers(null, "extension:purge-localStorage");

    Services.cookies.removeAll();
    OfflineAppCacheHelper.clear();

    await ServiceWorkerCleanUp.removeAll();

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
    this._sites.clear();
    await this._getQuotaUsage();
    let promises = [];
    for (let site of this._sites.values()) {
      this._removePermission(site);
      promises.push(this._removeQuotaUsage(site));
    }
    return Promise.all(promises).then(() => this.updateSites());
  },
};
