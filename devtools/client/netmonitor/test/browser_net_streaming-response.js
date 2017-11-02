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
  let { document, store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  let {
    getDisplayedRequests,
    getSortedRequests,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");

  store.dispatch(Actions.batchEnable(false));

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
      getDisplayedRequests(store.getState()),
      getSortedRequests(store.getState()).get(i),
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

  store.dispatch(Actions.selectRequest(null));

  yield selectIndexAndWaitForSourceEditor(0);
  // the hls-m3u8 part
  testEditorContent(REQUESTS[0]);

  yield selectIndexAndWaitForSourceEditor(1);
  // the mpeg-dash part
  testEditorContent(REQUESTS[1]);

  return teardown(monitor);

  function* selectIndexAndWaitForSourceEditor(index) {
    let editor = document.querySelector("#response-panel .CodeMirror-code");
    if (!editor) {
      let waitDOM = waitForDOM(document, "#response-panel .CodeMirror-code");
      EventUtils.sendMouseEvent({ type: "mousedown" },
        document.querySelectorAll(".request-list-item")[index]);
      document.querySelector("#response-tab").click();
      yield waitDOM;
    } else {
      EventUtils.sendMouseEvent({ type: "mousedown" },
        document.querySelectorAll(".request-list-item")[index]);
    }
  }

  function testEditorContent([ fmt, textRe ]) {
    ok(document.querySelector(".CodeMirror-line").textContent.match(textRe),
      "The text shown in the source editor for " + fmt + " is correct.");
  }
});
