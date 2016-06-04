/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

requestLongerTimeout(3);

add_task(function* () {
  for (let i = 0; i < 3; ++i) {
    let tab = gBrowser.addTab("http://example.com/", { userContextId: i });
    let browser = tab.linkedBrowser;

    yield promiseBrowserLoaded(browser);

    let tab2 = gBrowser.duplicateTab(tab);
    Assert.equal(tab2.getAttribute("usercontextid"), i);
    let browser2 = tab2.linkedBrowser;
    yield promiseTabRestored(tab2)

    yield ContentTask.spawn(browser2, { expectedId: i }, function* (args) {
      let loadContext = docShell.QueryInterface(Ci.nsILoadContext);
      Assert.equal(loadContext.originAttributes.userContextId,
        args.expectedId, "The docShell has the correct userContextId");
    });

    yield promiseRemoveTab(tab);
    yield promiseRemoveTab(tab2);
  }
});

add_task(function* () {
  let tab = gBrowser.addTab("http://example.com/", { userContextId: 1 });
  let browser = tab.linkedBrowser;

  yield promiseBrowserLoaded(browser);

  gBrowser.selectedTab = tab;

  let tab2 = gBrowser.duplicateTab(tab);
  let browser2 = tab2.linkedBrowser;
  yield promiseTabRestored(tab2)

  yield ContentTask.spawn(browser2, { expectedId: 1 }, function* (args) {
    Assert.equal(docShell.getOriginAttributes().userContextId,
                 args.expectedId,
                 "The docShell has the correct userContextId");
  });

  yield promiseRemoveTab(tab);
  yield promiseRemoveTab(tab2);
});

add_task(function* () {
  let tab = gBrowser.addTab("http://example.com/", { userContextId: 1 });
  let browser = tab.linkedBrowser;

  yield promiseBrowserLoaded(browser);

  gBrowser.removeTab(tab);

  let tab2 = ss.undoCloseTab(window, 0);
  Assert.equal(tab2.getAttribute("usercontextid"), 1);
  yield promiseTabRestored(tab2);
  yield ContentTask.spawn(tab2.linkedBrowser, { expectedId: 1 }, function* (args) {
    Assert.equal(docShell.getOriginAttributes().userContextId,
                 args.expectedId,
                 "The docShell has the correct userContextId");
  });

  yield promiseRemoveTab(tab2);
});

add_task(function* () {
  let win = window.openDialog(location, "_blank", "chrome,all,dialog=no");
  yield promiseWindowLoaded(win);

  // Create 4 tabs with different userContextId.
  for (let userContextId = 1; userContextId < 5; userContextId++) {
    let tab = win.gBrowser.addTab("http://example.com/", {userContextId});
    yield promiseBrowserLoaded(tab.linkedBrowser);
    yield TabStateFlusher.flush(tab.linkedBrowser);
  }

  // Move the default tab of window to the end.
  // We want the 1st tab to have non-default userContextId, so later when we
  // restore into win2 we can test restore into an existing tab with different
  // userContextId.
  win.gBrowser.moveTabTo(win.gBrowser.tabs[0], win.gBrowser.tabs.length - 1);

  let winState = JSON.parse(ss.getWindowState(win));

  for (let i = 0; i < 4; i++) {
    Assert.equal(winState.windows[0].tabs[i].userContextId, i + 1,
                 "1st Window: tabs[" + i + "].userContextId should exist.");
  }

  let win2 = window.openDialog(location, "_blank", "chrome,all,dialog=no");
  yield promiseWindowLoaded(win2);

  // Create tabs with different userContextId, but this time we create them with
  // fewer tabs and with different order with win.
  for (let userContextId = 3; userContextId > 0; userContextId--) {
    let tab = win2.gBrowser.addTab("http://example.com/", {userContextId});
    yield promiseBrowserLoaded(tab.linkedBrowser);
    yield TabStateFlusher.flush(tab.linkedBrowser);
  }

  ss.setWindowState(win2, JSON.stringify(winState), true);

  for (let i = 0; i < 4; i++) {
    let browser = win2.gBrowser.tabs[i].linkedBrowser;
    yield ContentTask.spawn(browser, { expectedId: i + 1 }, function* (args) {
      Assert.equal(docShell.getOriginAttributes().userContextId,
                   args.expectedId,
                   "The docShell has the correct userContextId");

      Assert.equal(content.document.nodePrincipal.originAttributes.userContextId,
                   args.expectedId,
                   "The document has the correct userContextId");
    });
  }

  // Test the last tab, which doesn't have userContextId.
  let browser = win2.gBrowser.tabs[4].linkedBrowser;
  yield ContentTask.spawn(browser, { expectedId: 0 }, function* (args) {
    Assert.equal(docShell.getOriginAttributes().userContextId,
                 args.expectedId,
                 "The docShell has the correct userContextId");

    Assert.equal(content.document.nodePrincipal.originAttributes.userContextId,
                 args.expectedId,
                 "The document has the correct userContextId");
  });

  yield BrowserTestUtils.closeWindow(win);
  yield BrowserTestUtils.closeWindow(win2);
});

add_task(function* () {
  let win = window.openDialog(location, "_blank", "chrome,all,dialog=no");
  yield promiseWindowLoaded(win);

  let tab = win.gBrowser.addTab("http://example.com/", { userContextId: 1 });
  yield promiseBrowserLoaded(tab.linkedBrowser);
  yield TabStateFlusher.flush(tab.linkedBrowser);

  // win should have 1 default tab, and 1 container tab.
  Assert.equal(win.gBrowser.tabs.length, 2, "win should have 2 tabs");

  let winState = JSON.parse(ss.getWindowState(win));

  for (let i = 0; i < 2; i++) {
    Assert.equal(winState.windows[0].tabs[i].userContextId, i,
                 "1st Window: tabs[" + i + "].userContextId should be " + i);
  }

  let win2 = window.openDialog(location, "_blank", "chrome,all,dialog=no");
  yield promiseWindowLoaded(win2);

  let tab2 = win2.gBrowser.addTab("http://example.com/", { userContextId : 1 });
  yield promiseBrowserLoaded(tab2.linkedBrowser);
  yield TabStateFlusher.flush(tab2.linkedBrowser);

  // Move the first normal tab to end, so the first tab of win2 will be a
  // container tab.
  win2.gBrowser.moveTabTo(win2.gBrowser.tabs[0], win2.gBrowser.tabs.length - 1);
  yield TabStateFlusher.flush(win2.gBrowser.tabs[0].linkedBrowser);

  ss.setWindowState(win2, JSON.stringify(winState), true);

  for (let i = 0; i < 2; i++) {
    let browser = win2.gBrowser.tabs[i].linkedBrowser;
    yield ContentTask.spawn(browser, { expectedId: i }, function* (args) {
      Assert.equal(docShell.getOriginAttributes().userContextId,
                   args.expectedId,
                   "The docShell has the correct userContextId");

      Assert.equal(content.document.nodePrincipal.originAttributes.userContextId,
                   args.expectedId,
                   "The document has the correct userContextId");
    });
  }

  yield BrowserTestUtils.closeWindow(win);
  yield BrowserTestUtils.closeWindow(win2);
});

