/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if different response content types are handled correctly.
 */

add_task(async function() {
  let { tab, monitor } = await initNetMonitor(CONTENT_TYPE_WITHOUT_CACHE_URL);
  info("Starting test... ");

  let { document, store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  let { L10N } = windowRequire("devtools/client/netmonitor/src/utils/l10n");
  let {
    getDisplayedRequests,
    getSortedRequests,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");

  store.dispatch(Actions.batchEnable(false));

  // Execute requests.
  await performRequests(monitor, tab, CONTENT_TYPE_WITHOUT_CACHE_REQUESTS);

  for (let requestItem of document.querySelectorAll(".request-list-item")) {
    let requestsListStatus = requestItem.querySelector(".status-code");
    requestItem.scrollIntoView();
    EventUtils.sendMouseEvent({ type: "mouseover" }, requestsListStatus);
    await waitUntil(() => requestsListStatus.title);
  }

  verifyRequestItemTarget(
    document,
    getDisplayedRequests(store.getState()),
    getSortedRequests(store.getState()).get(0),
    "GET",
    CONTENT_TYPE_SJS + "?fmt=xml",
    {
      status: 200,
      statusText: "OK",
      type: "xml",
      fullMimeType: "text/xml; charset=utf-8",
      size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 42),
      time: true
    }
  );
  verifyRequestItemTarget(
    document,
    getDisplayedRequests(store.getState()),
    getSortedRequests(store.getState()).get(1),
    "GET",
    CONTENT_TYPE_SJS + "?fmt=css",
    {
      status: 200,
      statusText: "OK",
      type: "css",
      fullMimeType: "text/css; charset=utf-8",
      size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 34),
      time: true
    }
  );
  verifyRequestItemTarget(
    document,
    getDisplayedRequests(store.getState()),
    getSortedRequests(store.getState()).get(2),
    "GET",
    CONTENT_TYPE_SJS + "?fmt=js",
    {
      status: 200,
      statusText: "OK",
      type: "js",
      fullMimeType: "application/javascript; charset=utf-8",
      size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 34),
      time: true
    }
  );
  verifyRequestItemTarget(
    document,
    getDisplayedRequests(store.getState()),
    getSortedRequests(store.getState()).get(3),
    "GET",
    CONTENT_TYPE_SJS + "?fmt=json",
    {
      status: 200,
      statusText: "OK",
      type: "json",
      fullMimeType: "application/json; charset=utf-8",
      size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 29),
      time: true
    }
  );
  verifyRequestItemTarget(
    document,
    getDisplayedRequests(store.getState()),
    getSortedRequests(store.getState()).get(4),
    "GET",
    CONTENT_TYPE_SJS + "?fmt=bogus",
    {
      status: 404,
      statusText: "Not Found",
      type: "html",
      fullMimeType: "text/html; charset=utf-8",
      size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 24),
      time: true
    }
  );
  verifyRequestItemTarget(
    document,
    getDisplayedRequests(store.getState()),
    getSortedRequests(store.getState()).get(5),
    "GET",
    TEST_IMAGE,
    {
      fuzzyUrl: true,
      status: 200,
      statusText: "OK",
      type: "png",
      fullMimeType: "image/png",
      size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 580),
      time: true
    }
  );
  verifyRequestItemTarget(
    document,
    getDisplayedRequests(store.getState()),
    getSortedRequests(store.getState()).get(6),
    "GET",
    CONTENT_TYPE_SJS + "?fmt=gzip",
    {
      status: 200,
      statusText: "OK",
      type: "plain",
      fullMimeType: "text/plain",
      transferred: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 324),
      size: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 10.73),
      time: true
    }
  );

  await selectIndexAndWaitForSourceEditor(monitor, 0);
  await testResponseTab("xml");

  await selectIndexAndWaitForSourceEditor(monitor, 1);
  await testResponseTab("css");

  await selectIndexAndWaitForSourceEditor(monitor, 2);
  await testResponseTab("js");

  await selectIndexAndWaitForJSONView(3);
  await testResponseTab("json");

  await selectIndexAndWaitForSourceEditor(monitor, 4);
  await testResponseTab("html");

  await selectIndexAndWaitForImageView(5);
  await testResponseTab("png");

  await selectIndexAndWaitForSourceEditor(monitor, 6);
  await testResponseTab("gzip");

  await teardown(monitor);

  function testResponseTab(type) {
    let tabpanel = document.querySelector("#response-panel");

    function checkVisibility(box) {
      is(tabpanel.querySelector(".response-error-header") === null,
        true,
        "The response error header doesn't display");
      let jsonView = tabpanel.querySelector(".tree-section .treeLabel") || {};
      is(jsonView.textContent !== L10N.getStr("jsonScopeName"),
        box != "json",
        "The response json view doesn't display");
      is(tabpanel.querySelector(".CodeMirror-code") === null,
        (box !== "textarea" && box !== "json"),
        "The response editor doesn't display");
      is(tabpanel.querySelector(".response-image-box") === null,
        box != "image",
        "The response image view doesn't display");
    }

    switch (type) {
      case "xml": {
        checkVisibility("textarea");

        let text = document.querySelector(".CodeMirror-line").textContent;

        is(text, "<label value='greeting'>Hello XML!</label>",
          "The text shown in the source editor is incorrect for the xml request.");
        break;
      }
      case "css": {
        checkVisibility("textarea");

        let text = document.querySelector(".CodeMirror-line").textContent;

        is(text, "body:pre { content: 'Hello CSS!' }",
          "The text shown in the source editor is incorrect for the css request.");
        break;
      }
      case "js": {
        checkVisibility("textarea");

        let text = document.querySelector(".CodeMirror-line").textContent;

        is(text, "function() { return 'Hello JS!'; }",
          "The text shown in the source editor is incorrect for the js request.");
        break;
      }
      case "json": {
        checkVisibility("json");

        is(tabpanel.querySelectorAll(".tree-section").length, 2,
          "There should be 2 tree sections displayed in this tabpanel.");
        is(tabpanel.querySelectorAll(".empty-notice").length, 0,
          "The empty notice should not be displayed in this tabpanel.");

        is(tabpanel.querySelector(".tree-section .treeLabel").textContent,
          L10N.getStr("jsonScopeName"),
          "The json view section doesn't have the correct title.");

        let labels = tabpanel
          .querySelectorAll("tr:not(.tree-section) .treeLabelCell .treeLabel");
        let values = tabpanel
          .querySelectorAll("tr:not(.tree-section) .treeValueCell .objectBox");

        is(labels[0].textContent, "greeting",
          "The first json property name was incorrect.");
        is(values[0].textContent,
          "Hello JSON!", "The first json property value was incorrect.");
        break;
      }
      case "html": {
        checkVisibility("textarea");

        let text = document.querySelector(".CodeMirror-line").textContent;

        is(text, "<blink>Not Found</blink>",
          "The text shown in the source editor is incorrect for the html request.");
        break;
      }
      case "png": {
        checkVisibility("image");

        let [name, dimensions, mime] = tabpanel
          .querySelectorAll(".response-image-box .tabpanel-summary-value");

        is(name.textContent, "test-image.png",
          "The image name info isn't correct.");
        is(mime.textContent, "image/png",
          "The image mime info isn't correct.");
        is(dimensions.textContent, "16" + " \u00D7 " + "16",
          "The image dimensions info isn't correct.");
        break;
      }
      case "gzip": {
        checkVisibility("textarea");

        let text = document.querySelector(".CodeMirror-line").textContent;

        is(text, new Array(1000).join("Hello gzip!"),
          "The text shown in the source editor is incorrect for the gzip request.");
        break;
      }
    }
  }

  async function selectIndexAndWaitForJSONView(index) {
    let onResponseContent = monitor.panelWin.api.once(EVENTS.RECEIVED_RESPONSE_CONTENT);
    let tabpanel = document.querySelector("#response-panel");
    let waitDOM = waitForDOM(tabpanel, ".treeTable");
    store.dispatch(Actions.selectRequestByIndex(index));
    await waitDOM;
    await onResponseContent;

    // Waiting for RECEIVED_RESPONSE_CONTENT isn't enough.
    // DOM may not be fully updated yet and checkVisibility(json) may still fail.
    await waitForTick();
  }

  async function selectIndexAndWaitForImageView(index) {
    let onResponseContent = monitor.panelWin.api.once(EVENTS.RECEIVED_RESPONSE_CONTENT);
    let tabpanel = document.querySelector("#response-panel");
    let waitDOM = waitForDOM(tabpanel, ".response-image");
    store.dispatch(Actions.selectRequestByIndex(index));
    let [imageNode] = await waitDOM;
    await once(imageNode, "load");
    await onResponseContent;
  }
});
