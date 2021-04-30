/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

var EXPORTED_SYMBOLS = ["SiteDataManager"];

XPCOMUtils.defineLazyGetter(this, "gStringBundle", function() {
  return Services.strings.createBundle(
    "chrome://browser/locale/siteData.properties"
  );
});

XPCOMUtils.defineLazyGetter(this, "gBrandBundle", function() {
  return Services.strings.createBundle(
    "chrome://branding/locale/brand.properties"
  );
});

var SiteDataManager = {
  // A Map of sites and their disk usage according to Quota Manager
  // Key is host (group sites based on host across scheme, port, origin atttributes).
  // Value is one object holding:
  //   - principals: instances of nsIPrincipal (only when the site has
  //     quota storage).
  //   - persisted: the persistent-storage status.
  //   - quotaUsage: the usage of indexedDB and localStorage.
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
            let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
              item.origin
            );
            if (principal.schemeIs("http") || principal.schemeIs("https")) {
              let site = this._getOrInsertSite(principal.host);
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
              if (entryUpdatedCallback) {
                entryUpdatedCallback(principal.host, site);
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
      let site = this._getOrInsertSite(cookie.rawHost);
      if (entryUpdatedCallback) {
        entryUpdatedCallback(cookie.rawHost, site);
      }
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

          let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
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
      promises.push(
        new Promise(resolve => {
          // We are clearing *All* across OAs so need to ensure a principal without suffix here,
          // or the call of `clearStoragesForPrincipal` would fail.
          principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
            originNoSuffix
          );
          let request = this._qms.clearStoragesForPrincipal(
            principal,
            null,
            null,
            true
          );
          request.callback = resolve;
        })
      );
    }
    return Promise.all(promises);
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
   * Removes all site data for the specified list of hosts.
   *
   * @param {Array} a list of hosts to match for removal.
   * @returns a Promise that resolves when data is removed and the site data
   *          manager has been updated.
   */
  async remove(hosts) {
    let perms = this._getDeletablePermissions();
    let promises = [];
    for (let host of hosts) {
      const kFlags =
        Ci.nsIClearDataService.CLEAR_COOKIES |
        Ci.nsIClearDataService.CLEAR_DOM_STORAGES |
        Ci.nsIClearDataService.CLEAR_SECURITY_SETTINGS |
        Ci.nsIClearDataService.CLEAR_EME |
        Ci.nsIClearDataService.CLEAR_ALL_CACHES;
      promises.push(
        new Promise(function(resolve) {
          const { clearData } = Services;
          if (host) {
            clearData.deleteDataFromHost(host, true, kFlags, resolve);
          } else {
            clearData.deleteDataFromLocalFiles(true, kFlags, resolve);
          }
        })
      );

      for (let perm of perms) {
        // Specialcase local file permissions.
        if (!host) {
          if (perm.principal.schemeIs("file")) {
            Services.perms.removePermission(perm);
          }
        } else if (Services.eTLD.hasRootDomain(perm.principal.host, host)) {
          Services.perms.removePermission(perm);
        }
      }
    }

    await Promise.all(promises);

    return this.updateSites();
  },

  /**
   * In the specified window, shows a prompt for removing
   * all site data or the specified list of hosts, warning the
   * user that this may log them out of websites.
   *
   * @param {mozIDOMWindowProxy} a parent DOM window to host the dialog.
   * @param {Array} [optional] an array of host name strings that will be removed.
   * @param {baseDomain} [optional] a baseDomain to use in the dialog when searching
   *        for hosts to be removed. This will trigger a SiteDataManager update.
   * @returns a boolean whether the user confirmed the prompt.
   */
  promptSiteDataRemoval(win, removals, baseDomain) {
    if (baseDomain || removals) {
      let args = {
        baseDomain,
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

    let brandName = gBrandBundle.GetStringFromName("brandShortName");
    let flags =
      Services.prompt.BUTTON_TITLE_IS_STRING * Services.prompt.BUTTON_POS_0 +
      Services.prompt.BUTTON_TITLE_CANCEL * Services.prompt.BUTTON_POS_1 +
      Services.prompt.BUTTON_POS_0_DEFAULT;
    let title = gStringBundle.GetStringFromName("clearSiteDataPromptTitle");
    let text = gStringBundle.formatStringFromName("clearSiteDataPromptText", [
      brandName,
    ]);
    let btn0Label = gStringBundle.GetStringFromName("clearSiteDataNow");

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
    return new Promise(function(resolve) {
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
    await new Promise(function(resolve) {
      Services.clearData.deleteData(
        Ci.nsIClearDataService.CLEAR_COOKIES |
          Ci.nsIClearDataService.CLEAR_DOM_STORAGES |
          Ci.nsIClearDataService.CLEAR_SECURITY_SETTINGS |
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
