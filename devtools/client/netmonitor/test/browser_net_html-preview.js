/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if html responses show and properly populate a "Preview" tab.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(CONTENT_TYPE_URL);
  info("Starting test... ");

  let { $, document, EVENTS, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;

  let wait = waitForNetworkEvents(monitor, 6);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests();
  });
  yield wait;

  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.getElementById("details-pane-toggle"));

  is($("#event-details-pane").selectedIndex, 0,
    "The first tab in the details pane should be selected.");
  is($("#preview-tab").hidden, true,
    "The preview tab should be hidden for non html responses.");
  is($("#preview-tabpanel").hidden, false,
    "The preview tabpanel is not hidden for non html responses.");

  RequestsMenu.selectedIndex = 4;
  NetMonitorView.toggleDetailsPane({ visible: true, animated: false }, 6);

  is($("#event-details-pane").selectedIndex, 6,
    "The sixth tab in the details pane should be selected.");
  is($("#preview-tab").hidden, false,
    "The preview tab should be visible now.");

  yield monitor.panelWin.once(EVENTS.RESPONSE_HTML_PREVIEW_DISPLAYED);
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
  is($("#preview-tabpanel").hidden, false,
    "The preview tabpanel is not hidden again for non html responses.");

  yield teardown(monitor);
});
