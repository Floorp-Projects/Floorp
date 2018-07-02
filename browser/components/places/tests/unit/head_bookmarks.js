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

ChromeUtils.defineModuleGetter(this, "PlacesUIUtils",
                               "resource:///modules/PlacesUIUtils.jsm");

// Needed by some test that relies on having an app registered.
ChromeUtils.import("resource://testing-common/AppInfo.jsm", this);
updateAppInfo({
  name: "PlacesTest",
  ID: "{230de50e-4cd1-11dc-8314-0800200c9a66}",
  version: "1",
  platformVersion: "",
});

// Default bookmarks constants.
const DEFAULT_BOOKMARKS_ON_TOOLBAR = 1;
const DEFAULT_BOOKMARKS_ON_MENU = 1;

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
