/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function cleanup() {
  info("Forgetting closed tabs");
  while (ss.getClosedTabCount(window)) {
    ss.forgetClosedTab(window, 0);
  }
});

add_task(async function() {
  // Clear the list of closed windows.
  forgetClosedWindows();

  // Create a new window to attach our frame script to.
  let win = await promiseNewWindowLoaded({ private: true });

  // Create a new tab in the new window that will load the frame script.
  let tab = BrowserTestUtils.addTab(win.gBrowser, "about:mozilla");
  let browser = tab.linkedBrowser;
  await promiseBrowserLoaded(browser);
  await TabStateFlusher.flush(browser);

  // Check that we consider the tab as private.
  let state = JSON.parse(ss.getTabState(tab));
  ok(state.isPrivate, "tab considered private");

  // Ensure that closed tabs in a private windows can be restored.
  win.gBrowser.removeTab(tab);
  is(ss.getClosedTabCount(win), 1, "there is a single tab to restore");

  // Ensure that closed private windows can never be restored.
  await BrowserTestUtils.closeWindow(win);
  is(ss.getClosedWindowCount(), 0, "no windows to restore");
});
