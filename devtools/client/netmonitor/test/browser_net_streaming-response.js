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
  let { document, NetMonitorView } = panelWin;
  let { RequestsMenu } = NetMonitorView;

  const REQUESTS = [
    [ "hls-m3u8", /^#EXTM3U/ ],
    [ "mpeg-dash", /^<\?xml/ ]
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

  wait = waitForDOM(document, "#panel-3");
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.getElementById("details-pane-toggle"));
  document.querySelector("#tab-3 a").click();
  yield wait;

  RequestsMenu.selectedIndex = -1;

  yield selectIndexAndWaitForEditor(0);
  // the hls-m3u8 part
  testEditorContent(REQUESTS[0]);

  yield selectIndexAndWaitForEditor(1);
  // the mpeg-dash part
  testEditorContent(REQUESTS[1]);

  return teardown(monitor);

  function* selectIndexAndWaitForEditor(index) {
    let editor = document.querySelector("#panel-3 .editor-mount iframe");
    if (!editor) {
      let waitDOM = waitForDOM(document, "#panel-3 .editor-mount iframe");
      RequestsMenu.selectedIndex = index;
      [editor] = yield waitDOM;
      yield once(editor, "DOMContentLoaded");
    } else {
      RequestsMenu.selectedIndex = index;
    }

    yield waitForDOM(editor.contentDocument, ".CodeMirror-code");
  }

  function testEditorContent([ fmt, textRe ]) {
    let editor = document.querySelector("#panel-3 .editor-mount iframe");
    let text = editor.contentDocument
          .querySelector(".CodeMirror-line").textContent;

    ok(text.match(textRe),
      "The text shown in the source editor for " + fmt + " is correct.");
  }
});
