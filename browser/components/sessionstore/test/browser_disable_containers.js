"use strict";

/**
 * This test is to see if tabs can be reopened when containers are disabled.
 */

async function openAndCloseTab(window, userContextId, url) {
  let tab = window.gBrowser.addTab(url, { userContextId });
  await promiseBrowserLoaded(tab.linkedBrowser, true, url);
  await TabStateFlusher.flush(tab.linkedBrowser);
  await promiseRemoveTab(tab);
}

async function openTab(window, userContextId, url) {
  let tab = window.gBrowser.addTab(url, { userContextId });
  await promiseBrowserLoaded(tab.linkedBrowser, true, url);
  await TabStateFlusher.flush(tab.linkedBrowser);
}

async function openWindow(url) {
  let win = await promiseNewWindowLoaded();
  let flags = Ci.nsIWebNavigation.LOAD_FLAGS_REPLACE_HISTORY;
  win.gBrowser.selectedBrowser.loadURIWithFlags(url, flags);
  await promiseBrowserLoaded(win.gBrowser.selectedBrowser, true, url);
  return win;
}

add_task(function* test_undoCloseById() {
  // Clear the lists of closed windows and tabs.
  forgetClosedWindows();
  while (SessionStore.getClosedTabCount(window)) {
    SessionStore.forgetClosedTab(window, 0);
  }

  // Open a new window.
  let win = yield openWindow("about:robots");

  // Open and close a tab.
  yield openAndCloseTab(win, 0, "about:mozilla");
  is(SessionStore.lastClosedObjectType, "tab", "The last closed object is a tab");

  // Open and close a container tab.
  yield openAndCloseTab(win, 3, "about:about");
  is(SessionStore.lastClosedObjectType, "tab", "The last closed object is a tab");

  // Restore the last closed tab.
  let tab = SessionStore.undoCloseTab(win, 0);
  yield promiseBrowserLoaded(tab.linkedBrowser);
  is(tab.linkedBrowser.currentURI.spec, "about:about", "The expected tab was re-opened");
  is(tab.getAttribute("usercontextid"), 3, "Expected the tab userContextId to match");

  // Open a few container tabs.
  yield openTab(win, 1, "about:config");
  yield openTab(win, 1, "about:preferences");
  yield openTab(win, 2, "about:support");

  // Let's simulate the disabling of containers.
  ContextualIdentityService.closeContainerTabs();
  ContextualIdentityService.disableContainers();
  yield TabStateFlusher.flushWindow(win);

  // Let's check we don't have container tab opened.
  for (let i = 0; i < win.gBrowser.tabContainer.childNodes.length; ++i) {
    let tab = win.gBrowser.tabContainer.childNodes[i];
    ok(!tab.hasAttribute("usercontextid"), "No userContextId for this tab");
  }

  // Restore the last closed tab (we don't want the container tabs).
  tab = SessionStore.undoCloseTab(win, 0);
  yield promiseBrowserLoaded(tab.linkedBrowser);
  is(tab.linkedBrowser.currentURI.spec, "about:mozilla", "The expected tab was re-opened");
  ok(!tab.hasAttribute("usercontextid"), "No userContextId for this tab");

  // Close the window again.
  yield BrowserTestUtils.closeWindow(win);
});
