"use strict";

requestLongerTimeout(2);

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.ipc.processCount", 1]],
  });
});

function promiseTabsRestored(win, nExpected) {
  return new Promise(resolve => {
    let nReceived = 0;
    function handler(event) {
      if (++nReceived === nExpected) {
        win.gBrowser.tabContainer.removeEventListener(
          "SSTabRestored",
          handler,
          true
        );
        resolve();
      }
    }
    win.gBrowser.tabContainer.addEventListener("SSTabRestored", handler, true);
  });
}

add_task(async function() {
  let win = await BrowserTestUtils.openNewBrowserWindow();

  // Create 4 tabs with different userContextId.
  for (let userContextId = 1; userContextId < 5; userContextId++) {
    let tab = BrowserTestUtils.addTab(win.gBrowser, "http://example.com/", {
      userContextId,
    });
    await promiseBrowserLoaded(tab.linkedBrowser);
    await TabStateFlusher.flush(tab.linkedBrowser);
  }

  // Move the default tab of window to the end.
  // We want the 1st tab to have non-default userContextId, so later when we
  // restore into win2 we can test restore into an existing tab with different
  // userContextId.
  win.gBrowser.moveTabTo(win.gBrowser.tabs[0], win.gBrowser.tabs.length - 1);

  let winState = JSON.parse(ss.getWindowState(win));

  for (let i = 0; i < 4; i++) {
    Assert.equal(
      winState.windows[0].tabs[i].userContextId,
      i + 1,
      "1st Window: tabs[" + i + "].userContextId should exist."
    );
  }

  let win2 = await BrowserTestUtils.openNewBrowserWindow();

  // Create tabs with different userContextId, but this time we create them with
  // fewer tabs and with different order with win.
  for (let userContextId = 3; userContextId > 0; userContextId--) {
    let tab = BrowserTestUtils.addTab(win2.gBrowser, "http://example.com/", {
      userContextId,
    });
    await promiseBrowserLoaded(tab.linkedBrowser);
    await TabStateFlusher.flush(tab.linkedBrowser);
  }

  let tabsRestored = promiseTabsRestored(win2, 5);
  await setWindowState(win2, winState, true);
  await tabsRestored;

  for (let i = 0; i < 4; i++) {
    let browser = win2.gBrowser.tabs[i].linkedBrowser;
    await ContentTask.spawn(browser, { expectedId: i + 1 }, async function(
      args
    ) {
      Assert.equal(
        docShell.getOriginAttributes().userContextId,
        args.expectedId,
        "The docShell has the correct userContextId"
      );

      Assert.equal(
        content.document.nodePrincipal.originAttributes.userContextId,
        args.expectedId,
        "The document has the correct userContextId"
      );
    });
  }

  // Test the last tab, which doesn't have userContextId.
  let browser = win2.gBrowser.tabs[4].linkedBrowser;
  await SpecialPowers.spawn(browser, [{ expectedId: 0 }], async function(args) {
    Assert.equal(
      docShell.getOriginAttributes().userContextId,
      args.expectedId,
      "The docShell has the correct userContextId"
    );

    Assert.equal(
      content.document.nodePrincipal.originAttributes.userContextId,
      args.expectedId,
      "The document has the correct userContextId"
    );
  });

  await BrowserTestUtils.closeWindow(win);
  await BrowserTestUtils.closeWindow(win2);
});

add_task(async function() {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  await TabStateFlusher.flush(win.gBrowser.selectedBrowser);

  let tab = BrowserTestUtils.addTab(win.gBrowser, "http://example.com/", {
    userContextId: 1,
  });
  await promiseBrowserLoaded(tab.linkedBrowser);
  await TabStateFlusher.flush(tab.linkedBrowser);

  // win should have 1 default tab, and 1 container tab.
  Assert.equal(win.gBrowser.tabs.length, 2, "win should have 2 tabs");

  let winState = JSON.parse(ss.getWindowState(win));

  for (let i = 0; i < 2; i++) {
    Assert.equal(
      winState.windows[0].tabs[i].userContextId,
      i,
      "1st Window: tabs[" + i + "].userContextId should be " + i
    );
  }

  let win2 = await BrowserTestUtils.openNewBrowserWindow();

  let tab2 = BrowserTestUtils.addTab(win2.gBrowser, "http://example.com/", {
    userContextId: 1,
  });
  await promiseBrowserLoaded(tab2.linkedBrowser);
  await TabStateFlusher.flush(tab2.linkedBrowser);

  // Move the first normal tab to end, so the first tab of win2 will be a
  // container tab.
  win2.gBrowser.moveTabTo(win2.gBrowser.tabs[0], win2.gBrowser.tabs.length - 1);
  await TabStateFlusher.flush(win2.gBrowser.tabs[0].linkedBrowser);

  let tabsRestored = promiseTabsRestored(win2, 2);
  await setWindowState(win2, winState, true);
  await tabsRestored;

  for (let i = 0; i < 2; i++) {
    let browser = win2.gBrowser.tabs[i].linkedBrowser;
    await ContentTask.spawn(browser, { expectedId: i }, async function(args) {
      Assert.equal(
        docShell.getOriginAttributes().userContextId,
        args.expectedId,
        "The docShell has the correct userContextId"
      );

      Assert.equal(
        content.document.nodePrincipal.originAttributes.userContextId,
        args.expectedId,
        "The document has the correct userContextId"
      );
    });
  }

  await BrowserTestUtils.closeWindow(win);
  await BrowserTestUtils.closeWindow(win2);
});
