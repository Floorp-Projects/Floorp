/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if Open in new tab works.
 */

add_task(async function() {
  let { tab, monitor } = await initNetMonitor(OPEN_REQUEST_IN_TAB_URL);
  info("Starting test...");

  let { document, store, windowRequire } = monitor.panelWin;
  let contextMenuDoc = monitor.panelWin.parent.document;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  let newTab;

  store.dispatch(Actions.batchEnable(false));

  // Post data may be fetched by the Header panel,
  // so set the Timings panel as the new default.
  store.getState().ui.detailsPanelSelectedTab = "timings";

  // Open GET request in new tab
  await performRequest("GET");

  newTab = await openLastRequestInTab();
  await checkTabResponse(newTab, "GET");

  // Open POST request in new tab
  await performRequest("POST");
  newTab = await openLastRequestInTab();
  await checkTabResponse(newTab, "POST");

  await teardown(monitor);

  gBrowser.removeCurrentTab();

  async function openLastRequestInTab() {
    let wait = waitForDOM(contextMenuDoc, "#request-list-context-newtab");
    let requestItems = document.querySelectorAll(".request-list-item");
    let lastRequest = requestItems[requestItems.length - 1];
    EventUtils.sendMouseEvent({ type: "mousedown" }, lastRequest);
    EventUtils.sendMouseEvent({ type: "contextmenu" }, lastRequest);
    await wait;

    let onTabOpen = once(gBrowser.tabContainer, "TabOpen", false);
    monitor.panelWin.parent.document
      .querySelector("#request-list-context-newtab").click();
    await onTabOpen;
    ok(true, "A new tab has been opened");

    let awaitedTab = gBrowser.selectedTab;
    await BrowserTestUtils.browserLoaded(awaitedTab.linkedBrowser);
    info("The tab load completed");

    return awaitedTab;
  }

  async function performRequest(method) {
    let wait = waitForNetworkEvents(monitor, 1);
    await ContentTask.spawn(tab.linkedBrowser, method, async function(meth) {
      content.wrappedJSObject.performRequest(meth);
    });
    await wait;
  }

  async function checkTabResponse(checkedTab, method) {
    await ContentTask.spawn(checkedTab.linkedBrowser, method, async function(met) {
      let { body } = content.wrappedJSObject.document;
      let responseRE = RegExp(met + (met == "POST" ? "\n*\s*foo\=bar\&amp;baz\=42" : ""));
      ok(body.innerHTML.match(responseRE), "Tab method and data match original request");
    });
  }
});
