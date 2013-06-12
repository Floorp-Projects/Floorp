/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the POST requests display the correct information in the UI.
 */

function test() {
  initNetMonitor(POST_DATA_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    let { document, L10N, SourceEditor, NetMonitorView } = aMonitor.panelWin;
    let { RequestsMenu, NetworkDetails } = NetMonitorView;

    RequestsMenu.lazyUpdate = false;
    NetworkDetails._params.lazyEmpty = false;

    waitForNetworkEvents(aMonitor, 0, 2).then(() => {
      verifyRequestItemTarget(RequestsMenu.getItemAtIndex(0),
        "POST", SIMPLE_SJS + "?foo=bar&baz=42&type=urlencoded", {
          status: 200,
          statusText: "Och Aye",
          type: "plain",
          fullMimeType: "text/plain; charset=utf-8",
          size: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 0.01),
          time: true
        });
      verifyRequestItemTarget(RequestsMenu.getItemAtIndex(1),
        "POST", SIMPLE_SJS + "?foo=bar&baz=42&type=multipart", {
          status: 200,
          statusText: "Och Aye",
          type: "plain",
          fullMimeType: "text/plain; charset=utf-8",
          size: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 0.01),
          time: true
        });

      EventUtils.sendMouseEvent({ type: "mousedown" },
        document.getElementById("details-pane-toggle"));
      EventUtils.sendMouseEvent({ type: "mousedown" },
        document.querySelectorAll("#details-pane tab")[2]);

      testParamsTab("urlencoded")
        .then(() => {
          RequestsMenu.selectedIndex = 1;
          return testParamsTab("multipart");
        })
        .then(() => {
          return teardown(aMonitor);
        })
        .then(finish);

      function testParamsTab(aType) {
        let tab = document.querySelectorAll("#details-pane tab")[2];
        let tabpanel = document.querySelectorAll("#details-pane tabpanel")[2];

        is(tab.getAttribute("selected"), "true",
          "The params tab in the network details pane should be selected.");

        function checkVisibility(aBox) {
          is(tabpanel.querySelector("#request-params-box")
            .hasAttribute("hidden"), !aBox.contains("params"),
            "The request params box doesn't have the indended visibility.");

          is(tabpanel.querySelector("#request-post-data-textarea-box")
            .hasAttribute("hidden"), !aBox.contains("textarea"),
            "The request post data textarea box doesn't have the indended visibility.");
        }

        is(tabpanel.querySelectorAll(".variables-view-scope").length, 2,
          "There should be 2 param scopes displayed in this tabpanel.");
        is(tabpanel.querySelectorAll(".variables-view-empty-notice").length, 0,
          "The empty notice should not be displayed in this tabpanel.");

        let queryScope = tabpanel.querySelectorAll(".variables-view-scope")[0];
        let postScope = tabpanel.querySelectorAll(".variables-view-scope")[1];

        is(queryScope.querySelector(".name").getAttribute("value"),
          L10N.getStr("paramsQueryString"),
          "The query scope doesn't have the correct title.");

        is(postScope.querySelector(".name").getAttribute("value"),
          L10N.getStr(aType == "urlencoded" ? "paramsFormData" : "paramsPostPayload"),
          "The post scope doesn't have the correct title.");

        is(queryScope.querySelectorAll(".variables-view-variable .name")[0].getAttribute("value"),
          "foo", "The first query param name was incorrect.");
        is(queryScope.querySelectorAll(".variables-view-variable .value")[0].getAttribute("value"),
          "\"bar\"", "The first query param value was incorrect.");
        is(queryScope.querySelectorAll(".variables-view-variable .name")[1].getAttribute("value"),
          "baz", "The second query param name was incorrect.");
        is(queryScope.querySelectorAll(".variables-view-variable .value")[1].getAttribute("value"),
          "\"42\"", "The second query param value was incorrect.");
        is(queryScope.querySelectorAll(".variables-view-variable .name")[2].getAttribute("value"),
          "type", "The third query param name was incorrect.");
        is(queryScope.querySelectorAll(".variables-view-variable .value")[2].getAttribute("value"),
          "\"" + aType + "\"", "The third query param value was incorrect.");

        if (aType == "urlencoded") {
          checkVisibility("params");

          is(tabpanel.querySelectorAll(".variables-view-variable").length, 5,
            "There should be 5 param values displayed in this tabpanel.");
          is(queryScope.querySelectorAll(".variables-view-variable").length, 3,
            "There should be 3 param values displayed in the query scope.");
          is(postScope.querySelectorAll(".variables-view-variable").length, 2,
            "There should be 2 param values displayed in the post scope.");

          is(postScope.querySelectorAll(".variables-view-variable .name")[0].getAttribute("value"),
            "foo", "The first post param name was incorrect.");
          is(postScope.querySelectorAll(".variables-view-variable .value")[0].getAttribute("value"),
            "\"bar\"", "The first post param value was incorrect.");
          is(postScope.querySelectorAll(".variables-view-variable .name")[1].getAttribute("value"),
            "baz", "The second post param name was incorrect.");
          is(postScope.querySelectorAll(".variables-view-variable .value")[1].getAttribute("value"),
            "\"123\"", "The second post param value was incorrect.");

          return Promise.resolve();
        }
        else {
          checkVisibility("params textarea");

          is(tabpanel.querySelectorAll(".variables-view-variable").length, 3,
            "There should be 3 param values displayed in this tabpanel.");
          is(queryScope.querySelectorAll(".variables-view-variable").length, 3,
            "There should be 3 param values displayed in the query scope.");
          is(postScope.querySelectorAll(".variables-view-variable").length, 0,
            "There should be 0 param values displayed in the post scope.");

          return NetMonitorView.editor("#request-post-data-textarea").then((aEditor) => {
            ok(aEditor.getText().contains("Content-Disposition: form-data; name=\"text\""),
              "The text shown in the source editor is incorrect (1.1).");
            ok(aEditor.getText().contains("Content-Disposition: form-data; name=\"email\""),
              "The text shown in the source editor is incorrect (2.1).");
            ok(aEditor.getText().contains("Content-Disposition: form-data; name=\"range\""),
              "The text shown in the source editor is incorrect (3.1).");
            ok(aEditor.getText().contains("Content-Disposition: form-data; name=\"Custom field\""),
              "The text shown in the source editor is incorrect (4.1).");
            ok(aEditor.getText().contains("Some text..."),
              "The text shown in the source editor is incorrect (2.2).");
            ok(aEditor.getText().contains("42"),
              "The text shown in the source editor is incorrect (3.2).");
            ok(aEditor.getText().contains("Extra data"),
              "The text shown in the source editor is incorrect (4.2).");
            is(aEditor.getMode(), SourceEditor.MODES.TEXT,
              "The mode active in the source editor is incorrect.");
          });
        }
      }
    });

    aDebuggee.performRequests();
  });
}
