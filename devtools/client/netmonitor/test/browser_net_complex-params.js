/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests whether complex request params and payload sent via POST are
 * displayed correctly.
 */

add_task(function* () {
  let [tab, , monitor] = yield initNetMonitor(PARAMS_URL);
  info("Starting test... ");

  let { document, L10N, EVENTS, Editor, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu, NetworkDetails } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;
  NetworkDetails._params.lazyEmpty = false;

  let wait = waitForNetworkEvents(monitor, 1, 6);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests();
  });
  yield wait;

  let onEvent = monitor.panelWin.once(EVENTS.REQUEST_POST_PARAMS_DISPLAYED);
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.getElementById("details-pane-toggle"));
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll("#details-pane tab")[2]);
  yield onEvent;
  yield testParamsTab1("a", '""', '{ "foo": "bar" }', '""');

  onEvent = monitor.panelWin.once(EVENTS.REQUEST_POST_PARAMS_DISPLAYED);
  RequestsMenu.selectedIndex = 1;
  yield onEvent;
  yield testParamsTab1("a", '"b"', '{ "foo": "bar" }', '""');

  onEvent = monitor.panelWin.once(EVENTS.REQUEST_POST_PARAMS_DISPLAYED);
  RequestsMenu.selectedIndex = 2;
  yield onEvent;
  yield testParamsTab1("a", '"b"', "foo", '"bar"');

  onEvent = monitor.panelWin.once(EVENTS.REQUEST_POST_PARAMS_DISPLAYED);
  RequestsMenu.selectedIndex = 3;
  yield onEvent;
  yield testParamsTab2("a", '""', '{ "foo": "bar" }', "js");

  onEvent = monitor.panelWin.once(EVENTS.REQUEST_POST_PARAMS_DISPLAYED);
  RequestsMenu.selectedIndex = 4;
  yield onEvent;
  yield testParamsTab2("a", '"b"', '{ "foo": "bar" }', "js");

  onEvent = monitor.panelWin.once(EVENTS.REQUEST_POST_PARAMS_DISPLAYED);
  RequestsMenu.selectedIndex = 5;
  yield onEvent;
  yield testParamsTab2("a", '"b"', "?foo=bar", "text");

  onEvent = monitor.panelWin.once(EVENTS.SIDEBAR_POPULATED);
  RequestsMenu.selectedIndex = 6;
  yield onEvent;
  yield testParamsTab3("a", '"b"');

  yield teardown(monitor);

  function testParamsTab1(queryStringParamName, queryStringParamValue,
                          formDataParamName, formDataParamValue) {
    let tabpanel = document.querySelectorAll("#details-pane tabpanel")[2];

    is(tabpanel.querySelectorAll(".variables-view-scope").length, 2,
      "The number of param scopes displayed in this tabpanel is incorrect.");
    is(tabpanel.querySelectorAll(".variable-or-property").length, 2,
      "The number of param values displayed in this tabpanel is incorrect.");
    is(tabpanel.querySelectorAll(".variables-view-empty-notice").length, 0,
      "The empty notice should not be displayed in this tabpanel.");

    is(tabpanel.querySelector("#request-params-box")
      .hasAttribute("hidden"), false,
      "The request params box should not be hidden.");
    is(tabpanel.querySelector("#request-post-data-textarea-box")
      .hasAttribute("hidden"), true,
      "The request post data textarea box should be hidden.");

    let paramsScope = tabpanel.querySelectorAll(".variables-view-scope")[0];
    let formDataScope = tabpanel.querySelectorAll(".variables-view-scope")[1];

    is(paramsScope.querySelector(".name").getAttribute("value"),
      L10N.getStr("paramsQueryString"),
      "The params scope doesn't have the correct title.");
    is(formDataScope.querySelector(".name").getAttribute("value"),
      L10N.getStr("paramsFormData"),
      "The form data scope doesn't have the correct title.");

    is(paramsScope.querySelectorAll(".variables-view-variable .name")[0]
      .getAttribute("value"),
      queryStringParamName,
      "The first query string param name was incorrect.");
    is(paramsScope.querySelectorAll(".variables-view-variable .value")[0]
      .getAttribute("value"),
      queryStringParamValue,
      "The first query string param value was incorrect.");

    is(formDataScope.querySelectorAll(".variables-view-variable .name")[0]
      .getAttribute("value"),
      formDataParamName,
      "The first form data param name was incorrect.");
    is(formDataScope.querySelectorAll(".variables-view-variable .value")[0]
      .getAttribute("value"),
      formDataParamValue,
      "The first form data param value was incorrect.");
  }

  function* testParamsTab2(queryStringParamName, queryStringParamValue,
                          requestPayload, editorMode) {
    let isJSON = editorMode == "js";
    let tabpanel = document.querySelectorAll("#details-pane tabpanel")[2];

    is(tabpanel.querySelectorAll(".variables-view-scope").length, 2,
      "The number of param scopes displayed in this tabpanel is incorrect.");
    is(tabpanel.querySelectorAll(".variable-or-property").length, isJSON ? 4 : 1,
      "The number of param values displayed in this tabpanel is incorrect.");
    is(tabpanel.querySelectorAll(".variables-view-empty-notice").length, 0,
      "The empty notice should not be displayed in this tabpanel.");

    is(tabpanel.querySelector("#request-params-box")
      .hasAttribute("hidden"), false,
      "The request params box should not be hidden.");
    is(tabpanel.querySelector("#request-post-data-textarea-box")
      .hasAttribute("hidden"), isJSON,
      "The request post data textarea box should be hidden.");

    let paramsScope = tabpanel.querySelectorAll(".variables-view-scope")[0];
    let payloadScope = tabpanel.querySelectorAll(".variables-view-scope")[1];

    is(paramsScope.querySelector(".name").getAttribute("value"),
      L10N.getStr("paramsQueryString"),
      "The params scope doesn't have the correct title.");
    is(payloadScope.querySelector(".name").getAttribute("value"),
      isJSON ? L10N.getStr("jsonScopeName") : L10N.getStr("paramsPostPayload"),
      "The request payload scope doesn't have the correct title.");

    is(paramsScope.querySelectorAll(".variables-view-variable .name")[0]
      .getAttribute("value"),
      queryStringParamName,
      "The first query string param name was incorrect.");
    is(paramsScope.querySelectorAll(".variables-view-variable .value")[0]
      .getAttribute("value"),
      queryStringParamValue,
      "The first query string param value was incorrect.");

    if (isJSON) {
      let requestPayloadObject = JSON.parse(requestPayload);
      let requestPairs = Object.keys(requestPayloadObject)
        .map(k => [k, requestPayloadObject[k]]);
      let displayedNames = payloadScope.querySelectorAll(
        ".variables-view-property.variable-or-property .name");
      let displayedValues = payloadScope.querySelectorAll(
        ".variables-view-property.variable-or-property .value");
      for (let i = 0; i < requestPairs.length; i++) {
        let [requestPayloadName, requestPayloadValue] = requestPairs[i];
        is(requestPayloadName, displayedNames[i].getAttribute("value"),
          "JSON property name " + i + " should be displayed correctly");
        is('"' + requestPayloadValue + '"', displayedValues[i].getAttribute("value"),
          "JSON property value " + i + " should be displayed correctly");
      }
    } else {
      let editor = yield NetMonitorView.editor("#request-post-data-textarea");
      is(editor.getText(), requestPayload,
        "The text shown in the source editor is incorrect.");
      is(editor.getMode(), Editor.modes[editorMode],
        "The mode active in the source editor is incorrect.");
    }
  }

  function testParamsTab3(queryStringParamName, queryStringParamValue) {
    let tabpanel = document.querySelectorAll("#details-pane tabpanel")[2];

    is(tabpanel.querySelectorAll(".variables-view-scope").length, 0,
      "The number of param scopes displayed in this tabpanel is incorrect.");
    is(tabpanel.querySelectorAll(".variable-or-property").length, 0,
      "The number of param values displayed in this tabpanel is incorrect.");
    is(tabpanel.querySelectorAll(".variables-view-empty-notice").length, 1,
      "The empty notice should be displayed in this tabpanel.");

    is(tabpanel.querySelector("#request-params-box")
      .hasAttribute("hidden"), false,
      "The request params box should not be hidden.");
    is(tabpanel.querySelector("#request-post-data-textarea-box")
      .hasAttribute("hidden"), true,
      "The request post data textarea box should be hidden.");
  }
});
