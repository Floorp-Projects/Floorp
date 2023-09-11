/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Import common head.
/* import-globals-from ../../../../../toolkit/components/places/tests/head_common.js */
var commonFile = do_get_file(
  "../../../../../toolkit/components/places/tests/head_common.js",
  false
);
if (commonFile) {
  let uri = Services.io.newFileURI(commonFile);
  Services.scriptloader.loadSubScript(uri.spec, this);
}

// Put any other stuff relative to this test folder below.

ChromeUtils.defineESModuleGetters(this, {
  PlacesUIUtils: "resource:///modules/PlacesUIUtils.sys.mjs",
});

// Needed by some test that relies on having an app registered.
const { updateAppInfo } = ChromeUtils.importESModule(
  "resource://testing-common/AppInfo.sys.mjs"
);
updateAppInfo({
  name: "PlacesTest",
  ID: "{230de50e-4cd1-11dc-8314-0800200c9a66}",
  version: "1",
  platformVersion: "",
});

// Default bookmarks constants.
const DEFAULT_BOOKMARKS_ON_TOOLBAR = 1;
const DEFAULT_BOOKMARKS_ON_MENU = 1;

var createCorruptDB = async function () {
  let dbPath = PathUtils.join(PathUtils.profileDir, "places.sqlite");
  await IOUtils.remove(dbPath);

  // Create a corrupt database.
  let src = PathUtils.join(do_get_cwd().path, "corruptDB.sqlite");
  await IOUtils.copy(src, dbPath);

  // Check there's a DB now.
  Assert.ok(await IOUtils.exists(dbPath), "should have a DB now");
};

const SINGLE_TRY_TIMEOUT = 100;
const NUMBER_OF_TRIES = 30;

/**
 * Similar to waitForConditionPromise, but poll for an asynchronous value
 * every SINGLE_TRY_TIMEOUT ms, for no more than tryCount times.
 *
 * @param {Function} promiseFn
 *        A function to generate a promise, which resolves to the expected
 *        asynchronous value.
 * @param {msg} timeoutMsg
 *        The reason to reject the returned promise with.
 * @param {number} [tryCount]
 *        Maximum times to try before rejecting the returned promise with
 *        timeoutMsg, defaults to NUMBER_OF_TRIES.
 * @returns {Promise} to the asynchronous value being polled.
 * @throws if the asynchronous value is not available after tryCount attempts.
 */
var waitForResolvedPromise = async function (
  promiseFn,
  timeoutMsg,
  tryCount = NUMBER_OF_TRIES
) {
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
