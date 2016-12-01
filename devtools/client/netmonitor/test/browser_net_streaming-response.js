/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if reponses from streaming content types (MPEG-DASH, HLS) are
 * displayed as XML or plain text
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(CUSTOM_GET_URL);

  info("Starting test... ");
  let { panelWin } = monitor;
  let { document, Editor, NetMonitorView } = panelWin;
  let { RequestsMenu } = NetMonitorView;

  const REQUESTS = [
    [ "hls-m3u8", /^#EXTM3U/, Editor.modes.text ],
    [ "mpeg-dash", /^<\?xml/, Editor.modes.html ]
  ];

  RequestsMenu.lazyUpdate = false;

  let wait = waitForNetworkEvents(monitor, REQUESTS.length);
  for (let [fmt] of REQUESTS) {
    let url = CONTENT_TYPE_SJS + "?fmt=" + fmt;
    yield ContentTask.spawn(tab.linkedBrowser, { url }, function* (args) {
      content.wrappedJSObject.performRequests(1, args.url);
    });
  }
  yield wait;

  REQUESTS.forEach(([ fmt ], i) => {
    verifyRequestItemTarget(RequestsMenu, RequestsMenu.getItemAtIndex(i),
      "GET", CONTENT_TYPE_SJS + "?fmt=" + fmt, {
        status: 200,
        statusText: "OK"
      });
  });

  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.getElementById("details-pane-toggle"));
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll("#details-pane tab")[3]);

  yield panelWin.once(panelWin.EVENTS.RESPONSE_BODY_DISPLAYED);
  let editor = yield NetMonitorView.editor("#response-content-textarea");

  // the hls-m3u8 part
  testEditorContent(editor, REQUESTS[0]);

  RequestsMenu.selectedIndex = 1;
  yield panelWin.once(panelWin.EVENTS.TAB_UPDATED);
  yield panelWin.once(panelWin.EVENTS.RESPONSE_BODY_DISPLAYED);

  // the mpeg-dash part
  testEditorContent(editor, REQUESTS[1]);

  return teardown(monitor);

  function testEditorContent(e, [ fmt, textRe, mode ]) {
    ok(e.getText().match(textRe),
      "The text shown in the source editor for " + fmt + " is correct.");
    is(e.getMode(), mode,
      "The mode active in the source editor for " + fmt + " is correct.");
  }
});
