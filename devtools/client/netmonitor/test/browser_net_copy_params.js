/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests whether copying a request item's parameters works.
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(PARAMS_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  // Execute requests.
  await performRequests(monitor, tab, 7);

  testCopyUrlParamsHidden(0, false);
  await testCopyUrlParams(0, "a");
  testCopyPostDataHidden(0, false);
  await testCopyPostData(0, '{ "foo": "bar" }');

  testCopyUrlParamsHidden(1, false);
  await testCopyUrlParams(1, "a=b");
  testCopyPostDataHidden(1, false);
  await testCopyPostData(1, '{ "foo": "bar" }');

  testCopyUrlParamsHidden(2, false);
  await testCopyUrlParams(2, "a=b");
  testCopyPostDataHidden(2, false);
  await testCopyPostData(2, "foo=bar");

  testCopyUrlParamsHidden(3, false);
  await testCopyUrlParams(3, "a");
  testCopyPostDataHidden(3, false);
  await testCopyPostData(3, '{ "foo": "bar" }');

  testCopyUrlParamsHidden(4, false);
  await testCopyUrlParams(4, "a=b");
  testCopyPostDataHidden(4, false);
  await testCopyPostData(4, '{ "foo": "bar" }');

  testCopyUrlParamsHidden(5, false);
  await testCopyUrlParams(5, "a=b");
  testCopyPostDataHidden(5, false);
  await testCopyPostData(5, "?foo=bar");
  testCopyRequestDataLabel(5, "POST");

  testCopyUrlParamsHidden(6, true);
  testCopyPostDataHidden(6, true);

  testCopyPostDataHidden(7, false);
  testCopyRequestDataLabel(7, "PATCH");

  testCopyPostDataHidden(8, false);
  testCopyRequestDataLabel(8, "PUT");

  return teardown(monitor);

  function testCopyUrlParamsHidden(index, hidden) {
    EventUtils.sendMouseEvent(
      { type: "mousedown" },
      document.querySelectorAll(".request-list-item")[index]
    );
    EventUtils.sendMouseEvent(
      { type: "contextmenu" },
      document.querySelectorAll(".request-list-item")[index]
    );
    const copyUrlParamsNode = getContextMenuItem(
      monitor,
      "request-list-context-copy-url-params"
    );
    is(
      !!copyUrlParamsNode,
      !hidden,
      'The "Copy URL Parameters" context menu item should' +
        (hidden ? " " : " not ") +
        "be hidden."
    );
  }

  async function testCopyUrlParams(index, queryString) {
    EventUtils.sendMouseEvent(
      { type: "mousedown" },
      document.querySelectorAll(".request-list-item")[index]
    );
    EventUtils.sendMouseEvent(
      { type: "contextmenu" },
      document.querySelectorAll(".request-list-item")[index]
    );
    await waitForClipboardPromise(function setup() {
      getContextMenuItem(
        monitor,
        "request-list-context-copy-url-params"
      ).click();
    }, queryString);
    ok(true, "The url query string copied from the selected item is correct.");
  }

  function testCopyPostDataHidden(index, hidden) {
    EventUtils.sendMouseEvent(
      { type: "mousedown" },
      document.querySelectorAll(".request-list-item")[index]
    );
    EventUtils.sendMouseEvent(
      { type: "contextmenu" },
      document.querySelectorAll(".request-list-item")[index]
    );
    const copyPostDataNode = getContextMenuItem(
      monitor,
      "request-list-context-copy-post-data"
    );
    is(
      !!copyPostDataNode,
      !hidden,
      'The "Copy POST Data" context menu item should' +
        (hidden ? " " : " not ") +
        "be hidden."
    );
  }

  function testCopyRequestDataLabel(index, method) {
    EventUtils.sendMouseEvent(
      { type: "mousedown" },
      document.querySelectorAll(".request-list-item")[index]
    );
    EventUtils.sendMouseEvent(
      { type: "contextmenu" },
      document.querySelectorAll(".request-list-item")[index]
    );
    const copyPostDataNode = getContextMenuItem(
      monitor,
      "request-list-context-copy-post-data"
    );
    is(
      copyPostDataNode.attributes.label.value,
      "Copy " + method + " Data",
      'The "Copy Data" context menu item should have label - Copy ' +
        method +
        " Data"
    );
  }

  async function testCopyPostData(index, postData) {
    // Wait for formDataSections and requestPostData state are ready in redux store
    // since copyPostData API needs to read these state.
    await waitUntil(() => {
      const { requests } = store.getState().requests;
      const { formDataSections, requestPostData } = requests[index];
      return formDataSections && requestPostData;
    });
    EventUtils.sendMouseEvent(
      { type: "mousedown" },
      document.querySelectorAll(".request-list-item")[index]
    );
    EventUtils.sendMouseEvent(
      { type: "contextmenu" },
      document.querySelectorAll(".request-list-item")[index]
    );
    await waitForClipboardPromise(function setup() {
      getContextMenuItem(
        monitor,
        "request-list-context-copy-post-data"
      ).click();
    }, postData);
    ok(true, "The post data string copied from the selected item is correct.");
  }
});
