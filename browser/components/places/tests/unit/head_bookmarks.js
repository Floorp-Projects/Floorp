/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/Services.jsm");

// Import common head.
/* import-globals-from ../../../../../toolkit/components/places/tests/head_common.js */
var commonFile = do_get_file("../../../../../toolkit/components/places/tests/head_common.js", false);
if (commonFile) {
  let uri = Services.io.newFileURI(commonFile);
  Services.scriptloader.loadSubScript(uri.spec, this);
}

// Put any other stuff relative to this test folder below.

XPCOMUtils.defineLazyGetter(this, "PlacesUIUtils", function() {
  ChromeUtils.import("resource:///modules/PlacesUIUtils.jsm");
  return PlacesUIUtils;
});

// Needed by some test that relies on having an app registered.
ChromeUtils.import("resource://testing-common/AppInfo.jsm", this);
updateAppInfo({
  name: "PlacesTest",
  ID: "{230de50e-4cd1-11dc-8314-0800200c9a66}",
  version: "1",
  platformVersion: "",
});

// Smart bookmarks constants.
const SMART_BOOKMARKS_VERSION = 8;
const SMART_BOOKMARKS_ON_TOOLBAR = 1;
const SMART_BOOKMARKS_ON_MENU =  2; // Takes into account the additional separator.

// Default bookmarks constants.
const DEFAULT_BOOKMARKS_ON_TOOLBAR = 1;
const DEFAULT_BOOKMARKS_ON_MENU = 1;

const SMART_BOOKMARKS_ANNO = "Places/SmartBookmark";

function checkItemHasAnnotation(guid, name) {
  return PlacesUtils.promiseItemId(guid).then(id => {
    let hasAnnotation = PlacesUtils.annotations.itemHasAnnotation(id, name);
    Assert.ok(hasAnnotation, `Expected annotation ${name}`);
  });
}

var createCorruptDB = async function() {
  let dbPath = OS.Path.join(OS.Constants.Path.profileDir, "places.sqlite");
  await OS.File.remove(dbPath);

  // Create a corrupt database.
  let dir = await OS.File.getCurrentDirectory();
  let src = OS.Path.join(dir, "corruptDB.sqlite");
  await OS.File.copy(src, dbPath);

  // Check there's a DB now.
  Assert.ok((await OS.File.exists(dbPath)), "should have a DB now");
};

/**
 * Rebuilds smart bookmarks listening to console output to report any message or
 * exception generated.
 *
 * @return {Promise}
 *         Resolved when done.
 */
function rebuildSmartBookmarks() {
  let consoleListener = {
    observe(aMsg) {
      if (aMsg.message.startsWith("[JavaScript Warning:")) {
        // TODO (Bug 1300416): Ignore spurious strict warnings.
        return;
      }
      do_throw("Got console message: " + aMsg.message);
    },
    QueryInterface: ChromeUtils.generateQI([ Ci.nsIConsoleListener ]),
  };
  Services.console.reset();
  Services.console.registerListener(consoleListener);
  registerCleanupFunction(() => {
    try {
      Services.console.unregisterListener(consoleListener);
    } catch (ex) { /* will likely fail */ }
  });
  Cc["@mozilla.org/browser/browserglue;1"]
    .getService(Ci.nsIObserver)
    .observe(null, "browser-glue-test", "smart-bookmarks-init");
  return promiseTopicObserved("test-smart-bookmarks-done").then(() => {
    Services.console.unregisterListener(consoleListener);
  });
}

const SINGLE_TRY_TIMEOUT = 100;
const NUMBER_OF_TRIES = 30;

/**
 * Similar to waitForConditionPromise, but poll for an asynchronous value
 * every SINGLE_TRY_TIMEOUT ms, for no more than tryCount times.
 *
 * @param promiseFn
 *        A function to generate a promise, which resolves to the expected
 *        asynchronous value.
 * @param timeoutMsg
 *        The reason to reject the returned promise with.
 * @param [optional] tryCount
 *        Maximum times to try before rejecting the returned promise with
 *        timeoutMsg, defaults to NUMBER_OF_TRIES.
 * @return {Promise}
 * @resolves to the asynchronous value being polled.
 * @rejects if the asynchronous value is not available after tryCount attempts.
 */
var waitForResolvedPromise = async function(promiseFn, timeoutMsg, tryCount = NUMBER_OF_TRIES) {
  let tries = 0;
  do {
    try {
      let value = await promiseFn();
      return value;
    } catch (ex) {}
    await new Promise(resolve => do_timeout(SINGLE_TRY_TIMEOUT, resolve));
  } while (++tries <= tryCount);
  throw new Error(timeoutMsg);
};
