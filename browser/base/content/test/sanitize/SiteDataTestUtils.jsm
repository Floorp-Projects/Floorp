"use strict";

var EXPORTED_SYMBOLS = [
  "SiteDataTestUtils",
];

ChromeUtils.import("resource://gre/modules/Services.jsm");
const {Sanitizer} = ChromeUtils.import("resource:///modules/Sanitizer.jsm", {});

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
   *
   * @returns a Promise that resolves when the data was added successfully.
   */
  addToIndexedDB(origin) {
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
        store.put({ id: performance.now().toString(), description: "IndexedDB Test"});
      };
    });
  },

  /**
   * Adds a new cookie for the specified origin, with the specified contents.
   * The cookie will be valid for one day.
   *
   * @param {String} name - the cookie name
   * @param {String} value - the cookie value
   */
  addToCookies(origin, name, value) {
    let uri = Services.io.newURI(origin);
    Services.cookies.add(uri.host, uri.pathQueryRef, name, value,
      false, false, false, Date.now() + 24000 * 60 * 60);
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
