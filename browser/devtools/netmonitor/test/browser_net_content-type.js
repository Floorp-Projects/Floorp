/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if different response content types are handled correctly.
 */

function test() {
  initNetMonitor(CONTENT_TYPE_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    let { document, L10N, Editor, NetMonitorView } = aMonitor.panelWin;
    let { RequestsMenu } = NetMonitorView;

    RequestsMenu.lazyUpdate = false;

    waitForNetworkEvents(aMonitor, 6).then(() => {
      verifyRequestItemTarget(RequestsMenu.getItemAtIndex(0),
        "GET", CONTENT_TYPE_SJS + "?fmt=xml", {
          status: 200,
          statusText: "OK",
          type: "xml",
          fullMimeType: "text/xml; charset=utf-8",
          size: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 0.04),
          time: true
        });
      verifyRequestItemTarget(RequestsMenu.getItemAtIndex(1),
        "GET", CONTENT_TYPE_SJS + "?fmt=css", {
          status: 200,
          statusText: "OK",
          type: "css",
          fullMimeType: "text/css; charset=utf-8",
          size: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 0.03),
          time: true
        });
      verifyRequestItemTarget(RequestsMenu.getItemAtIndex(2),
        "GET", CONTENT_TYPE_SJS + "?fmt=js", {
          status: 200,
          statusText: "OK",
          type: "js",
          fullMimeType: "application/javascript; charset=utf-8",
          size: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 0.03),
          time: true
        });
      verifyRequestItemTarget(RequestsMenu.getItemAtIndex(3),
        "GET", CONTENT_TYPE_SJS + "?fmt=json", {
          status: 200,
          statusText: "OK",
          type: "json",
          fullMimeType: "application/json; charset=utf-8",
          size: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 0.02),
          time: true
        });
      verifyRequestItemTarget(RequestsMenu.getItemAtIndex(4),
        "GET", CONTENT_TYPE_SJS + "?fmt=bogus", {
          status: 404,
          statusText: "Not Found",
          type: "html",
          fullMimeType: "text/html; charset=utf-8",
          size: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 0.02),
          time: true
        });
      verifyRequestItemTarget(RequestsMenu.getItemAtIndex(5),
        "GET", TEST_IMAGE, {
          status: 200,
          statusText: "OK",
          type: "png",
          fullMimeType: "image/png",
          size: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 0.75),
          time: true
        });

      EventUtils.sendMouseEvent({ type: "mousedown" },
        document.getElementById("details-pane-toggle"));
      EventUtils.sendMouseEvent({ type: "mousedown" },
        document.querySelectorAll("#details-pane tab")[3]);

      Task.spawn(function*() {
        yield waitForResponseBodyDisplayed();
        yield testResponseTab("xml");
        RequestsMenu.selectedIndex = 1;
        yield waitForTabUpdated();
        yield testResponseTab("css");
        RequestsMenu.selectedIndex = 2;
        yield waitForTabUpdated();
        yield testResponseTab("js");
        RequestsMenu.selectedIndex = 3;
        yield waitForTabUpdated();
        yield testResponseTab("json");
        RequestsMenu.selectedIndex = 4;
        yield waitForTabUpdated();
        yield testResponseTab("html");
        RequestsMenu.selectedIndex = 5;
        yield waitForTabUpdated();
        yield testResponseTab("png");
        yield teardown(aMonitor);
        finish();
      });

      function testResponseTab(aType) {
        let tab = document.querySelectorAll("#details-pane tab")[3];
        let tabpanel = document.querySelectorAll("#details-pane tabpanel")[3];

        is(tab.getAttribute("selected"), "true",
          "The response tab in the network details pane should be selected.");

        function checkVisibility(aBox) {
          is(tabpanel.querySelector("#response-content-info-header")
            .hasAttribute("hidden"), true,
            "The response info header doesn't have the intended visibility.");
          is(tabpanel.querySelector("#response-content-json-box")
            .hasAttribute("hidden"), aBox != "json",
            "The response content json box doesn't have the intended visibility.");
          is(tabpanel.querySelector("#response-content-textarea-box")
            .hasAttribute("hidden"), aBox != "textarea",
            "The response content textarea box doesn't have the intended visibility.");
          is(tabpanel.querySelector("#response-content-image-box")
            .hasAttribute("hidden"), aBox != "image",
            "The response content image box doesn't have the intended visibility.");
        }

        switch (aType) {
          case "xml": {
            checkVisibility("textarea");

            return NetMonitorView.editor("#response-content-textarea").then((aEditor) => {
              is(aEditor.getText(), "<label value='greeting'>Hello XML!</label>",
                "The text shown in the source editor is incorrect for the xml request.");
              is(aEditor.getMode(), Editor.modes.html,
                "The mode active in the source editor is incorrect for the xml request.");
            });
          }
          case "css": {
            checkVisibility("textarea");

            return NetMonitorView.editor("#response-content-textarea").then((aEditor) => {
              is(aEditor.getText(), "body:pre { content: 'Hello CSS!' }",
                "The text shown in the source editor is incorrect for the xml request.");
              is(aEditor.getMode(), Editor.modes.css,
                "The mode active in the source editor is incorrect for the xml request.");
            });
          }
          case "js": {
            checkVisibility("textarea");

            return NetMonitorView.editor("#response-content-textarea").then((aEditor) => {
              is(aEditor.getText(), "function() { return 'Hello JS!'; }",
                "The text shown in the source editor is incorrect for the xml request.");
              is(aEditor.getMode(), Editor.modes.js,
                "The mode active in the source editor is incorrect for the xml request.");
            });
          }
          case "json": {
            checkVisibility("json");

            is(tabpanel.querySelectorAll(".variables-view-scope").length, 1,
              "There should be 1 json scope displayed in this tabpanel.");
            is(tabpanel.querySelectorAll(".variables-view-property").length, 2,
              "There should be 2 json properties displayed in this tabpanel.");
            is(tabpanel.querySelectorAll(".variables-view-empty-notice").length, 0,
              "The empty notice should not be displayed in this tabpanel.");

            let jsonScope = tabpanel.querySelectorAll(".variables-view-scope")[0];

            is(jsonScope.querySelector(".name").getAttribute("value"),
              L10N.getStr("jsonScopeName"),
              "The json scope doesn't have the correct title.");

            is(jsonScope.querySelectorAll(".variables-view-property .name")[0].getAttribute("value"),
              "greeting", "The first json property name was incorrect.");
            is(jsonScope.querySelectorAll(".variables-view-property .value")[0].getAttribute("value"),
              "\"Hello JSON!\"", "The first json property value was incorrect.");

            is(jsonScope.querySelectorAll(".variables-view-property .name")[1].getAttribute("value"),
              "__proto__", "The second json property name was incorrect.");
            is(jsonScope.querySelectorAll(".variables-view-property .value")[1].getAttribute("value"),
              "Object", "The second json property value was incorrect.");

            return promise.resolve();
          }
          case "html": {
            checkVisibility("textarea");

            return NetMonitorView.editor("#response-content-textarea").then((aEditor) => {
              is(aEditor.getText(), "<blink>Not Found</blink>",
                "The text shown in the source editor is incorrect for the xml request.");
              is(aEditor.getMode(), Editor.modes.html,
                "The mode active in the source editor is incorrect for the xml request.");
            });
          }
          case "png": {
            checkVisibility("image");

            let imageNode = tabpanel.querySelector("#response-content-image");
            let deferred = promise.defer();

            imageNode.addEventListener("load", function onLoad() {
              imageNode.removeEventListener("load", onLoad);

              is(tabpanel.querySelector("#response-content-image-name-value")
                .getAttribute("value"), "test-image.png",
                "The image name info isn't correct.");
              is(tabpanel.querySelector("#response-content-image-mime-value")
                .getAttribute("value"), "image/png",
                "The image mime info isn't correct.");
              is(tabpanel.querySelector("#response-content-image-encoding-value")
                .getAttribute("value"), "base64",
                "The image encoding info isn't correct.");
              is(tabpanel.querySelector("#response-content-image-dimensions-value")
                .getAttribute("value"), "16 x 16",
                "The image dimensions info isn't correct.");

              deferred.resolve();
            });

            return deferred.promise;
          }
        }
      }

      function waitForTabUpdated () {
        return waitFor(aMonitor.panelWin, aMonitor.panelWin.EVENTS.TAB_UPDATED);
      }

      function waitForResponseBodyDisplayed () {
        return waitFor(aMonitor.panelWin, aMonitor.panelWin.EVENTS.RESPONSE_BODY_DISPLAYED);
      }
    });

    aDebuggee.performRequests();
  });
}
