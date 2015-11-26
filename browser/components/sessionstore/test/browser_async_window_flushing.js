/**
 * Tests that when we close a window, it is immediately removed from the
 * _windows array.
 */
add_task(function* test_synchronously_remove_window_state() {
  // Depending on previous tests, we might already have some closed
  // windows stored. We'll use its length to determine whether or not
  // the window was added or not.
  let state = JSON.parse(ss.getBrowserState());
  ok(state, "Make sure we can get the state");
  let initialWindows = state.windows.length;

  // Open a new window and send the first tab somewhere
  // interesting.
  let newWin = yield BrowserTestUtils.openNewBrowserWindow();
  let browser = newWin.gBrowser.selectedBrowser;
  browser.loadURI("http://example.com");
  yield BrowserTestUtils.browserLoaded(browser);
  yield TabStateFlusher.flush(browser);

  state = JSON.parse(ss.getBrowserState());
  is(state.windows.length, initialWindows + 1,
     "The new window to be in the state");

  // Now close the window, and make sure that the window was removed
  // from the windows list from the SessionState. We're specifically
  // testing the case where the window is _not_ removed in between
  // the close-initiated flush request and the flush response.
  let windowClosed = BrowserTestUtils.windowClosed(newWin);
  newWin.close();

  state = JSON.parse(ss.getBrowserState());
  is(state.windows.length, initialWindows,
     "The new window should have been removed from the state");

  // Wait for our window to go away
  yield windowClosed;
});
