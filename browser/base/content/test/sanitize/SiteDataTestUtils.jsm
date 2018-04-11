"use strict";

var EXPORTED_SYMBOLS = [
  "SiteDataTestUtils",
];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://testing-common/ContentTask.jsm");
ChromeUtils.import("resource://testing-common/BrowserTestUtils.jsm");

const {Sanitizer} = ChromeUtils.import("resource:///modules/Sanitizer.jsm", {});

XPCOMUtils.defineLazyServiceGetter(this, "swm",
                                   "@mozilla.org/serviceworkers/manager;1",
                                   "nsIServiceWorkerManager");

/**
 * This module assists with tasks around testing functionality that shows
 * or clears site data.
 *
 * Please note that you will have to clean up changes made manually, for
 * example using SiteDataTestUtils.clear().
 */
var SiteDataTestUtils = {

  /**
   * Adds a new entry to a dummy indexedDB database for the specified origin.
   *
   * @param {String} origin - the origin of the site to add test data for
   * @param {String} name [optional] - the entry key
   * @param {String} value [optional] - the entry value
   *
   * @returns a Promise that resolves when the data was added successfully.
   */
  addToIndexedDB(origin, key = "foo", value = "bar") {
    return new Promise(resolve => {
      let uri = Services.io.newURI(origin);
      let principal = Services.scriptSecurityManager.createCodebasePrincipal(uri, {});
      let request = indexedDB.openForPrincipal(principal, "TestDatabase", 1);
      request.onupgradeneeded = function(e) {
        let db = e.target.result;
        db.createObjectStore("TestStore", { keyPath: "id" });
      };
      request.onsuccess = function(e) {
        let db = e.target.result;
        let tx = db.transaction("TestStore", "readwrite");
        let store = tx.objectStore("TestStore");
        tx.oncomplete = resolve;
        store.put({ id: key, description: value});
      };
    });
  },

  /**
   * Adds a new cookie for the specified origin, with the specified contents.
   * The cookie will be valid for one day.
   *
   * @param {String} origin - the origin of the site to add test data for
   * @param {String} name [optional] - the cookie name
   * @param {String} value [optional] - the cookie value
   */
  addToCookies(origin, name = "foo", value = "bar") {
    let uri = Services.io.newURI(origin);
    Services.cookies.add(uri.host, uri.pathQueryRef, name, value,
      false, false, false, Date.now() + 24000 * 60 * 60);
  },

  /**
   * Adds a new serviceworker with the specified path. Note that this
   * method will open a new tab at the domain of the SW path to that effect.
   *
   * @param {String} path - the path to the service worker to add.
   *
   * @returns a Promise that resolves when the service worker was registered
   */
  addServiceWorker(path) {
    let uri = Services.io.newURI(path);
    // Register a dummy ServiceWorker.
    return BrowserTestUtils.withNewTab(uri.prePath, async function(browser) {
      return ContentTask.spawn(browser, {path}, async ({path: p}) => {
        let r = await content.navigator.serviceWorker.register(p);
        return new Promise(resolve => {
          let worker = r.installing;
          worker.addEventListener("statechange", () => {
            if (worker.state === "installed") {
              resolve();
            }
          });
        });
      });
    });
  },

  /**
   * Checks whether the specified origin has registered ServiceWorkers.
   *
   * @param {String} origin - the origin of the site to check
   *
   * @returns {Boolean} whether or not the site has ServiceWorkers.
   */
  hasServiceWorkers(origin) {
    let serviceWorkers = swm.getAllRegistrations();
    for (let i = 0; i < serviceWorkers.length; i++) {
      let sw = serviceWorkers.queryElementAt(i, Ci.nsIServiceWorkerRegistrationInfo);
      if (sw.principal.origin == origin) {
        return true;
      }
    }
    return false;
  },

  /**
   * Gets the current quota usage for the specified origin.
   *
   * @returns a Promise that resolves to an integer with the total
   *          amount of disk usage by a given origin.
   */
  getQuotaUsage(origin) {
    return new Promise(resolve => {
      let uri = Services.io.newURI(origin);
      let principal = Services.scriptSecurityManager.createCodebasePrincipal(uri, {});
      Services.qms.getUsageForPrincipal(principal, request => resolve(request.result.usage));
    });
  },

  /**
   * Cleans up all site data.
   */
  clear() {
    return Sanitizer.sanitize(["cookies", "cache", "siteSettings", "offlineApps"]);
  },
};
