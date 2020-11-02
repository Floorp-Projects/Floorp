/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  LoginHelper: "resource://gre/modules/LoginHelper.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  Preferences: "resource://gre/modules/Preferences.jsm",
  Sanitizer: "resource:///modules/Sanitizer.jsm",
  Services: "resource://gre/modules/Services.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
  ServiceWorkerCleanUp: "resource://gre/modules/ServiceWorkerCleanUp.jsm",
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "quotaManagerService",
  "@mozilla.org/dom/quota-manager-service;1",
  "nsIQuotaManagerService"
);

/**
 * A number of iterations after which to yield time back
 * to the system.
 */
const YIELD_PERIOD = 10;

const makeRange = options => {
  return options.since == null
    ? null
    : [PlacesUtils.toPRTime(options.since), PlacesUtils.toPRTime(Date.now())];
};

// General implementation for clearing data using Services.clearData.
// Currently Sanitizer.items uses this under the hood.
async function clearData(options, flags) {
  if (options.hostnames) {
    await Promise.all(
      options.hostnames.map(
        host =>
          new Promise(resolve => {
            // Set aIsUserRequest to true. This means when the ClearDataService
            // "Cleaner" implementation doesn't support clearing by host
            // it will delete all data instead.
            // This is appropriate for cases like |cache|, which doesn't
            // support clearing by a time range.
            // In future when we use this for other data types, we have to
            // evaluate if that behavior is still acceptable.
            Services.clearData.deleteDataFromHost(host, true, flags, resolve);
          })
      )
    );
    return;
  }

  if (options.since) {
    const range = makeRange(options);
    await new Promise(resolve => {
      Services.clearData.deleteDataInTimeRange(...range, true, flags, resolve);
    });
    return;
  }

  // Don't return the promise here and above to prevent leaking the resolved
  // value.
  await new Promise(resolve => Services.clearData.deleteData(flags, resolve));
}

const clearCache = options => {
  return clearData(options, Ci.nsIClearDataService.CLEAR_ALL_CACHES);
};

const clearCookies = async function(options) {
  let cookieMgr = Services.cookies;
  // This code has been borrowed from Sanitizer.jsm.
  let yieldCounter = 0;

  if (options.since || options.hostnames || options.cookieStoreId) {
    // Iterate through the cookies and delete any created after our cutoff.
    for (const cookie of cookieMgr.cookies) {
      if (
        (!options.since ||
          cookie.creationTime >= PlacesUtils.toPRTime(options.since)) &&
        (!options.hostnames ||
          options.hostnames.includes(cookie.host.replace(/^\./, ""))) &&
        (!options.cookieStoreId ||
          getCookieStoreIdForOriginAttributes(cookie.originAttributes) ===
            options.cookieStoreId)
      ) {
        // This cookie was created after our cutoff, clear it.
        cookieMgr.remove(
          cookie.host,
          cookie.name,
          cookie.path,
          cookie.originAttributes
        );

        if (++yieldCounter % YIELD_PERIOD == 0) {
          await new Promise(resolve => setTimeout(resolve, 0)); // Don't block the main thread too long.
        }
      }
    }
  } else {
    // Remove everything.
    cookieMgr.removeAll();
  }
};

const clearDownloads = options => {
  return Sanitizer.items.downloads.clear(makeRange(options));
};

const clearFormData = options => {
  return Sanitizer.items.formdata.clear(makeRange(options));
};

const clearHistory = options => {
  return Sanitizer.items.history.clear(makeRange(options));
};

// Ideally we could reuse the logic in Sanitizer.jsm or nsIClearDataService,
// but this API exposes an ability to wipe data at a much finger granularity
// than those APIs. (See also Bug 1531276)
async function clearQuotaManager(options, dataType) {
  // Can not clear localStorage/indexedDB in private browsing mode,
  // just ignore.
  if (options.cookieStoreId == PRIVATE_STORE) {
    return;
  }

  let promises = [];
  await new Promise((resolve, reject) => {
    quotaManagerService.getUsage(request => {
      if (request.resultCode != Cr.NS_OK) {
        reject({ message: `Clear ${dataType} failed` });
        return;
      }

      for (let item of request.result) {
        let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
          item.origin
        );

        // Consistently to removeIndexedDB and the API documentation for
        // removeLocalStorage, we should only clear the data stored by
        // regular websites, on the contrary we shouldn't clear data stored
        // by browser components (like about:newtab) or other extensions.
        if (!["http", "https", "file"].includes(principal.scheme)) {
          continue;
        }

        let host = principal.hostPort;
        if (
          (!options.hostnames || options.hostnames.includes(host)) &&
          (!options.cookieStoreId ||
            getCookieStoreIdForOriginAttributes(principal.originAttributes) ===
              options.cookieStoreId)
        ) {
          promises.push(
            new Promise((resolve, reject) => {
              let clearRequest;
              if (dataType === "indexedDB") {
                clearRequest = quotaManagerService.clearStoragesForPrincipal(
                  principal,
                  null,
                  "idb"
                );
              } else {
                clearRequest = quotaManagerService.clearStoragesForPrincipal(
                  principal,
                  "default",
                  "ls"
                );
              }

              clearRequest.callback = () => {
                if (clearRequest.resultCode == Cr.NS_OK) {
                  resolve();
                } else {
                  reject({ message: `Clear ${dataType} failed` });
                }
              };
            })
          );
        }
      }

      resolve();
    });
  });

  return Promise.all(promises);
}

const clearIndexedDB = async function(options) {
  return clearQuotaManager(options, "indexedDB");
};

const clearLocalStorage = async function(options) {
  if (options.since) {
    return Promise.reject({
      message: "Firefox does not support clearing localStorage with 'since'.",
    });
  }

  // The legacy LocalStorage implementation that will eventually be removed
  // depends on this observer notification.  Some other subsystems like
  // Reporting headers depend on this too.
  // When NextGenLocalStorage is enabled these notifications are ignored.
  if (options.hostnames) {
    for (let hostname of options.hostnames) {
      Services.obs.notifyObservers(
        null,
        "extension:purge-localStorage",
        hostname
      );
    }
  } else {
    Services.obs.notifyObservers(null, "extension:purge-localStorage");
  }

  return clearQuotaManager(options, "localStorage");
};

const clearPasswords = async function(options) {
  let yieldCounter = 0;

  // Iterate through the logins and delete any updated after our cutoff.
  for (let login of await LoginHelper.getAllUserFacingLogins()) {
    login.QueryInterface(Ci.nsILoginMetaInfo);
    if (!options.since || login.timePasswordChanged >= options.since) {
      Services.logins.removeLogin(login);
      if (++yieldCounter % YIELD_PERIOD == 0) {
        await new Promise(resolve => setTimeout(resolve, 0)); // Don't block the main thread too long.
      }
    }
  }
};

const clearPluginData = options => {
  return clearData(options, Ci.nsIClearDataService.CLEAR_PLUGIN_DATA);
};

const clearServiceWorkers = options => {
  if (!options.hostnames) {
    return ServiceWorkerCleanUp.removeAll();
  }

  return Promise.all(
    options.hostnames.map(host => {
      return ServiceWorkerCleanUp.removeFromHost(host);
    })
  );
};

const doRemoval = (options, dataToRemove, extension) => {
  if (
    options.originTypes &&
    (options.originTypes.protectedWeb || options.originTypes.extension)
  ) {
    return Promise.reject({
      message:
        "Firefox does not support protectedWeb or extension as originTypes.",
    });
  }

  if (options.cookieStoreId) {
    const SUPPORTED_TYPES = ["cookies", "indexedDB"];
    if (Services.domStorageManager.nextGenLocalStorageEnabled) {
      // Only the next-gen storage supports removal by cookieStoreId.
      SUPPORTED_TYPES.push("localStorage");
    }

    for (let dataType in dataToRemove) {
      if (dataToRemove[dataType] && !SUPPORTED_TYPES.includes(dataType)) {
        return Promise.reject({
          message: `Firefox does not support clearing ${dataType} with 'cookieStoreId'.`,
        });
      }
    }

    if (
      !isPrivateCookieStoreId(options.cookieStoreId) &&
      !isDefaultCookieStoreId(options.cookieStoreId) &&
      !getContainerForCookieStoreId(options.cookieStoreId)
    ) {
      return Promise.reject({
        message: `Invalid cookieStoreId: ${options.cookieStoreId}`,
      });
    }
  }

  let removalPromises = [];
  let invalidDataTypes = [];
  for (let dataType in dataToRemove) {
    if (dataToRemove[dataType]) {
      switch (dataType) {
        case "cache":
          removalPromises.push(clearCache(options));
          break;
        case "cookies":
          removalPromises.push(clearCookies(options));
          break;
        case "downloads":
          removalPromises.push(clearDownloads(options));
          break;
        case "formData":
          removalPromises.push(clearFormData(options));
          break;
        case "history":
          removalPromises.push(clearHistory(options));
          break;
        case "indexedDB":
          removalPromises.push(clearIndexedDB(options));
          break;
        case "localStorage":
          removalPromises.push(clearLocalStorage(options));
          break;
        case "passwords":
          removalPromises.push(clearPasswords(options));
          break;
        case "pluginData":
          removalPromises.push(clearPluginData(options));
          break;
        case "serviceWorkers":
          removalPromises.push(clearServiceWorkers(options));
          break;
        default:
          invalidDataTypes.push(dataType);
      }
    }
  }
  if (extension && invalidDataTypes.length) {
    extension.logger.warn(
      `Firefox does not support dataTypes: ${invalidDataTypes.toString()}.`
    );
  }
  return Promise.all(removalPromises);
};

this.browsingData = class extends ExtensionAPI {
  getAPI(context) {
    let { extension } = context;
    return {
      browsingData: {
        settings() {
          const PREF_DOMAIN = "privacy.cpd.";
          // The following prefs are the only ones in Firefox that match corresponding
          // values used by Chrome when rerturning settings.
          const PREF_LIST = [
            "cache",
            "cookies",
            "history",
            "formdata",
            "downloads",
          ];

          // since will be the start of what is returned by Sanitizer.getClearRange
          // divided by 1000 to convert to ms.
          // If Sanitizer.getClearRange returns undefined that means the range is
          // currently "Everything", so we should set since to 0.
          let clearRange = Sanitizer.getClearRange();
          let since = clearRange ? clearRange[0] / 1000 : 0;
          let options = { since };

          let dataToRemove = {};
          let dataRemovalPermitted = {};

          for (let item of PREF_LIST) {
            // The property formData needs a different case than the
            // formdata preference.
            const name = item === "formdata" ? "formData" : item;
            dataToRemove[name] = Preferences.get(`${PREF_DOMAIN}${item}`);
            // Firefox doesn't have the same concept of dataRemovalPermitted
            // as Chrome, so it will always be true.
            dataRemovalPermitted[name] = true;
          }

          return Promise.resolve({
            options,
            dataToRemove,
            dataRemovalPermitted,
          });
        },
        remove(options, dataToRemove) {
          return doRemoval(options, dataToRemove, extension);
        },
        removeCache(options) {
          return doRemoval(options, { cache: true });
        },
        removeCookies(options) {
          return doRemoval(options, { cookies: true });
        },
        removeDownloads(options) {
          return doRemoval(options, { downloads: true });
        },
        removeFormData(options) {
          return doRemoval(options, { formData: true });
        },
        removeHistory(options) {
          return doRemoval(options, { history: true });
        },
        removeIndexedDB(options) {
          return doRemoval(options, { indexedDB: true });
        },
        removeLocalStorage(options) {
          return doRemoval(options, { localStorage: true });
        },
        removePasswords(options) {
          return doRemoval(options, { passwords: true });
        },
        removePluginData(options) {
          return doRemoval(options, { pluginData: true });
        },
      },
    };
  }
};
