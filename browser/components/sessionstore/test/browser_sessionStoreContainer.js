/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
