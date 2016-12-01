"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Sanitizer",
                                  "resource:///modules/Sanitizer.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "setTimeout",
                                  "resource://gre/modules/Timer.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cookieMgr",
                                   "@mozilla.org/cookiemanager;1",
                                   "nsICookieManager");

/**
* A number of iterations after which to yield time back
* to the system.
*/
const YIELD_PERIOD = 10;

const PREF_DOMAIN = "privacy.cpd.";

XPCOMUtils.defineLazyGetter(this, "sanitizer", () => {
  let sanitizer = new Sanitizer();
  sanitizer.prefDomain = PREF_DOMAIN;
  return sanitizer;
});

function clearCache() {
  // Clearing the cache does not support timestamps.
  return sanitizer.items.cache.clear();
}

let clearCookies = Task.async(function* (options) {
  // This code has been borrowed from sanitize.js.
  let yieldCounter = 0;

  if (options.since) {
    // Iterate through the cookies and delete any created after our cutoff.
    let cookiesEnum = cookieMgr.enumerator;
    while (cookiesEnum.hasMoreElements()) {
      let cookie = cookiesEnum.getNext().QueryInterface(Ci.nsICookie2);

      if (cookie.creationTime > PlacesUtils.toPRTime(options.since)) {
        // This cookie was created after our cutoff, clear it.
        cookieMgr.remove(cookie.host, cookie.name, cookie.path,
                         false, cookie.originAttributes);

        if (++yieldCounter % YIELD_PERIOD == 0) {
          yield new Promise(resolve => setTimeout(resolve, 0)); // Don't block the main thread too long.
        }
      }
    }
  } else {
    // Remove everything.
    cookieMgr.removeAll();
  }
});

function doRemoval(options, dataToRemove, extension) {
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
}

extensions.registerSchemaAPI("browsingData", "addon_parent", context => {
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
        let since = Sanitizer.getClearRange()[0] / 1000;
        let options = {since};

        let dataToRemove = {};
        let dataRemovalPermitted = {};

        for (let item of PREF_LIST) {
          dataToRemove[item] = Preferences.get(`${PREF_DOMAIN}${item}`);
          // Firefox doesn't have the same concept of dataRemovalPermitted
          // as Chrome, so it will always be true.
          dataRemovalPermitted[item] = true;
        }
        // formData has a different case than the pref formdata.
        dataToRemove.formData = Preferences.get(`${PREF_DOMAIN}formdata`);
        dataRemovalPermitted.formData = true;

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
    },
  };
});
