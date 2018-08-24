"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");

var EXPORTED_SYMBOLS = [
  "SearchTestUtils",
];

var gTestGlobals;

var SearchTestUtils = Object.freeze({
  init(Assert, registerCleanupFunction) {
    gTestGlobals = {
      Assert,
      registerCleanupFunction
    };
  },

  /**
   * Adds a search engine to the search service. It will remove the engine
   * at the end of the test.
   *
   * @param {String}   url                     The URL of the engine to add.
   * @param {Function} registerCleanupFunction Pass the registerCleanupFunction
   *                                           from the test's scope.
   * @returns {Promise} Returns a promise that is resolved with the new engine
   *                    or rejected if it fails.
   */
  promiseNewSearchEngine(url) {
    return new Promise((resolve, reject) => {
      Services.search.addEngine(url, null, "", false, {
        onSuccess(engine) {
          gTestGlobals.registerCleanupFunction(() => Services.search.removeEngine(engine));
          resolve(engine);
        },
        onError(errCode) {
          gTestGlobals.Assert.ok(false, `addEngine failed with error code ${errCode}`);
          reject();
        },
      });
    });
  }
});
