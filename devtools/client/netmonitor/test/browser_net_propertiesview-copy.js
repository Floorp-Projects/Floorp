/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test if response JSON in PropertiesView can be copied
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(JSON_BASIC_URL + "?name=nogrip");
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  await performRequests(monitor, tab, 1);

  const onResponsePanelReady = waitForDOM(document, "#response-panel .treeTable");
  store.dispatch(Actions.toggleNetworkDetails());
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#response-tab"));
  await onResponsePanelReady;

  const responsePanel = document.querySelector("#response-panel");

  const objectRow = responsePanel.querySelectorAll(".objectRow")[1];
  const stringRow = responsePanel.querySelectorAll(".stringRow")[0];

  /* Test for copy an object */
  EventUtils.sendMouseEvent({ type: "contextmenu" },
    objectRow);
  await waitForClipboardPromise(function setup() {
    getContextMenuItem(monitor, "properties-view-context-menu-copy").click();
  }, `{"obj":{"type":"string"}}`);

  /* Test for copy all */
  EventUtils.sendMouseEvent({ type: "contextmenu" },
    objectRow);
  await waitForClipboardPromise(function setup() {
    getContextMenuItem(monitor, "properties-view-context-menu-copyall").click();
  }, `{"JSON":{"obj":{"type":"string"}},` +
    `"Response payload":{"EDITOR_CONFIG":{"text":` +
    `"{\\"obj\\": {\\"type\\": \\"string\\" }}","mode":"application/json"}}}`);

  /* Test for copy a single row */
  EventUtils.sendMouseEvent({ type: "contextmenu" },
    stringRow);
  await waitForClipboardPromise(function setup() {
    getContextMenuItem(monitor, "properties-view-context-menu-copy").click();
  }, "type: string");

  await teardown(monitor);
});

/**
 * Test if response/request Cookies in PropertiesView can be copied
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(SIMPLE_UNSORTED_COOKIES_SJS);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  tab.linkedBrowser.reload();

  let wait = waitForNetworkEvents(monitor, 1);
  await wait;

  wait = waitForDOM(document, ".headers-overview");
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]);
  await wait;

  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]);
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#cookies-tab"));

  const cookiesPanel = document.querySelector("#cookies-panel");

  const objectRows = cookiesPanel.querySelectorAll(".objectRow:not(.tree-section)");
  const stringRows = cookiesPanel.querySelectorAll(".stringRow");

  const expectedResponseCookies = [
    { bob: { httpOnly: true, value: "true" } },
    { foo: { httpOnly: true, value: "bar"  } },
    { tom: { httpOnly: true, value: "cool" } }];
  for (let i = 0; i < objectRows.length; i++) {
    const cur = objectRows[i];
    EventUtils.sendMouseEvent({ type: "contextmenu" },
      cur);
    await waitForClipboardPromise(function setup() {
      getContextMenuItem(monitor, "properties-view-context-menu-copy").click();
    }, JSON.stringify(expectedResponseCookies[i]));
  }

  const expectedRequestCookies = ["bob: true", "foo: bar", "tom: cool"];
  for (let i = 0; i < expectedRequestCookies.length; i++) {
    const cur = stringRows[objectRows.length + i];
    EventUtils.sendMouseEvent({ type: "contextmenu" },
      cur);
    await waitForClipboardPromise(function setup() {
      getContextMenuItem(monitor, "properties-view-context-menu-copy").click();
    }, expectedRequestCookies[i]);
  }

  await teardown(monitor);
});
