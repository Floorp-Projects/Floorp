/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if Open in new tab works.
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(OPEN_REQUEST_IN_TAB_URL);
  info("Starting test...");

  const { document, store, windowRequire } = monitor.panelWin;
  const contextMenuDoc = monitor.panelWin.parent.document;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
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
  await performRequest("POST", "application/x-www-form-urlencoded", "foo=bar&baz=42");
  newTab = await openLastRequestInTab();
  await checkTabResponse(newTab, "POST", "application/x-www-form-urlencoded",
    "foo=bar&amp;baz=42");

  // Open POST application/json request in new tab
  await performRequest("POST", "application/json", '{"foo":"bar"}');
  newTab = await openLastRequestInTab();
  await checkTabResponse(newTab, "POST", "application/json", '{"foo":"bar"}');

  await teardown(monitor);

  gBrowser.removeCurrentTab();

  async function openLastRequestInTab() {
    const wait = waitForDOM(contextMenuDoc, "#request-list-context-newtab");
    const requestItems = document.querySelectorAll(".request-list-item");
    const lastRequest = requestItems[requestItems.length - 1];
    EventUtils.sendMouseEvent({ type: "mousedown" }, lastRequest);
    EventUtils.sendMouseEvent({ type: "contextmenu" }, lastRequest);
    await wait;

    const onTabOpen = once(gBrowser.tabContainer, "TabOpen", false);
    monitor.panelWin.parent.document
      .querySelector("#request-list-context-newtab").click();
    await onTabOpen;
    info("A new tab has been opened");

    const awaitedTab = gBrowser.selectedTab;
    await BrowserTestUtils.browserLoaded(awaitedTab.linkedBrowser);
    info("The tab load completed");

    return awaitedTab;
  }

  async function performRequest(method, contentType, payload) {
    const wait = waitForNetworkEvents(monitor, 1);
    await ContentTask.spawn(tab.linkedBrowser, [method, contentType, payload],
      async function([method_, contentType_, payload_]) {
        content.wrappedJSObject.performRequest(method_, contentType_, payload_);
      }
    );
    await wait;
    info("Performed request to test server");
  }

  async function checkTabResponse(checkedTab, method, contentType, payload) {
    await ContentTask.spawn(checkedTab.linkedBrowser, [method, contentType, payload],
      async function([method_, contentType_, payload_]) {
        const { body } = content.wrappedJSObject.document;
        const expected = [method_, contentType_, payload_].join("\n");
        info("Response from the server:" + body.innerHTML.replace(/\n/g, "\\n"));
        ok(body.innerHTML.includes(expected),
          "Tab method and data match original request");
      }
    );
  }
});
