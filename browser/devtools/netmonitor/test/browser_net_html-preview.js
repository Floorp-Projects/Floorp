/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if html responses show and properly populate a "Preview" tab.
 */

function test() {
  initNetMonitor(CONTENT_TYPE_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    let { $, document, EVENTS, NetMonitorView } = aMonitor.panelWin;
    let { RequestsMenu } = NetMonitorView;

    RequestsMenu.lazyUpdate = false;

    waitForNetworkEvents(aMonitor, 6).then(() => {
      EventUtils.sendMouseEvent({ type: "mousedown" },
        document.getElementById("details-pane-toggle"));

      is($("#event-details-pane").selectedIndex, 0,
        "The first tab in the details pane should be selected.");
      is($("#preview-tab").hidden, true,
        "The preview tab should be hidden for non html responses.");
      is($("#preview-tabpanel").hidden, true,
        "The preview tabpanel should be hidden for non html responses.");

      RequestsMenu.selectedIndex = 4;
      NetMonitorView.toggleDetailsPane({ visible: true, animated: false }, 5);

      is($("#event-details-pane").selectedIndex, 5,
        "The fifth tab in the details pane should be selected.");
      is($("#preview-tab").hidden, false,
        "The preview tab should be visible now.");
      is($("#preview-tabpanel").hidden, false,
        "The preview tabpanel should be visible now.");

      waitFor(aMonitor.panelWin, EVENTS.RESPONSE_HTML_PREVIEW_DISPLAYED).then(() => {
        let iframe = $("#response-preview");
        ok(iframe,
          "There should be a response preview iframe available.");
        ok(iframe.contentDocument,
          "The iframe's content document should be available.");
        is(iframe.contentDocument.querySelector("blink").textContent, "Not Found",
          "The iframe's content document should be loaded and correct.");

        RequestsMenu.selectedIndex = 5;

        is($("#event-details-pane").selectedIndex, 0,
          "The first tab in the details pane should be selected again.");
        is($("#preview-tab").hidden, true,
          "The preview tab should be hidden again for non html responses.");
        is($("#preview-tabpanel").hidden, true,
          "The preview tabpanel should be hidden again for non html responses.");

        teardown(aMonitor).then(finish);
      });
    });

    aDebuggee.performRequests();
  });
}
