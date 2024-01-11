/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "gStringBundle", function () {
  return Services.strings.createBundle(
    "chrome://browser/locale/siteData.properties"
  );
});

ChromeUtils.defineLazyGetter(lazy, "gBrandBundle", function () {
  return Services.strings.createBundle(
    "chrome://branding/locale/brand.properties"
  );
});

export var SiteDataManager = {
  // A Map of sites and their disk usage according to Quota Manager.
  // Key is base domain (group sites based on base domain across scheme, port,
  // origin attributes) or host if the entry does not have a base domain.
  // Value is one object holding:
  //   - baseDomainOrHost: Same as key.
  //   - principals: instances of nsIPrincipal (only when the site has
  //     quota storage).
  //   - persisted: the persistent-storage status.
  //   - quotaUsage: the usage of indexedDB and localStorage.
  //   - containersData: a map containing cookiesBlocked,lastAccessed and quotaUsage by userContextID.
  _sites: new Map(),

  _getCacheSizeObserver: null,

  _getCacheSizePromise: null,

  _getQuotaUsagePromise: null,

  _quotaUsageRequest: null,

  /**
   *  Retrieve the latest site data and store it in SiteDataManager.
   *
   *  Updating site data is a *very* expensive operation. This method exists so that
   *  consumers can manually decide when to update, most methods on SiteDataManager
   *  will not trigger updates automatically.
   *
   *  It is *highly discouraged* to await on this function to finish before showing UI.
   *  Either trigger the update some time before the data is needed or use the
   *  entryUpdatedCallback parameter to update the UI async.
   *
   * @param {entryUpdatedCallback} a function to be called whenever a site is added or
   *        updated. This can be used to e.g. fill a UI that lists sites without
   *        blocking on the entire update to finish.
   * @returns a Promise that resolves when updating is done.
   **/
  async updateSites(entryUpdatedCallback) {
    Services.obs.notifyObservers(null, "sitedatamanager:updating-sites");
    // Clear old data and requests first
    this._sites.clear();
    this._getAllCookies(entryUpdatedCallback);
    await this._getQuotaUsage(entryUpdatedCallback);
    Services.obs.notifyObservers(null, "sitedatamanager:sites-updated");
  },

  /**
   * Get the base domain of a host on a best-effort basis.
   * @param {string} host - Host to convert.
   * @returns {string} Computed base domain. If the base domain cannot be
   * determined, because the host is an IP address or does not have enough
   * domain levels we will return the original host. This includes the empty
   * string.
   * @throws {Error} Throws for unexpected conversion errors from eTLD service.
   */
  getBaseDomainFromHost(host) {
    let result = host;
    try {
      result = Services.eTLD.getBaseDomainFromHost(host);
    } catch (e) {
      if (
        e.result == Cr.NS_ERROR_HOST_IS_IP_ADDRESS ||
        e.result == Cr.NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS
      ) {
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

  _getOrInsertSite(baseDomainOrHost) {
    let site = this._sites.get(baseDomainOrHost);
    if (!site) {
      site = {
        baseDomainOrHost,
        cookies: [],
        persisted: false,
        quotaUsage: 0,
        lastAccessed: 0,
        principals: [],
      };
      this._sites.set(baseDomainOrHost, site);
    }
    return site;
  },

  _getOrInsertContainersData(site, userContextId) {
    if (!site.containersData) {
      site.containersData = new Map();
    }

    let containerData = site.containersData.get(userContextId);
    if (!containerData) {
      containerData = {
        cookiesBlocked: 0,
        lastAccessed: new Date(0),
        quotaUsage: 0,
      };
      site.containersData.set(userContextId, containerData);
    }
    return containerData;
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
          "nsICacheStorageConsumptionObserver",
          "nsISupportsWeakReference",
        ]),
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

  _getQuotaUsage(entryUpdatedCallback) {
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
              Services.scriptSecurityManager.createContentPrincipalFromOrigin(
                item.origin
              );
            if (principal.schemeIs("http") || principal.schemeIs("https")) {
              // Group dom storage by first party. If an entry is partitioned
              // the first party site will be in the partitionKey, instead of
              // the principal baseDomain.
              let pkBaseDomain;
              try {
                pkBaseDomain = ChromeUtils.getBaseDomainFromPartitionKey(
                  principal.originAttributes.partitionKey
                );
              } catch (e) {
                console.error(e);
              }
              let site = this._getOrInsertSite(
                pkBaseDomain || principal.baseDomain
              );
              // Assume 3 sites:
              //   - Site A (not persisted): https://www.foo.com
              //   - Site B (not persisted): https://www.foo.com^userContextId=2
              //   - Site C (persisted):     https://www.foo.com:1234
              //     Although only C is persisted, grouping by base domain, as a
              //     result, we still mark as persisted here under this base
              //     domain group.
              if (item.persisted) {
                site.persisted = true;
              }
              if (site.lastAccessed < item.lastAccessed) {
                site.lastAccessed = item.lastAccessed;
              }
              if (Number.isInteger(principal.userContextId)) {
                let containerData = this._getOrInsertContainersData(
                  site,
                  principal.userContextId
                );
                containerData.quotaUsage = item.usage;
                let itemTime = item.lastAccessed / 1000;
                if (containerData.lastAccessed.getTime() < itemTime) {
                  containerData.lastAccessed.setTime(itemTime);
                }
              }
              site.principals.push(principal);
              site.quotaUsage += item.usage;
              if (entryUpdatedCallback) {
                entryUpdatedCallback(principal.baseDomain, site);
              }
            }
          }
        }
        resolve();
      };
      // XXX: The work of integrating localStorage into Quota Manager is in progress.
      //      After the bug 742822 and 1286798 landed, localStorage usage will be included.
      //      So currently only get indexedDB usage.
      this._quotaUsageRequest = Services.qms.getUsage(onUsageResult);
    });
    return this._getQuotaUsagePromise;
  },

  _getAllCookies(entryUpdatedCallback) {
    for (let cookie of Services.cookies.cookies) {
      // Group cookies by first party. If a cookie is partitioned the
      // partitionKey will contain the first party site, instead of the host
      // field.
      let pkBaseDomain;
      try {
        pkBaseDomain = ChromeUtils.getBaseDomainFromPartitionKey(
          cookie.originAttributes.partitionKey
        );
      } catch (e) {
        console.error(e);
      }
      let baseDomainOrHost =
        pkBaseDomain || this.getBaseDomainFromHost(cookie.rawHost);
      let site = this._getOrInsertSite(baseDomainOrHost);
      if (entryUpdatedCallback) {
        entryUpdatedCallback(baseDomainOrHost, site);
      }
      site.cookies.push(cookie);
      if (Number.isInteger(cookie.originAttributes.userContextId)) {
        let containerData = this._getOrInsertContainersData(
          site,
          cookie.originAttributes.userContextId
        );
        containerData.cookiesBlocked += 1;
        let cookieTime = cookie.lastAccessed / 1000;
        if (containerData.lastAccessed.getTime() < cookieTime) {
          containerData.lastAccessed.setTime(cookieTime);
        }
      }
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

  /**
   * Checks if the site with the provided ASCII host is using any site data at all.
   * This will check for:
   *   - Cookies (incl. subdomains)
   *   - Quota Usage
   * in that order. This function is meant to be fast, and thus will
   * end searching and return true once the first trace of site data is found.
   *
   * @param {String} the ASCII host to check
   * @returns {Boolean} whether the site has any data associated with it
   */
  async hasSiteData(asciiHost) {
    if (Services.cookies.countCookiesFromHost(asciiHost)) {
      return true;
    }

    let hasQuota = await new Promise(resolve => {
      Services.qms.getUsage(request => {
        if (request.resultCode != Cr.NS_OK) {
          resolve(false);
          return;
        }

        for (let item of request.result) {
          if (!item.persisted && item.usage <= 0) {
            continue;
          }

          let principal =
            Services.scriptSecurityManager.createContentPrincipalFromOrigin(
              item.origin
            );
          if (principal.asciiHost == asciiHost) {
            resolve(true);
            return;
          }
        }

        resolve(false);
      });
    });

    if (hasQuota) {
      return true;
    }

    return false;
  },

  getTotalUsage() {
    return this._getQuotaUsagePromise.then(() => {
      let usage = 0;
      for (let site of this._sites.values()) {
        usage += site.quotaUsage;
      }
      return usage;
    });
  },

  /**
   * Gets all sites that are currently storing site data. Entries are grouped by
   * parent base domain if applicable. For example "foo.example.com",
   * "example.com" and "bar.example.com" will have one entry with the baseDomain
   * "example.com".
   * A base domain entry will represent all data of its storage jar. The storage
   * jar holds all first party data of the domain as well as any third party
   * data partitioned under the domain. Additionally we will add data which
   * belongs to the domain but is part of other domains storage jars . That is
   * data third-party partitioned under other domains.
   * Sites which cannot be associated with a base domain, for example IP hosts,
   * are not grouped.
   *
   * The list is not automatically up-to-date. You need to call
   * {@link updateSites} before you can use this method for the first time (and
   * whenever you want to get an updated set of list.)
   *
   * @returns {Promise} Promise that resolves with the list of all sites.
   */
  async getSites() {
    await this._getQuotaUsagePromise;

    return Array.from(this._sites.values()).map(site => ({
      baseDomain: site.baseDomainOrHost,
      cookies: site.cookies,
      usage: site.quotaUsage,
      containersData: site.containersData,
      persisted: site.persisted,
      lastAccessed: new Date(site.lastAccessed / 1000),
    }));
  },

  /**
   * Get site, which stores data, by base domain or host.
   *
   * The list is not automatically up-to-date. You need to call
   * {@link updateSites} before you can use this method for the first time (and
   * whenever you want to get an updated set of list.)
   *
   * @param {String} baseDomainOrHost - Base domain or host of the site to get.
   *
   * @returns {Promise<Object|null>} Promise that resolves with the site object
   * or null if no site with given base domain or host stores data.
   */
  async getSite(baseDomainOrHost) {
    let baseDomain = this.getBaseDomainFromHost(baseDomainOrHost);

    let site = this._sites.get(baseDomain);
    if (!site) {
      return null;
    }
    return {
      baseDomain: site.baseDomainOrHost,
      cookies: site.cookies,
      usage: site.quotaUsage,
      containersData: site.containersData,
      persisted: site.persisted,
      lastAccessed: new Date(site.lastAccessed / 1000),
    };
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

  _removeCookies(site) {
    for (let cookie of site.cookies) {
      Services.cookies.remove(
        cookie.host,
        cookie.name,
        cookie.path,
        cookie.originAttributes
      );
    }
    site.cookies = [];
  },

  // Returns a list of permissions from the permission manager that
  // we consider part of "site data and cookies".
  _getDeletablePermissions() {
    let perms = [];

    for (let permission of Services.perms.all) {
      if (
        permission.type == "persistent-storage" ||
        permission.type == "storage-access"
      ) {
        perms.push(permission);
      }
    }

    return perms;
  },

  /**
   * Removes all site data for the specified list of domains and hosts.
   * This includes site data of subdomains belonging to the domains or hosts and
   * partitioned storage. Data is cleared per storage jar, which means if we
   * clear "example.com", we will also clear third parties embedded on
   * "example.com". Additionally we will clear all data of "example.com" (as a
   * third party) from other jars.
   *
   * @param {string|string[]} domainsOrHosts - List of domains and hosts or
   * single domain or host to remove.
   * @returns {Promise} Promise that resolves when data is removed and the site
   * data manager has been updated.
   */
  async remove(domainsOrHosts) {
    if (domainsOrHosts == null) {
      throw new Error("domainsOrHosts is required.");
    }
    // Allow the caller to pass a single base domain or host.
    if (!Array.isArray(domainsOrHosts)) {
      domainsOrHosts = [domainsOrHosts];
    }
    let perms = this._getDeletablePermissions();
    let promises = [];
    for (let domainOrHost of domainsOrHosts) {
      const kFlags =
        Ci.nsIClearDataService.CLEAR_COOKIES |
        Ci.nsIClearDataService.CLEAR_DOM_STORAGES |
        Ci.nsIClearDataService.CLEAR_EME |
        Ci.nsIClearDataService.CLEAR_ALL_CACHES |
        Ci.nsIClearDataService.CLEAR_COOKIE_BANNER_EXECUTED_RECORD;
      promises.push(
        new Promise(function (resolve) {
          const { clearData } = Services;
          if (domainOrHost) {
            // First try to clear by base domain for aDomainOrHost. If we can't
            // get a base domain, fall back to clearing by just host.
            try {
              clearData.deleteDataFromBaseDomain(
                domainOrHost,
                true,
                kFlags,
                resolve
              );
            } catch (e) {
              if (
                e.result != Cr.NS_ERROR_HOST_IS_IP_ADDRESS &&
                e.result != Cr.NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS
              ) {
                throw e;
              }
              clearData.deleteDataFromHost(domainOrHost, true, kFlags, resolve);
            }
          } else {
            clearData.deleteDataFromLocalFiles(true, kFlags, resolve);
          }
        })
      );

      for (let perm of perms) {
        // Specialcase local file permissions.
        if (!domainOrHost) {
          if (perm.principal.schemeIs("file")) {
            Services.perms.removePermission(perm);
          }
        } else if (
          Services.eTLD.hasRootDomain(perm.principal.host, domainOrHost)
        ) {
          Services.perms.removePermission(perm);
        }
      }
    }

    await Promise.all(promises);

    return this.updateSites();
  },

  /**
   * In the specified window, shows a prompt for removing all site data or the
   * specified list of base domains or hosts, warning the user that this may log
   * them out of websites.
   *
   * @param {mozIDOMWindowProxy} win - a parent DOM window to host the dialog.
   * @param {string[]} [removals] - an array of base domain or host strings that
   * will be removed.
   * @returns {boolean} whether the user confirmed the prompt.
   */
  promptSiteDataRemoval(win, removals) {
    if (removals) {
      let args = {
        hosts: removals,
        allowed: false,
      };
      let features = "centerscreen,chrome,modal,resizable=no";
      win.browsingContext.topChromeWindow.openDialog(
        "chrome://browser/content/preferences/dialogs/siteDataRemoveSelected.xhtml",
        "",
        features,
        args
      );
      return args.allowed;
    }

    let brandName = lazy.gBrandBundle.GetStringFromName("brandShortName");
    let flags =
      Services.prompt.BUTTON_TITLE_IS_STRING * Services.prompt.BUTTON_POS_0 +
      Services.prompt.BUTTON_TITLE_CANCEL * Services.prompt.BUTTON_POS_1 +
      Services.prompt.BUTTON_POS_0_DEFAULT;
    let title = lazy.gStringBundle.GetStringFromName(
      "clearSiteDataPromptTitle"
    );
    let text = lazy.gStringBundle.formatStringFromName(
      "clearSiteDataPromptText",
      [brandName]
    );
    let btn0Label = lazy.gStringBundle.GetStringFromName("clearSiteDataNow");

    let result = Services.prompt.confirmEx(
      win,
      title,
      text,
      flags,
      btn0Label,
      null,
      null,
      null,
      {}
    );
    return result == 0;
  },

  /**
   * Clears all site data and cache
   *
   * @returns a Promise that resolves when the data is cleared.
   */
  async removeAll() {
    await this.removeCache();
    return this.removeSiteData();
  },

  /**
   * Clears all caches.
   *
   * @returns a Promise that resolves when the data is cleared.
   */
  removeCache() {
    return new Promise(function (resolve) {
      Services.clearData.deleteData(
        Ci.nsIClearDataService.CLEAR_ALL_CACHES,
        resolve
      );
    });
  },

  /**
   * Clears all site data, but not cache, because the UI offers
   * that functionality separately.
   *
   * @returns a Promise that resolves when the data is cleared.
   */
  async removeSiteData() {
    await new Promise(function (resolve) {
      Services.clearData.deleteData(
        Ci.nsIClearDataService.CLEAR_COOKIES |
          Ci.nsIClearDataService.CLEAR_DOM_STORAGES |
          Ci.nsIClearDataService.CLEAR_HSTS |
          Ci.nsIClearDataService.CLEAR_EME,
        resolve
      );
    });

    for (let permission of this._getDeletablePermissions()) {
      Services.perms.removePermission(permission);
    }

    return this.updateSites();
  },
};
