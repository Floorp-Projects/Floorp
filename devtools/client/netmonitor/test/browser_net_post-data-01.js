/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the POST requests display the correct information in the UI.
 */

add_task(async function() {
  const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");

  // Set a higher panel height in order to get full CodeMirror content
  Services.prefs.setIntPref("devtools.toolbox.footer.height", 600);

  const { tab, monitor } = await initNetMonitor(POST_DATA_URL);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const {
    getDisplayedRequests,
    getSortedRequests,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");

  store.dispatch(Actions.batchEnable(false));

  // Execute requests.
  await performRequests(monitor, tab, 2);

  const requestItems = document.querySelectorAll(".request-list-item");
  for (const requestItem of requestItems) {
    requestItem.scrollIntoView();
    const requestsListStatus = requestItem.querySelector(".status-code");
    EventUtils.sendMouseEvent({ type: "mouseover" }, requestsListStatus);
    await waitUntil(() => requestsListStatus.title);
  }

  verifyRequestItemTarget(
    document,
    getDisplayedRequests(store.getState()),
    getSortedRequests(store.getState()).get(0),
    "POST",
    SIMPLE_SJS + "?foo=bar&baz=42&type=urlencoded",
    {
      status: 200,
      statusText: "Och Aye",
      type: "plain",
      fullMimeType: "text/plain; charset=utf-8",
      size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 12),
      time: true
    }
  );
  verifyRequestItemTarget(
    document,
    getDisplayedRequests(store.getState()),
    getSortedRequests(store.getState()).get(1),
    "POST",
    SIMPLE_SJS + "?foo=bar&baz=42&type=multipart",
    {
      status: 200,
      statusText: "Och Aye",
      type: "plain",
      fullMimeType: "text/plain; charset=utf-8",
      size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 12),
      time: true
    }
  );

  // Wait for all tree sections updated by react
  wait = waitForDOM(document, "#params-panel .tree-section", 2);
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]);
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#params-tab"));
  await wait;
  await testParamsTab("urlencoded");

  // Wait for all tree sections and editor updated by react
  const waitForSections = waitForDOM(document, "#params-panel .tree-section", 2);
  const waitForSourceEditor = waitForDOM(document, "#params-panel .CodeMirror-code");
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[1]);
  await Promise.all([waitForSections, waitForSourceEditor]);
  await testParamsTab("multipart");

  return teardown(monitor);

  function testParamsTab(type) {
    const tabpanel = document.querySelector("#params-panel");

    function checkVisibility(box) {
      is(!tabpanel.querySelector(".treeTable"), !box.includes("params"),
        "The request params doesn't have the indended visibility.");
      is(tabpanel.querySelector(".CodeMirror-code") === null,
        !box.includes("editor"),
        "The request post data doesn't have the indended visibility.");
    }

    is(tabpanel.querySelectorAll(".tree-section").length, 2,
      "There should be 2 tree sections displayed in this tabpanel.");
    is(tabpanel.querySelectorAll(".empty-notice").length, 0,
      "The empty notice should not be displayed in this tabpanel.");

    const treeSections = tabpanel.querySelectorAll(".tree-section");

    is(treeSections[0].querySelector(".treeLabel").textContent,
      L10N.getStr("paramsQueryString"),
      "The query section doesn't have the correct title.");

    is(treeSections[1].querySelector(".treeLabel").textContent,
      L10N.getStr(type == "urlencoded" ? "paramsFormData" : "paramsPostPayload"),
      "The post section doesn't have the correct title.");

    const labels = tabpanel
      .querySelectorAll("tr:not(.tree-section) .treeLabelCell .treeLabel");
    const values = tabpanel
      .querySelectorAll("tr:not(.tree-section) .treeValueCell .objectBox");

    is(labels[0].textContent, "baz", "The first query param name was incorrect.");
    is(values[0].textContent, "42", "The first query param value was incorrect.");
    is(labels[1].textContent, "foo", "The second query param name was incorrect.");
    is(values[1].textContent, "bar", "The second query param value was incorrect.");
    is(labels[2].textContent, "type", "The third query param name was incorrect.");
    is(values[2].textContent, type, "The third query param value was incorrect.");

    if (type == "urlencoded") {
      checkVisibility("params");
      is(labels.length, 5, "There should be 5 param values displayed in this tabpanel.");
      is(labels[3].textContent, "baz", "The first post param name was incorrect.");
      is(values[3].textContent, "123", "The first post param value was incorrect.");
      is(labels[4].textContent, "foo", "The second post param name was incorrect.");
      is(values[4].textContent, "bar", "The second post param value was incorrect.");
    } else {
      checkVisibility("params editor");

      is(labels.length, 3, "There should be 3 param values displayed in this tabpanel.");

      // Collect code lines and combine into one text for checking
      let text = "";
      const lines = [...document.querySelectorAll(".CodeMirror-line")];

      lines.forEach((line) => {
        text += line.textContent + "\n";
      });

      ok(text.includes("Content-Disposition: form-data; name=\"text\""),
        "The text shown in the source editor is incorrect (1.1).");
      ok(text.includes("Content-Disposition: form-data; name=\"email\""),
        "The text shown in the source editor is incorrect (2.1).");
      ok(text.includes("Content-Disposition: form-data; name=\"range\""),
        "The text shown in the source editor is incorrect (3.1).");
      ok(text.includes("Content-Disposition: form-data; name=\"Custom field\""),
        "The text shown in the source editor is incorrect (4.1).");
      ok(text.includes("Some text..."),
        "The text shown in the source editor is incorrect (2.2).");
      ok(text.includes("42"),
        "The text shown in the source editor is incorrect (3.2).");
      ok(text.includes("Extra data"),
        "The text shown in the source editor is incorrect (4.2).");
    }
  }
});
