/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * With session restore enabled and quit on last tab close enabled
 * When closing the last tab while having another private window,
 * it shouldn't be restored at the next startup of Firefox.
 * See bug 1732366 for more information.
 */
add_task(async function test_bug_1730021() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.sessionstore.resume_session_once", true]],
  });
  ok(SessionStore.willAutoRestore, "the session will be restored if we quit");

  SessionStore.maybeDontSaveTabs(window);
  ok(window._dontSaveTabs, "the tabs should be closed at quit");
  delete window._dontSaveTabs;
  ok(!window._dontSaveTabs, "the flag should be reset");

  let newWin = await BrowserTestUtils.openNewBrowserWindow({ private: true });

  SessionStore.maybeDontSaveTabs(window);
  ok(
    window._dontSaveTabs,
    "the tabs should be closed at quit even if a private window is open"
  );
  delete window._dontSaveTabs;
  ok(!window._dontSaveTabs, "the flag should be reset");

  await BrowserTestUtils.closeWindow(newWin);
});
