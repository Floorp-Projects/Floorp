/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let tmp = {};
Cu.import("resource:///modules/sessionstore/SessionSaver.jsm", tmp);
let {SessionSaver} = tmp;

const URL = ROOT + "browser_454908_sample.html";
const PASS = "pwd-" + Math.random();

/**
 * Bug 454908 - Don't save/restore values of password fields.
 */
add_task(function* test_dont_save_passwords() {
  // Make sure we do save form data.
  Services.prefs.clearUserPref("browser.sessionstore.privacy_level");

  // Add a tab with a password field.
  let tab = gBrowser.addTab(URL);
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // Fill in some values.
  let usernameValue = "User " + Math.random();
  yield setInputValue(browser, {id: "username", value: usernameValue});
  yield setInputValue(browser, {id: "passwd", value: PASS});

  // Close and restore the tab.
  gBrowser.removeTab(tab);
  tab = ss.undoCloseTab(window, 0);
  browser = tab.linkedBrowser;
  yield promiseTabRestored(tab);

  // Check that password fields aren't saved/restored.
  let username = yield getInputValue(browser, {id: "username"});
  is(username, usernameValue, "username was saved/restored");
  let passwd = yield getInputValue(browser, {id: "passwd"});
  is(passwd, "", "password wasn't saved/restored");

  // Write to disk and read our file.
  yield forceSaveState();
  yield promiseForEachSessionRestoreFile((state, key) =>
    // Ensure that we have not saved our password.
    ok(!state.contains(PASS), "password has not been written to file " + key)
  );


  // Cleanup.
  gBrowser.removeTab(tab);
});
