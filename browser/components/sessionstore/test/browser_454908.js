/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URL = ROOT + "browser_454908_sample.html";
const PASS = "pwd-" + Math.random();

/**
 * Bug 454908 - Don't save/restore values of password fields.
 */
add_task(async function test_dont_save_passwords() {
  // Make sure we do save form data.
  Services.prefs.clearUserPref("browser.sessionstore.privacy_level");

  // Add a tab with a password field.
  let tab = BrowserTestUtils.addTab(gBrowser, URL);
  let browser = tab.linkedBrowser;
  await promiseBrowserLoaded(browser);

  // Fill in some values.
  let usernameValue = "User " + Math.random();
  await setInputValue(browser, {id: "username", value: usernameValue});
  await setInputValue(browser, {id: "passwd", value: PASS});

  // Close and restore the tab.
  await promiseRemoveTab(tab);
  tab = ss.undoCloseTab(window, 0);
  browser = tab.linkedBrowser;
  await promiseTabRestored(tab);

  // Check that password fields aren't saved/restored.
  let username = await getInputValue(browser, {id: "username"});
  is(username, usernameValue, "username was saved/restored");
  let passwd = await getInputValue(browser, {id: "passwd"});
  is(passwd, "", "password wasn't saved/restored");

  // Write to disk and read our file.
  await forceSaveState();
  await promiseForEachSessionRestoreFile((state, key) =>
    // Ensure that we have not saved our password.
    ok(!state.includes(PASS), "password has not been written to file " + key)
  );

  // Cleanup.
  gBrowser.removeTab(tab);
});
