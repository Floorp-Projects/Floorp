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
  let { document, gStore, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/actions/index");
  let {
    getDisplayedRequests,
    getSortedRequests,
  } = windowRequire("devtools/client/netmonitor/selectors/index");

  gStore.dispatch(Actions.batchEnable(false));

  const REQUESTS = [
    [ "hls-m3u8", /^#EXTM3U/ ],
    [ "mpeg-dash", /^<\?xml/ ]
  ];

  let wait = waitForNetworkEvents(monitor, REQUESTS.length);
  for (let [fmt] of REQUESTS) {
    let url = CONTENT_TYPE_SJS + "?fmt=" + fmt;
    yield ContentTask.spawn(tab.linkedBrowser, { url }, function* (args) {
      content.wrappedJSObject.performRequests(1, args.url);
    });
  }
  yield wait;

  REQUESTS.forEach(([ fmt ], i) => {
    verifyRequestItemTarget(
      document,
      getDisplayedRequests(gStore.getState()),
      getSortedRequests(gStore.getState()).get(i),
      "GET",
      CONTENT_TYPE_SJS + "?fmt=" + fmt,
      {
        status: 200,
        statusText: "OK"
      });
  });

  wait = waitForDOM(document, "#response-panel");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".network-details-panel-toggle"));
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#response-tab"));
  yield wait;

  gStore.dispatch(Actions.selectRequest(null));

  yield selectIndexAndWaitForEditor(0);
  // the hls-m3u8 part
  testEditorContent(REQUESTS[0]);

  yield selectIndexAndWaitForEditor(1);
  // the mpeg-dash part
  testEditorContent(REQUESTS[1]);

  return teardown(monitor);

  function* selectIndexAndWaitForEditor(index) {
    let editor = document.querySelector("#response-panel .editor-mount iframe");
    if (!editor) {
      let waitDOM = waitForDOM(document, "#response-panel .editor-mount iframe");
      EventUtils.sendMouseEvent({ type: "mousedown" },
        document.querySelectorAll(".request-list-item")[index]);
      document.querySelector("#response-tab").click();
      [editor] = yield waitDOM;
      yield once(editor, "DOMContentLoaded");
    } else {
      EventUtils.sendMouseEvent({ type: "mousedown" },
        document.querySelectorAll(".request-list-item")[index]);
    }

    yield waitForDOM(editor.contentDocument, ".CodeMirror-code");
  }

  function testEditorContent([ fmt, textRe ]) {
    let editor = document.querySelector("#response-panel .editor-mount iframe");
    let text = editor.contentDocument
          .querySelector(".CodeMirror-line").textContent;

    ok(text.match(textRe),
      "The text shown in the source editor for " + fmt + " is correct.");
  }
});
