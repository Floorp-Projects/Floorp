/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";


ChromeUtils.defineModuleGetter(this, "PlacesUtils",
                               "resource://gre/modules/PlacesUtils.jsm");
ChromeUtils.defineModuleGetter(this, "Preferences",
                               "resource://gre/modules/Preferences.jsm");
ChromeUtils.defineModuleGetter(this, "Sanitizer",
                               "resource:///modules/Sanitizer.jsm");
ChromeUtils.defineModuleGetter(this, "Services",
                               "resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(this, "setTimeout",
                               "resource://gre/modules/Timer.jsm");
ChromeUtils.defineModuleGetter(this, "ServiceWorkerCleanUp",
                               "resource://gre/modules/ServiceWorkerCleanUp.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "quotaManagerService",
                                   "@mozilla.org/dom/quota-manager-service;1",
                                   "nsIQuotaManagerService");

/**
* A number of iterations after which to yield time back
* to the system.
*/
const YIELD_PERIOD = 10;

const makeRange = options => {
  return (options.since == null) ?
    null :
    [PlacesUtils.toPRTime(options.since), PlacesUtils.toPRTime(Date.now())];
};

const clearCache = () => {
  // Clearing the cache does not support timestamps.
  return Sanitizer.items.cache.clear();
};

const clearCookies = async function(options) {
  let cookieMgr = Services.cookies;
  // This code has been borrowed from Sanitizer.jsm.
  let yieldCounter = 0;

  if (options.since || options.hostnames) {
    // Iterate through the cookies and delete any created after our cutoff.
    for (const cookie of XPCOMUtils.IterSimpleEnumerator(cookieMgr.enumerator, Ci.nsICookie2)) {
      if ((!options.since || cookie.creationTime >= PlacesUtils.toPRTime(options.since)) &&
          (!options.hostnames || options.hostnames.includes(cookie.host.replace(/^\./, "")))) {
        // This cookie was created after our cutoff, clear it.
        cookieMgr.remove(cookie.host, cookie.name, cookie.path,
                         false, cookie.originAttributes);

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

const clearIndexedDB = async function(options) {
  let promises = [];

  await new Promise(resolve => {
    quotaManagerService.getUsage(request => {
      if (request.resultCode != Cr.NS_OK) {
        // We are probably shutting down. We don't want to propagate the error,
        // rejecting the promise.
        resolve();
        return;
      }

      for (let item of request.result) {
        let principal = Services.scriptSecurityManager.createCodebasePrincipalFromOrigin(item.origin);
        let uri = principal.URI;
        if (uri.scheme == "http" || uri.scheme == "https" || uri.scheme == "file") {
          promises.push(new Promise(r => {
            let req = quotaManagerService.clearStoragesForPrincipal(principal, null, false);
            req.callback = () => { r(); };
          }));
        }
      }

      resolve();
    });
  });

  return Promise.all(promises);
};

const clearLocalStorage = async function(options) {
  if (options.since) {
    return Promise.reject(
      {message: "Firefox does not support clearing localStorage with 'since'."});
  }
  if (options.hostnames) {
    for (let hostname of options.hostnames) {
      Services.obs.notifyObservers(null, "extension:purge-localStorage", hostname);
    }
  } else {
    Services.obs.notifyObservers(null, "extension:purge-localStorage");
  }
};

const clearPasswords = async function(options) {
  let loginManager = Services.logins;
  let yieldCounter = 0;

  if (options.since) {
    // Iterate through the logins and delete any updated after our cutoff.
    let logins = loginManager.getAllLogins();
    for (let login of logins) {
      login.QueryInterface(Ci.nsILoginMetaInfo);
      if (login.timePasswordChanged >= options.since) {
        loginManager.removeLogin(login);
        if (++yieldCounter % YIELD_PERIOD == 0) {
          await new Promise(resolve => setTimeout(resolve, 0)); // Don't block the main thread too long.
        }
      }
    }
  } else {
    // Remove everything.
    loginManager.removeAllLogins();
  }
};

const clearPluginData = options => {
  return Sanitizer.items.pluginData.clear(makeRange(options));
};

const doRemoval = (options, dataToRemove, extension) => {
  if (options.originTypes &&
      (options.originTypes.protectedWeb || options.originTypes.extension)) {
    return Promise.reject(
      {message: "Firefox does not support protectedWeb or extension as originTypes."});
  }

  let removalPromises = [];
  let invalidDataTypes = [];
  for (let dataType in dataToRemove) {
    if (dataToRemove[dataType]) {
      switch (dataType) {
        case "cache":
          removalPromises.push(clearCache());
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
          removalPromises.push(ServiceWorkerCleanUp.removeAll());
          break;
        default:
          invalidDataTypes.push(dataType);
      }
    }
  }
  if (extension && invalidDataTypes.length) {
    extension.logger.warn(
      `Firefox does not support dataTypes: ${invalidDataTypes.toString()}.`);
  }
  return Promise.all(removalPromises);
};

this.browsingData = class extends ExtensionAPI {
  getAPI(context) {
    let {extension} = context;
    return {
      browsingData: {
        settings() {
          const PREF_DOMAIN = "privacy.cpd.";
          // The following prefs are the only ones in Firefox that match corresponding
          // values used by Chrome when rerturning settings.
          const PREF_LIST = ["cache", "cookies", "history", "formdata", "downloads"];

          // since will be the start of what is returned by Sanitizer.getClearRange
          // divided by 1000 to convert to ms.
          // If Sanitizer.getClearRange returns undefined that means the range is
          // currently "Everything", so we should set since to 0.
          let clearRange = Sanitizer.getClearRange();
          let since = clearRange ? clearRange[0] / 1000 : 0;
          let options = {since};

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

          return Promise.resolve({options, dataToRemove, dataRemovalPermitted});
        },
        remove(options, dataToRemove) {
          return doRemoval(options, dataToRemove, extension);
        },
        removeCache(options) {
          return doRemoval(options, {cache: true});
        },
        removeCookies(options) {
          return doRemoval(options, {cookies: true});
        },
        removeDownloads(options) {
          return doRemoval(options, {downloads: true});
        },
        removeFormData(options) {
          return doRemoval(options, {formData: true});
        },
        removeHistory(options) {
          return doRemoval(options, {history: true});
        },
        removeIndexedDB(options) {
          return doRemoval(options, {indexedDB: true});
        },
        removeLocalStorage(options) {
          return doRemoval(options, {localStorage: true});
        },
        removePasswords(options) {
          return doRemoval(options, {passwords: true});
        },
        removePluginData(options) {
          return doRemoval(options, {pluginData: true});
        },
      },
    };
  }
};
