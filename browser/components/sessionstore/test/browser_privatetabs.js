/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function cleanup() {
  info("Forgetting closed tabs");
  while (ss.getClosedTabCount(window)) {
    ss.forgetClosedTab(window, 0);
  }
});

add_task(function() {
  let URL_PUBLIC = "http://example.com/public/" + Math.random();
  let URL_PRIVATE = "http://example.com/private/" + Math.random();
  let tab1, tab2;
  try {
    // Setup a public tab and a private tab
    info("Setting up public tab");
    tab1 = gBrowser.addTab(URL_PUBLIC);
    yield promiseBrowserLoaded(tab1.linkedBrowser);

    info("Setting up private tab");
    tab2 = gBrowser.addTab();
    yield promiseBrowserLoaded(tab2.linkedBrowser);
    yield setUsePrivateBrowsing(tab2.linkedBrowser, true);
    tab2.linkedBrowser.loadURI(URL_PRIVATE);
    yield promiseBrowserLoaded(tab2.linkedBrowser);

    info("Flush to make sure chrome received all data.");
    yield TabStateFlusher.flush(tab1.linkedBrowser);
    yield TabStateFlusher.flush(tab2.linkedBrowser);

    info("Checking out state");
    let state = yield promiseRecoveryFileContents();

    info("State: " + state);
    // Ensure that sessionstore.js only knows about the public tab
    ok(state.indexOf(URL_PUBLIC) != -1, "State contains public tab");
    ok(state.indexOf(URL_PRIVATE) == -1, "State does not contain private tab");

    // Ensure that we can close and undo close the public tab but not the private tab
    gBrowser.removeTab(tab2);
    tab2 = null;

    gBrowser.removeTab(tab1);
    tab1 = null;

    tab1 = ss.undoCloseTab(window, 0);
    ok(true, "Public tab supports undo close");

    is(ss.getClosedTabCount(window), 0, "Private tab does not support undo close");

  } finally {
    if (tab1) {
      gBrowser.removeTab(tab1);
    }
    if (tab2) {
      gBrowser.removeTab(tab2);
    }
  }
});

add_task(function () {
  const FRAME_SCRIPT = "data:," +
    "docShell.QueryInterface%28Components.interfaces.nsILoadContext%29.usePrivateBrowsing%3Dtrue";

  // Clear the list of closed windows.
  forgetClosedWindows();

  // Create a new window to attach our frame script to.
  let win = yield promiseNewWindowLoaded();
  let mm = win.getGroupMessageManager("browsers");
  mm.loadFrameScript(FRAME_SCRIPT, true);

  // Create a new tab in the new window that will load the frame script.
  let tab = win.gBrowser.addTab("about:mozilla");
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);
  yield TabStateFlusher.flush(browser);

  // Check that we consider the tab as private.
  let state = JSON.parse(ss.getTabState(tab));
  ok(state.isPrivate, "tab considered private");

  // Ensure we don't allow restoring closed private tabs in non-private windows.
  win.gBrowser.removeTab(tab);
  is(ss.getClosedTabCount(win), 0, "no tabs to restore");

  // Create a new tab in the new window that will load the frame script.
  tab = win.gBrowser.addTab("about:mozilla");
  browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);
  yield TabStateFlusher.flush(browser);

  // Check that we consider the tab as private.
  state = JSON.parse(ss.getTabState(tab));
  ok(state.isPrivate, "tab considered private");

  // Check that all private tabs are removed when the non-private
  // window is closed and we don't save windows without any tabs.
  yield promiseWindowClosed(win);
  is(ss.getClosedWindowCount(), 0, "no windows to restore");
});

add_task(function () {
  // Clear the list of closed windows.
  forgetClosedWindows();

  // Create a new window to attach our frame script to.
  let win = yield promiseNewWindowLoaded({private: true});

  // Create a new tab in the new window that will load the frame script.
  let tab = win.gBrowser.addTab("about:mozilla");
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);
  yield TabStateFlusher.flush(browser);

  // Check that we consider the tab as private.
  let state = JSON.parse(ss.getTabState(tab));
  ok(state.isPrivate, "tab considered private");

  // Ensure that closed tabs in a private windows can be restored.
  win.gBrowser.removeTab(tab);
  is(ss.getClosedTabCount(win), 1, "there is a single tab to restore");

  // Ensure that closed private windows can never be restored.
  yield promiseWindowClosed(win);
  is(ss.getClosedWindowCount(), 0, "no windows to restore");
});

function setUsePrivateBrowsing(browser, val) {
  return sendMessage(browser, "ss-test:setUsePrivateBrowsing", val);
}

