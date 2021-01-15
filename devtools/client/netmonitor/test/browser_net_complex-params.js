/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests whether complex request params and payload sent via POST are
 * displayed correctly.
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(PARAMS_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const { L10N } = windowRequire("devtools/client/netmonitor/src/utils/l10n");

  store.dispatch(Actions.batchEnable(false));

  // Execute requests.
  await performRequests(monitor, tab, 12);

  let wait = waitForDOM(document, "#request-panel .accordion-item", 2);
  await EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]
  );
  await clickOnSidebarTab(document, "request");
  await wait;
  testParamsTab1('{ "foo": "bar" }', "");

  wait = waitForDOM(document, "#request-panel .accordion-item", 2);
  await EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[1]
  );
  await wait;
  testParamsTab1('{ "foo": "bar" }', "");

  wait = waitForDOM(document, "#request-panel .accordion-item", 2);
  await EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[2]
  );
  await wait;
  testParamsTab1("?foo", "bar");

  let waitRows, waitSourceEditor;
  waitRows = waitForDOM(document, "#request-panel tr.treeRow", 1);
  waitSourceEditor = waitForDOM(document, "#request-panel .CodeMirror-code");
  await EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[3]
  );
  await Promise.all([waitRows, waitSourceEditor]);
  testParamsTab2('{ "foo": "bar" }', "js");

  waitRows = waitForDOM(document, "#request-panel tr.treeRow", 1);
  waitSourceEditor = waitForDOM(document, "#request-panel .CodeMirror-code");
  await EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[4]
  );
  await Promise.all([waitRows, waitSourceEditor]);
  testParamsTab2('{ "foo": "bar" }', "js");

  // Wait for all accordion items and editor updated by react
  const waitAccordionItems = waitForDOM(
    document,
    "#request-panel .accordion-item",
    1
  );
  waitSourceEditor = waitForDOM(document, "#request-panel .CodeMirror-code");
  await EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[5]
  );
  await Promise.all([waitAccordionItems, waitSourceEditor]);
  testParamsTab2("?foo=bar", "text");

  await EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[6]
  );
  testParamsTab3();

  wait = waitForDOM(document, "#request-panel .accordion-item", 2);
  await EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[7]
  );
  await wait;
  testParamsTab1('{ "foo": "bar" }', "");

  wait = waitForDOM(document, "#request-panel .accordion-item", 2);
  await EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[8]
  );
  await wait;
  testParamsTab1('{ "foo": "bar" }', "");

  await teardown(monitor);

  function testParamsTab1(formDataParamName, formDataParamValue) {
    const tabpanel = document.querySelector("#request-panel");

    is(
      tabpanel.querySelectorAll(".accordion-item").length,
      2,
      "The number of param accordion items displayed in this tabpanel is incorrect."
    );
    is(
      tabpanel.querySelectorAll("tr.treeRow").length,
      1,
      "The number of param rows displayed in this tabpanel is incorrect."
    );
    is(
      tabpanel.querySelectorAll(".empty-notice").length,
      0,
      "The empty notice should not be displayed in this tabpanel."
    );

    ok(
      tabpanel.querySelector(".treeTable"),
      "The request params box should be displayed."
    );
    ok(
      tabpanel.querySelector(".CodeMirror-code") === null,
      "The request post data editor should not be displayed."
    );

    const accordionItems = tabpanel.querySelectorAll(".accordion-item");
    const labels = tabpanel.querySelectorAll("tr .treeLabelCell .treeLabel");
    const values = tabpanel.querySelectorAll("tr .treeValueCell .objectBox");

    is(
      accordionItems[0].querySelector(".accordion-header-label").textContent,
      L10N.getStr("paramsFormData"),
      "The form data section doesn't have the correct title."
    );

    is(
      labels[0].textContent,
      formDataParamName,
      "The first form data param name was incorrect."
    );
    is(
      values[0].textContent,
      `"${formDataParamValue}"`,
      "The first form data param value was incorrect."
    );
  }

  function testParamsTab2(requestPayload, editorMode) {
    const isJSON = editorMode === "js";
    const tabpanel = document.querySelector("#request-panel");

    is(
      tabpanel.querySelectorAll(".accordion-item").length,
      isJSON ? 2 : 1,
      "The number of param accordion items displayed in this tabpanel is incorrect."
    );
    is(
      tabpanel.querySelectorAll("tr.treeRow").length,
      isJSON ? 1 : 0,
      "The number of param rows displayed in this tabpanel is incorrect."
    );
    is(
      tabpanel.querySelectorAll(".empty-notice").length,
      0,
      "The empty notice should not be displayed in this tabpanel."
    );

    ok(
      tabpanel.querySelector(".CodeMirror-code"),
      "The request post data editor should be displayed."
    );

    const accordionItems = tabpanel.querySelectorAll(".accordion-item");

    is(
      accordionItems[0].querySelector(".accordion-header-label").textContent,
      isJSON ? L10N.getStr("jsonScopeName") : L10N.getStr("paramsPostPayload"),
      "The post section doesn't have the correct title."
    );

    const labels = tabpanel.querySelectorAll("tr .treeLabelCell .treeLabel");
    const values = tabpanel.querySelectorAll("tr .treeValueCell .objectBox");

    ok(
      getCodeMirrorValue(monitor).includes(requestPayload),
      "The text shown in the source editor is incorrect."
    );

    if (isJSON) {
      is(
        accordionItems[1].querySelector(".accordion-header-label").textContent,
        L10N.getStr("paramsPostPayload"),
        "The post section doesn't have the correct title."
      );

      const requestPayloadObject = JSON.parse(requestPayload);
      const requestPairs = Object.keys(requestPayloadObject).map(k => [
        k,
        requestPayloadObject[k],
      ]);
      for (let i = 1; i < requestPairs.length; i++) {
        const [requestPayloadName, requestPayloadValue] = requestPairs[i];
        is(
          requestPayloadName,
          labels[i].textContent,
          "JSON property name " + i + " should be displayed correctly"
        );
        is(
          '"' + requestPayloadValue + '"',
          values[i].textContent,
          "JSON property value " + i + " should be displayed correctly"
        );
      }
    }
  }

  function testParamsTab3() {
    const tabpanel = document.querySelector("#request-panel");

    is(
      tabpanel.querySelectorAll(".accordion-item").length,
      0,
      "The number of param accordion items displayed in this tabpanel is incorrect."
    );
    is(
      tabpanel.querySelectorAll("tr.treeRow").length,
      0,
      "The number of param rows displayed in this tabpanel is incorrect."
    );
    is(
      tabpanel.querySelectorAll(".empty-notice").length,
      1,
      "The empty notice should be displayed in this tabpanel."
    );

    ok(
      !tabpanel.querySelector(".treeTable"),
      "The request params box should be hidden."
    );
    ok(
      !tabpanel.querySelector(".CodeMirror-code"),
      "The request post data editor should be hidden."
    );
  }
});
