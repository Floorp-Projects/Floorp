/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests whether complex request params and payload sent via POST are
 * displayed correctly.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(PARAMS_URL);
  info("Starting test... ");

  let { document, store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  let { L10N } = windowRequire("devtools/client/netmonitor/src/utils/l10n");

  store.dispatch(Actions.batchEnable(false));

  let wait = waitForNetworkEvents(monitor, 1, 6);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests();
  });
  yield wait;

  wait = waitForDOM(document, "#params-panel .tree-section", 2);
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]);
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#params-tab"));
  yield wait;
  testParamsTab1("a", "", '{ "foo": "bar" }', "");

  wait = waitForDOM(document, "#params-panel .tree-section", 2);
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[1]);
  yield wait;
  testParamsTab1("a", "b", '{ "foo": "bar" }', "");

  wait = waitForDOM(document, "#params-panel .tree-section", 2);
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[2]);
  yield wait;
  testParamsTab1("a", "b", "?foo", "bar");

  wait = waitForDOM(document, "#params-panel tr:not(.tree-section).treeRow", 2);
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[3]);
  yield wait;
  testParamsTab2("a", "", '{ "foo": "bar" }', "js");

  wait = waitForDOM(document, "#params-panel tr:not(.tree-section).treeRow", 2);
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[4]);
  yield wait;
  testParamsTab2("a", "b", '{ "foo": "bar" }', "js");

  // Wait for all tree sections and editor updated by react
  let waitSections = waitForDOM(document, "#params-panel .tree-section", 2);
  let waitSourceEditor = waitForDOM(document, "#params-panel .CodeMirror-code");
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[5]);
  yield Promise.all([waitSections, waitSourceEditor]);
  testParamsTab2("a", "b", "?foo=bar", "text");

  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[6]);
  testParamsTab3();

  yield teardown(monitor);

  function testParamsTab1(queryStringParamName, queryStringParamValue,
                          formDataParamName, formDataParamValue) {
    let tabpanel = document.querySelector("#params-panel");

    is(tabpanel.querySelectorAll(".tree-section").length, 2,
      "The number of param tree sections displayed in this tabpanel is incorrect.");
    is(tabpanel.querySelectorAll("tr:not(.tree-section).treeRow").length, 2,
      "The number of param rows displayed in this tabpanel is incorrect.");
    is(tabpanel.querySelectorAll(".empty-notice").length, 0,
      "The empty notice should not be displayed in this tabpanel.");

    ok(tabpanel.querySelector(".treeTable"),
      "The request params box should be displayed.");
    ok(tabpanel.querySelector(".CodeMirror-code") === null,
      "The request post data editor should not be displayed.");

    let treeSections = tabpanel.querySelectorAll(".tree-section");
    let labels = tabpanel
      .querySelectorAll("tr:not(.tree-section) .treeLabelCell .treeLabel");
    let values = tabpanel
      .querySelectorAll("tr:not(.tree-section) .treeValueCell .objectBox");

    is(treeSections[0].querySelector(".treeLabel").textContent,
      L10N.getStr("paramsQueryString"),
      "The params section doesn't have the correct title.");
    is(treeSections[1].querySelector(".treeLabel").textContent,
      L10N.getStr("paramsFormData"),
      "The form data section doesn't have the correct title.");

    is(labels[0].textContent, queryStringParamName,
      "The first query string param name was incorrect.");
    is(values[0].textContent, queryStringParamValue,
      "The first query string param value was incorrect.");

    is(labels[1].textContent, formDataParamName,
      "The first form data param name was incorrect.");
    is(values[1].textContent, formDataParamValue,
      "The first form data param value was incorrect.");
  }

  function testParamsTab2(queryStringParamName, queryStringParamValue,
                          requestPayload, editorMode) {
    let isJSON = editorMode === "js";
    let tabpanel = document.querySelector("#params-panel");

    is(tabpanel.querySelectorAll(".tree-section").length, 2,
      "The number of param tree sections displayed in this tabpanel is incorrect.");
    is(tabpanel.querySelectorAll("tr:not(.tree-section).treeRow").length, isJSON ? 2 : 1,
      "The number of param rows displayed in this tabpanel is incorrect.");
    is(tabpanel.querySelectorAll(".empty-notice").length, 0,
      "The empty notice should not be displayed in this tabpanel.");

    ok(tabpanel.querySelector(".treeTable"),
      "The request params box should be displayed.");
    is(tabpanel.querySelector(".CodeMirror-code") === null,
      isJSON,
      "The request post data editor should be not displayed.");

    let treeSections = tabpanel.querySelectorAll(".tree-section");

    is(treeSections[0].querySelector(".treeLabel").textContent,
      L10N.getStr("paramsQueryString"),
      "The query section doesn't have the correct title.");
    is(treeSections[1].querySelector(".treeLabel").textContent,
      isJSON ? L10N.getStr("jsonScopeName") : L10N.getStr("paramsPostPayload"),
      "The post section doesn't have the correct title.");

    let labels = tabpanel
      .querySelectorAll("tr:not(.tree-section) .treeLabelCell .treeLabel");
    let values = tabpanel
      .querySelectorAll("tr:not(.treeS-section) .treeValueCell .objectBox");

    is(labels[0].textContent, queryStringParamName,
      "The first query string param name was incorrect.");
    is(values[0].textContent, queryStringParamValue,
      "The first query string param value was incorrect.");

    if (isJSON) {
      let requestPayloadObject = JSON.parse(requestPayload);
      let requestPairs = Object.keys(requestPayloadObject)
        .map(k => [k, requestPayloadObject[k]]);
      for (let i = 1; i < requestPairs.length; i++) {
        let [requestPayloadName, requestPayloadValue] = requestPairs[i];
        is(requestPayloadName, labels[i].textContent,
          "JSON property name " + i + " should be displayed correctly");
        is('"' + requestPayloadValue + '"', values[i].textContent,
          "JSON property value " + i + " should be displayed correctly");
      }
    } else {
      ok(document.querySelector(".CodeMirror-code").textContent.includes(requestPayload),
        "The text shown in the source editor is incorrect.");
    }
  }

  function testParamsTab3() {
    let tabpanel = document.querySelector("#params-panel");

    is(tabpanel.querySelectorAll(".tree-section").length, 0,
      "The number of param tree sections displayed in this tabpanel is incorrect.");
    is(tabpanel.querySelectorAll("tr:not(.tree-section).treeRow").length, 0,
      "The number of param rows displayed in this tabpanel is incorrect.");
    is(tabpanel.querySelectorAll(".empty-notice").length, 1,
      "The empty notice should be displayed in this tabpanel.");

    ok(!tabpanel.querySelector(".treeTable"),
      "The request params box should be hidden.");
    ok(!tabpanel.querySelector(".CodeMirror-code"),
      "The request post data editor should be hidden.");
  }
});
