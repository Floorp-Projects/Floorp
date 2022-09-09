/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if different response content types are handled correctly.
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(
    CONTENT_TYPE_WITHOUT_CACHE_URL,
    { requestCount: 1 }
  );
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const { L10N } = windowRequire("devtools/client/netmonitor/src/utils/l10n");
  const { getDisplayedRequests, getSortedRequests } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );

  store.dispatch(Actions.batchEnable(false));

  // Execute requests.
  await performRequests(monitor, tab, CONTENT_TYPE_WITHOUT_CACHE_REQUESTS);

  for (const requestItem of document.querySelectorAll(".request-list-item")) {
    const requestsListStatus = requestItem.querySelector(".status-code");
    requestItem.scrollIntoView();
    EventUtils.sendMouseEvent({ type: "mouseover" }, requestsListStatus);
    await waitUntil(() => requestsListStatus.title);
    await waitForDOMIfNeeded(requestItem, ".requests-list-timings-total");
  }

  verifyRequestItemTarget(
    document,
    getDisplayedRequests(store.getState()),
    getSortedRequests(store.getState())[0],
    "GET",
    CONTENT_TYPE_SJS + "?fmt=xml",
    {
      status: 200,
      statusText: "OK",
      type: "xml",
      fullMimeType: "text/xml; charset=utf-8",
      size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 42),
      time: true,
    }
  );
  verifyRequestItemTarget(
    document,
    getDisplayedRequests(store.getState()),
    getSortedRequests(store.getState())[1],
    "GET",
    CONTENT_TYPE_SJS + "?fmt=css",
    {
      status: 200,
      statusText: "OK",
      type: "css",
      fullMimeType: "text/css; charset=utf-8",
      size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 34),
      time: true,
    }
  );
  verifyRequestItemTarget(
    document,
    getDisplayedRequests(store.getState()),
    getSortedRequests(store.getState())[2],
    "GET",
    CONTENT_TYPE_SJS + "?fmt=js",
    {
      status: 200,
      statusText: "OK",
      type: "js",
      fullMimeType: "application/javascript; charset=utf-8",
      size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 34),
      time: true,
    }
  );
  verifyRequestItemTarget(
    document,
    getDisplayedRequests(store.getState()),
    getSortedRequests(store.getState())[3],
    "GET",
    CONTENT_TYPE_SJS + "?fmt=json",
    {
      status: 200,
      statusText: "OK",
      type: "json",
      fullMimeType: "application/json; charset=utf-8",
      size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 29),
      time: true,
    }
  );
  verifyRequestItemTarget(
    document,
    getDisplayedRequests(store.getState()),
    getSortedRequests(store.getState())[4],
    "GET",
    CONTENT_TYPE_SJS + "?fmt=bogus",
    {
      status: 404,
      statusText: "Not Found",
      type: "html",
      fullMimeType: "text/html; charset=utf-8",
      size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 24),
      time: true,
    }
  );
  verifyRequestItemTarget(
    document,
    getDisplayedRequests(store.getState()),
    getSortedRequests(store.getState())[5],
    "GET",
    TEST_IMAGE,
    {
      fuzzyUrl: true,
      status: 200,
      statusText: "OK",
      type: "png",
      fullMimeType: "image/png",
      size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 580),
      time: true,
    }
  );
  verifyRequestItemTarget(
    document,
    getDisplayedRequests(store.getState()),
    getSortedRequests(store.getState())[6],
    "GET",
    CONTENT_TYPE_SJS + "?fmt=gzip",
    {
      status: 200,
      statusText: "OK",
      type: "plain",
      fullMimeType: "text/plain",
      transferred: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 324),
      size: L10N.getFormatStrWithNumbers("networkMenu.size.kB", 10.99),
      time: true,
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

  await selectIndexAndWaitForHtmlView(4);
  await testResponseTab("html");

  await selectIndexAndWaitForImageView(5);
  await testResponseTab("png");

  await selectIndexAndWaitForSourceEditor(monitor, 6);
  await testResponseTab("gzip");

  await teardown(monitor);

  function testResponseTab(type) {
    const tabpanel = document.querySelector("#response-panel");

    function checkVisibility(box) {
      is(
        tabpanel.querySelector(".response-error-header") === null,
        true,
        "The response error header doesn't display"
      );
      const jsonView = tabpanel.querySelector(".data-label") || {};
      is(
        jsonView.textContent !== L10N.getStr("jsonScopeName"),
        box != "json",
        "The response json view doesn't display"
      );
      is(
        tabpanel.querySelector(".CodeMirror-code") === null,
        box !== "textarea",
        "The response editor doesn't display"
      );
      is(
        tabpanel.querySelector(".response-image-box") === null,
        box != "image",
        "The response image view doesn't display"
      );
    }

    switch (type) {
      case "xml": {
        checkVisibility("textarea");

        const text = getCodeMirrorValue(monitor);

        is(
          text,
          "<label value='greeting'>Hello XML!</label>",
          "The text shown in the source editor is incorrect for the xml request."
        );
        break;
      }
      case "css": {
        checkVisibility("textarea");

        const text = getCodeMirrorValue(monitor);

        is(
          text,
          "body:pre { content: 'Hello CSS!' }",
          "The text shown in the source editor is incorrect for the css request."
        );
        break;
      }
      case "js": {
        checkVisibility("textarea");

        const text = getCodeMirrorValue(monitor);

        is(
          text,
          "function() { return 'Hello JS!'; }",
          "The text shown in the source editor is incorrect for the js request."
        );
        break;
      }
      case "json": {
        checkVisibility("json");

        is(
          tabpanel.querySelectorAll(".raw-data-toggle").length,
          1,
          "The response payload toggle should be displayed in this tabpanel."
        );
        is(
          tabpanel.querySelectorAll(".empty-notice").length,
          0,
          "The empty notice should not be displayed in this tabpanel."
        );

        is(
          tabpanel.querySelector(".data-label").textContent,
          L10N.getStr("jsonScopeName"),
          "The json view section doesn't have the correct title."
        );

        const labels = tabpanel.querySelectorAll(
          "tr .treeLabelCell .treeLabel"
        );
        const values = tabpanel.querySelectorAll(
          "tr .treeValueCell .objectBox"
        );

        is(
          labels[0].textContent,
          "greeting",
          "The first json property name was incorrect."
        );
        is(
          values[0].textContent,
          `"Hello JSON!"`,
          "The first json property value was incorrect."
        );
        break;
      }
      case "html": {
        checkVisibility("html");

        const text = document.querySelector(".html-preview iframe").srcdoc;
        is(
          text,
          "<blink>Not Found</blink>",
          "The text shown in the iframe is incorrect for the html request."
        );
        break;
      }
      case "png": {
        checkVisibility("image");

        const [name, dimensions, mime] = tabpanel.querySelectorAll(
          ".response-image-box .tabpanel-summary-value"
        );

        is(
          name.textContent,
          "test-image.png",
          "The image name info isn't correct."
        );
        is(mime.textContent, "image/png", "The image mime info isn't correct.");
        is(
          dimensions.textContent,
          "16" + " \u00D7 " + "16",
          "The image dimensions info isn't correct."
        );
        break;
      }
      case "gzip": {
        checkVisibility("textarea");

        const text = getCodeMirrorValue(monitor);

        is(
          text,
          new Array(1000).join("Hello gzip!"),
          "The text shown in the source editor is incorrect for the gzip request."
        );
        break;
      }
    }
  }

  async function selectIndexAndWaitForHtmlView(index) {
    const onResponseContent = monitor.panelWin.api.once(
      TEST_EVENTS.RECEIVED_RESPONSE_CONTENT
    );
    const tabpanel = document.querySelector("#response-panel");
    const waitDOM = waitForDOM(tabpanel, ".html-preview");
    store.dispatch(Actions.selectRequestByIndex(index));
    await waitDOM;
    await onResponseContent;
  }

  async function selectIndexAndWaitForJSONView(index) {
    const onResponseContent = monitor.panelWin.api.once(
      TEST_EVENTS.RECEIVED_RESPONSE_CONTENT
    );
    const tabpanel = document.querySelector("#response-panel");
    const waitDOM = waitForDOM(tabpanel, ".treeTable");
    store.dispatch(Actions.selectRequestByIndex(index));
    await waitDOM;
    await onResponseContent;

    // Waiting for RECEIVED_RESPONSE_CONTENT isn't enough.
    // DOM may not be fully updated yet and checkVisibility(json) may still fail.
    await waitForTick();
  }

  async function selectIndexAndWaitForImageView(index) {
    const onResponseContent = monitor.panelWin.api.once(
      TEST_EVENTS.RECEIVED_RESPONSE_CONTENT
    );
    const tabpanel = document.querySelector("#response-panel");
    const waitDOM = waitForDOM(tabpanel, ".response-image");
    store.dispatch(Actions.selectRequestByIndex(index));
    const [imageNode] = await waitDOM;
    await once(imageNode, "load");
    await onResponseContent;
  }
});
