add_task(function* setup() {
  // We want to suppress all non-final updates from the browser tabs
  // so as to eliminate any racy-ness with this test.
  yield pushPrefs(["browser.sessionstore.debug.no_auto_updates", true]);
});

/**
 * Tests that if we initially discard a window as not interesting
 * to save in the closed windows array, that we revisit that decision
 * after a window flush has completed.
 */
add_task(function* test_add_interesting_window() {
  // Depending on previous tests, we might already have some closed
  // windows stored. We'll use its length to determine whether or not
  // the window was added or not.
  let initialClosedWindows = ss.getClosedWindowCount();

  // Make sure we can actually store another closed window
  yield pushPrefs(["browser.sessionstore.max_windows_undo",
                   initialClosedWindows + 1]);

  // Create a new browser window. Since the default window will start
  // at about:blank, SessionStore should find this tab (and therefore the
  // whole window) uninteresting, and should not initially put it into
  // the closed windows array.
  let newWin = yield BrowserTestUtils.openNewBrowserWindow();

  let browser = newWin.gBrowser.selectedBrowser;

  // Send a message that will cause the content to change its location
  // asynchronously so that we have a chance to close the window before
  // an update comes up.
  ContentTask.spawn(browser, null, function*() {
    content.location = "http://example.com";
  });

  // for e10s, this will cause a remoteness switch, since the
  // initial browser in a newly opened window will not be remote.
  // We need to wait for that remoteness change before we attach
  // our OnHistoryReplaceEntry listener.
  if (gMultiProcessBrowser) {
    yield BrowserTestUtils.waitForEvent(newWin.gBrowser.selectedTab,
                                        "TabRemotenessChange");
  }

  yield promiseContentMessage(browser, "ss-test:OnHistoryReplaceEntry");

  // Clear out the userTypedValue so that the new window looks like
  // it's really not worth restoring.
  browser.userTypedValue = null;

  // Once the domWindowClosed Promise resolves, the window should
  // have closed, and SessionStore's onClose handler should have just
  // run.
  let domWindowClosed = BrowserTestUtils.domWindowClosed(newWin);

  // Once this windowClosed Promise resolves, we should have finished
  // the flush and revisited our decision to put this window into
  // the closed windows array.
  let windowClosed = BrowserTestUtils.windowClosed(newWin);

  // Ok, let's close the window.
  newWin.close();

  yield domWindowClosed;
  // OnClose has just finished running.
  let currentClosedWindows = ss.getClosedWindowCount();
  is(currentClosedWindows, initialClosedWindows,
     "We should not have added the window to the closed windows array");

  yield windowClosed;
  // The window flush has finished
  currentClosedWindows = ss.getClosedWindowCount();
  is(currentClosedWindows,
     initialClosedWindows + 1,
     "We should have added the window to the closed windows array");
});

/**
 * Tests that if we initially store a closed window as interesting
 * to save in the closed windows array, that we revisit that decision
 * after a window flush has completed, and stop storing a window that
 * we've deemed no longer interesting.
 */
add_task(function* test_remove_uninteresting_window() {
  // Depending on previous tests, we might already have some closed
  // windows stored. We'll use its length to determine whether or not
  // the window was added or not.
  let initialClosedWindows = ss.getClosedWindowCount();

  // Make sure we can actually store another closed window
  yield pushPrefs(["browser.sessionstore.max_windows_undo",
                   initialClosedWindows + 1]);

  let newWin = yield BrowserTestUtils.openNewBrowserWindow();

  // Now browse the initial tab of that window to an interesting
  // site, and flush the tab so that the parent knows that this
  // is indeed a tab worth saving.
  let browser = newWin.gBrowser.selectedBrowser;
  browser.loadURI("http://example.com");
  yield BrowserTestUtils.browserLoaded(browser);
  yield TabStateFlusher.flush(browser);

  // Send a message that will cause the content to purge its
  // history entries and make itself seem uninteresting.
  ContentTask.spawn(browser, null, function*() {
    // Epic hackery to make this browser seem suddenly boring.
    Components.utils.import("resource://gre/modules/BrowserUtils.jsm");
    docShell.setCurrentURI(BrowserUtils.makeURI("about:blank"));

    let {sessionHistory} = docShell.QueryInterface(Ci.nsIWebNavigation);
    sessionHistory.PurgeHistory(sessionHistory.count);
  });

  // Once the domWindowClosed Promise resolves, the window should
  // have closed, and SessionStore's onClose handler should have just
  // run.
  let domWindowClosed = BrowserTestUtils.domWindowClosed(newWin);

  // Once this windowClosed Promise resolves, we should have finished
  // the flush and revisited our decision to put this window into
  // the closed windows array.
  let windowClosed = BrowserTestUtils.windowClosed(newWin);

  // Ok, let's close the window.
  newWin.close();

  yield domWindowClosed;
  // OnClose has just finished running.
  let currentClosedWindows = ss.getClosedWindowCount();
  is(currentClosedWindows, initialClosedWindows + 1,
     "We should have added the window to the closed windows array");

  yield windowClosed;
  // The window flush has finished
  currentClosedWindows = ss.getClosedWindowCount();
  is(currentClosedWindows,
     initialClosedWindows,
     "We should have removed the window from the closed windows array");
});