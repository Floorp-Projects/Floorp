/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if reponses from streaming content types (MPEG-DASH, HLS) are
 * displayed as XML or plain text
 */

function test() {
  Task.spawn(function* () {
    let [tab, debuggee, monitor] = yield initNetMonitor(CUSTOM_GET_URL);

    info("Starting test... ");
    let { panelWin } = monitor;
    let { document, Editor, NetMonitorView } = panelWin;
    let { RequestsMenu } = NetMonitorView;

    const REQUESTS = [
      [ "hls-m3u8", /^#EXTM3U/, Editor.modes.text ],
      [ "mpeg-dash", /^<\?xml/, Editor.modes.html ]
    ];

    RequestsMenu.lazyUpdate = false;

    REQUESTS.forEach(([ fmt ]) => {
      debuggee.performRequests(1, CONTENT_TYPE_SJS + "?fmt=" + fmt);
    });

    yield waitForNetworkEvents(monitor, REQUESTS.length);

    REQUESTS.forEach(([ fmt ], i) => {
      verifyRequestItemTarget(RequestsMenu.getItemAtIndex(i),
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

    testEditorContent(editor, REQUESTS[0]); // the hls-m3u8 part

    RequestsMenu.selectedIndex = 1;
    yield panelWin.once(panelWin.EVENTS.TAB_UPDATED);
    yield panelWin.once(panelWin.EVENTS.RESPONSE_BODY_DISPLAYED);

    testEditorContent(editor, REQUESTS[1]); // the mpeg-dash part

    yield teardown(monitor);
    finish();
  });

  function testEditorContent(editor, [ fmt, textRe, mode ]) {
    ok(editor.getText().match(textRe),
      "The text shown in the source editor for " + fmt + " is incorrect.");
    is(editor.getMode(), mode,
      "The mode active in the source editor for " + fmt + " is incorrect.");
  }
}
