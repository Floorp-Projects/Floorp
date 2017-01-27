/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if html responses show and properly populate a "Preview" tab.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(CONTENT_TYPE_URL);
  info("Starting test... ");

  let { document, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;

  let wait = waitForNetworkEvents(monitor, 6);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests();
  });
  yield wait;

  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.getElementById("details-pane-toggle"));

  ok(document.querySelector("#headers-tab[aria-selected=true]"),
    "The headers tab in the details panel should be selected.");
  ok(!document.querySelector("#preview-tab"),
    "The preview tab should be hidden for non html responses.");
  ok(!document.querySelector("#preview-panel"),
    "The preview panel is hidden for non html responses.");

  wait = waitForDOM(document, ".tabs");
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[4]);
  yield wait;

  document.querySelector("#preview-tab").click();

  ok(document.querySelector("#preview-tab[aria-selected=true]"),
    "The preview tab in the details panel should be selected.");
  ok(document.querySelector("#preview-panel"),
    "The preview panel should be visible now.");

  let iframe = document.querySelector("#preview-panel iframe");
  yield once(iframe, "DOMContentLoaded");

  ok(iframe,
    "There should be a response preview iframe available.");
  ok(iframe.contentDocument,
    "The iframe's content document should be available.");
  is(iframe.contentDocument.querySelector("blink").textContent, "Not Found",
    "The iframe's content document should be loaded and correct.");

  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[5]);

  ok(document.querySelector("#headers-tab[aria-selected=true]"),
    "The headers tab in the details panel should be selected again.");
  ok(!document.querySelector("#preview-tab"),
    "The preview tab should be hidden again for non html responses.");
  ok(!document.querySelector("#preview-panel"),
    "The preview panel is hidden again for non html responses.");

  yield teardown(monitor);
});
