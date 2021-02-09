/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
/**
 * Test that the URL Preview can be copied
 */
add_task(async function() {
  const { tab, monitor } = await initNetMonitor(SIMPLE_URL, {
    requestCount: 1,
  });

  info("Starting the url preview copy test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  const wait = waitForNetworkEvents(monitor, 1);
  tab.linkedBrowser.reload();
  await wait;

  store.dispatch(Actions.toggleNetworkDetails());

  await waitForDOM(document, "#headers-panel .url-preview", 1);

  const urlPreview = document.querySelector("#headers-panel .url-preview");
  const urlRow = urlPreview.querySelector(".objectRow");

  /* Test for copy on the url */
  EventUtils.sendMouseEvent({ type: "contextmenu" }, urlRow);
  await waitForClipboardPromise(function setup() {
    getContextMenuItem(monitor, "properties-view-context-menu-copy").click();
  }, "http://example.com/browser/devtools/client/netmonitor/test/html_simple-test-page.html");

  ok(true, "The copy action put expected url string into clipboard");

  /* Test for copy all */
  EventUtils.sendMouseEvent({ type: "contextmenu" }, urlRow);
  const expected = JSON.stringify(
    {
      GET: {
        scheme: "http",
        host: "example.com",
        filename:
          "/browser/devtools/client/netmonitor/test/html_simple-test-page.html",
        remote: {
          Address: "127.0.0.1:8888",
        },
      },
    },
    null,
    "\t"
  );
  await waitForClipboardPromise(function setup() {
    getContextMenuItem(monitor, "properties-view-context-menu-copyall").click();
  }, expected);

  ok(true, "The copy all action put expected json data into clipboard");

  await teardown(monitor);
});

/**
 * Test that the Headers summary can be copied
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(SIMPLE_URL, {
    requestCount: 1,
  });

  info("Starting the headers summary copy test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  const wait = waitForNetworkEvents(monitor, 1);
  tab.linkedBrowser.reload();
  await wait;

  store.dispatch(Actions.toggleNetworkDetails());

  await waitForDOM(document, "#headers-panel .summary", 1);

  const headersSummary = document.querySelector("#headers-panel .summary");
  const httpSummaryValue = headersSummary.querySelectorAll(
    ".tabpanel-summary-value"
  )[1];

  /* Test for copy */
  EventUtils.sendMouseEvent({ type: "contextmenu" }, httpSummaryValue);
  await waitForClipboardPromise(function setup() {
    getContextMenuItem(monitor, "headers-panel-context-menu-copy").click();
  }, "Version: HTTP/1.1");

  ok(true, "The copy action put expected text into clipboard");

  /* Test for copy all */
  EventUtils.sendMouseEvent({ type: "contextmenu" }, httpSummaryValue);
  const expected = JSON.stringify(
    {
      Status: "200OK",
      Version: "HTTP/1.1",
      Transferred: "650 B (465 B size)",
    },
    null,
    "\t"
  );
  await waitForClipboardPromise(function setup() {
    getContextMenuItem(monitor, "headers-panel-context-menu-copyall").click();
  }, expected);

  ok(true, "The copy all action put expected json into clipboard");

  await teardown(monitor);
});

/**
 * Test if response JSON in PropertiesView can be copied
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(
    JSON_BASIC_URL + "?name=nogrip",
    { requestCount: 1 }
  );
  info("Starting the json in properties view copy test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  await performRequests(monitor, tab, 1);

  const onResponsePanelReady = waitForDOM(
    document,
    "#response-panel .treeTable"
  );
  store.dispatch(Actions.toggleNetworkDetails());
  clickOnSidebarTab(document, "response");
  await onResponsePanelReady;

  const responsePanel = document.querySelector("#response-panel");

  const objectRow = responsePanel.querySelectorAll(".objectRow")[0];
  const stringRow = responsePanel.querySelectorAll(".stringRow")[0];

  /* Test for copy an object */
  EventUtils.sendMouseEvent({ type: "contextmenu" }, objectRow);
  const expected = JSON.stringify({ obj: { type: "string" } }, null, "\t");
  await waitForClipboardPromise(function setup() {
    getContextMenuItem(monitor, "properties-view-context-menu-copy").click();
  }, expected);

  ok(true, "The copy action put expected json into clipboard");

  /* Test for copy all */
  EventUtils.sendMouseEvent({ type: "contextmenu" }, objectRow);
  await waitForClipboardPromise(function setup() {
    getContextMenuItem(monitor, "properties-view-context-menu-copyall").click();
  }, expected);

  ok(true, "The copy all action put expected json into clipboard");

  /* Test for copy a single row */
  EventUtils.sendMouseEvent({ type: "contextmenu" }, stringRow);
  await waitForClipboardPromise(function setup() {
    getContextMenuItem(monitor, "properties-view-context-menu-copy").click();
  }, "type: string");

  ok(true, "The copy action put expected text into clipboard");

  await teardown(monitor);
});

/**
 * Test if response/request Cookies in PropertiesView can be copied
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(SIMPLE_UNSORTED_COOKIES_SJS, {
    requestCount: 1,
  });
  info(
    "Starting the request/response  cookies in properties view copy test... "
  );

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  let wait = waitForNetworkEvents(monitor, 1);
  tab.linkedBrowser.reload();
  await wait;

  wait = waitForDOM(document, ".headers-overview");
  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]
  );
  await wait;

  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]
  );
  clickOnSidebarTab(document, "cookies");

  const cookiesPanel = document.querySelector("#cookies-panel");

  const objectRows = cookiesPanel.querySelectorAll(".objectRow");
  const stringRows = cookiesPanel.querySelectorAll(".stringRow");

  const expectedResponseCookies = [
    { bob: { httpOnly: true, value: "true" } },
    { foo: { httpOnly: true, value: "bar" } },
    { tom: { httpOnly: true, value: "cool" } },
  ];
  for (let i = 0; i < objectRows.length; i++) {
    const cur = objectRows[i];
    EventUtils.sendMouseEvent({ type: "contextmenu" }, cur);
    await waitForClipboardPromise(function setup() {
      getContextMenuItem(monitor, "properties-view-context-menu-copy").click();
    }, JSON.stringify(expectedResponseCookies[i], null, "\t"));
  }

  const expectedRequestCookies = ["bob: true", "foo: bar", "tom: cool"];
  for (let i = 0; i < expectedRequestCookies.length; i++) {
    const cur = stringRows[objectRows.length + i];
    EventUtils.sendMouseEvent({ type: "contextmenu" }, cur);
    await waitForClipboardPromise(function setup() {
      getContextMenuItem(monitor, "properties-view-context-menu-copy").click();
    }, expectedRequestCookies[i]);
  }

  await teardown(monitor);
});
