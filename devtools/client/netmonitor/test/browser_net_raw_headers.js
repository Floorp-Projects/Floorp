/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var gPanelWin;
var gPanelDoc;

/**
 * Tests if showing raw headers works.
 */

function test() {
  initNetMonitor(POST_DATA_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    gPanelWin = aMonitor.panelWin;
    gPanelDoc = gPanelWin.document;

    let { document, Editor, NetMonitorView } = gPanelWin;
    let { RequestsMenu } = NetMonitorView;
    let TAB_UPDATED = gPanelWin.EVENTS.TAB_UPDATED;

    RequestsMenu.lazyUpdate = false;

    waitForNetworkEvents(aMonitor, 0, 2).then(() => {
      let origItem = RequestsMenu.getItemAtIndex(0);
      RequestsMenu.selectedItem = origItem;

      waitFor(aMonitor.panelWin, TAB_UPDATED).then(() => {
        EventUtils.sendMouseEvent({ type: "click" }, document.getElementById("toggle-raw-headers"));
        testShowRawHeaders(origItem.attachment);
        EventUtils.sendMouseEvent({ type: "click" }, document.getElementById("toggle-raw-headers"));
        testHideRawHeaders(document);
        finishUp(aMonitor);
      });
    });

    aDebuggee.performRequests();
  });
}

/*
 * Tests that raw headers were displayed correctly
 */
function testShowRawHeaders(aData) {
  let requestHeaders = gPanelDoc.getElementById("raw-request-headers-textarea").value;
  for (let header of aData.requestHeaders.headers) {
    ok(requestHeaders.indexOf(header.name + ": " + header.value) >= 0, "textarea contains request headers");
  }
  let responseHeaders = gPanelDoc.getElementById("raw-response-headers-textarea").value;
  for (let header of aData.responseHeaders.headers) {
    ok(responseHeaders.indexOf(header.name + ": " + header.value) >= 0, "textarea contains response headers");
  }
}

/*
 * Tests that raw headers textareas are hidden and empty
 */
function testHideRawHeaders(document) {
  let rawHeadersHidden = document.getElementById("raw-headers").getAttribute("hidden");
  let requestTextarea = document.getElementById("raw-request-headers-textarea");
  let responseTextare = document.getElementById("raw-response-headers-textarea");
  ok(rawHeadersHidden, "raw headers textareas are hidden");
  ok(requestTextarea.value == "", "raw request headers textarea is empty");
  ok(responseTextare.value == "", "raw response headers textarea is empty");
}

function finishUp(aMonitor) {
  gPanelWin = null;
  gPanelDoc = null;

  teardown(aMonitor).then(finish);
}
