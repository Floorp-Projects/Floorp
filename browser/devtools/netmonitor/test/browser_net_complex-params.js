/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests whether complex request params and payload sent via POST are
 * displayed correctly.
 */

function test() {
  initNetMonitor(PARAMS_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    let { document, L10N, EVENTS, Editor, NetMonitorView } = aMonitor.panelWin;
    let { RequestsMenu, NetworkDetails } = NetMonitorView;

    RequestsMenu.lazyUpdate = false;
    NetworkDetails._params.lazyEmpty = false;

    Task.spawn(function () {
      yield waitForNetworkEvents(aMonitor, 0, 6);

      EventUtils.sendMouseEvent({ type: "mousedown" },
        document.getElementById("details-pane-toggle"));
      EventUtils.sendMouseEvent({ type: "mousedown" },
        document.querySelectorAll("#details-pane tab")[2]);

      yield waitFor(aMonitor.panelWin, EVENTS.REQUEST_POST_PARAMS_DISPLAYED);
      yield testParamsTab1('a', '""', '{ "foo": "bar" }', '""');

      RequestsMenu.selectedIndex = 1;
      yield waitFor(aMonitor.panelWin, EVENTS.REQUEST_POST_PARAMS_DISPLAYED);
      yield testParamsTab1('a', '"b"', '{ "foo": "bar" }', '""');

      RequestsMenu.selectedIndex = 2;
      yield waitFor(aMonitor.panelWin, EVENTS.REQUEST_POST_PARAMS_DISPLAYED);
      yield testParamsTab1('a', '"b"', 'foo', '"bar"');

      RequestsMenu.selectedIndex = 3;
      yield waitFor(aMonitor.panelWin, EVENTS.REQUEST_POST_PARAMS_DISPLAYED);
      yield testParamsTab2('a', '""', '{ "foo": "bar" }', "js");

      RequestsMenu.selectedIndex = 4;
      yield waitFor(aMonitor.panelWin, EVENTS.REQUEST_POST_PARAMS_DISPLAYED);
      yield testParamsTab2('a', '"b"', '{ "foo": "bar" }', "js");

      RequestsMenu.selectedIndex = 5;
      yield waitFor(aMonitor.panelWin, EVENTS.REQUEST_POST_PARAMS_DISPLAYED);
      yield testParamsTab2('a', '"b"', '?foo=bar', "text");

      yield teardown(aMonitor);
      finish();
    });

    function testParamsTab1(
      aQueryStringParamName, aQueryStringParamValue, aFormDataParamName, aFormDataParamValue)
    {
      let tab = document.querySelectorAll("#details-pane tab")[2];
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

      is(paramsScope.querySelectorAll(".variables-view-variable .name")[0].getAttribute("value"),
        aQueryStringParamName,
        "The first query string param name was incorrect.");
      is(paramsScope.querySelectorAll(".variables-view-variable .value")[0].getAttribute("value"),
        aQueryStringParamValue,
        "The first query string param value was incorrect.");

      is(formDataScope.querySelectorAll(".variables-view-variable .name")[0].getAttribute("value"),
        aFormDataParamName,
        "The first form data param name was incorrect.");
      is(formDataScope.querySelectorAll(".variables-view-variable .value")[0].getAttribute("value"),
        aFormDataParamValue,
        "The first form data param value was incorrect.");
    }

    function testParamsTab2(
      aQueryStringParamName, aQueryStringParamValue, aRequestPayload, aEditorMode)
    {
      let tab = document.querySelectorAll("#details-pane tab")[2];
      let tabpanel = document.querySelectorAll("#details-pane tabpanel")[2];

      is(tabpanel.querySelectorAll(".variables-view-scope").length, 2,
        "The number of param scopes displayed in this tabpanel is incorrect.");
      is(tabpanel.querySelectorAll(".variable-or-property").length, 1,
        "The number of param values displayed in this tabpanel is incorrect.");
      is(tabpanel.querySelectorAll(".variables-view-empty-notice").length, 0,
        "The empty notice should not be displayed in this tabpanel.");

      is(tabpanel.querySelector("#request-params-box")
        .hasAttribute("hidden"), false,
        "The request params box should not be hidden.");
      is(tabpanel.querySelector("#request-post-data-textarea-box")
        .hasAttribute("hidden"), false,
        "The request post data textarea box should not be hidden.");

      let paramsScope = tabpanel.querySelectorAll(".variables-view-scope")[0];
      let payloadScope = tabpanel.querySelectorAll(".variables-view-scope")[1];

      is(paramsScope.querySelector(".name").getAttribute("value"),
        L10N.getStr("paramsQueryString"),
        "The params scope doesn't have the correct title.");
      is(payloadScope.querySelector(".name").getAttribute("value"),
        L10N.getStr("paramsPostPayload"),
        "The request payload scope doesn't have the correct title.");

      is(paramsScope.querySelectorAll(".variables-view-variable .name")[0].getAttribute("value"),
        aQueryStringParamName,
        "The first query string param name was incorrect.");
      is(paramsScope.querySelectorAll(".variables-view-variable .value")[0].getAttribute("value"),
        aQueryStringParamValue,
        "The first query string param value was incorrect.");

      return NetMonitorView.editor("#request-post-data-textarea").then((aEditor) => {
        is(aEditor.getText(), aRequestPayload,
          "The text shown in the source editor is incorrect.");
        is(aEditor.getMode(), Editor.modes[aEditorMode],
          "The mode active in the source editor is incorrect.");
      });
    }

    aDebuggee.performRequests();
  });
}
