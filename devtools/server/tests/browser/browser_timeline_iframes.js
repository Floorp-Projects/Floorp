/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the timeline front receives markers events for operations that occur in
// iframes.

const {TimelineFront} = require("devtools/shared/fronts/timeline");

add_task(function* () {
  yield addTab(MAIN_DOMAIN + "timeline-iframe-parent.html");

  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = yield connectDebuggerClient(client);
  let front = TimelineFront(client, form);

  info("Start timeline marker recording");
  yield front.start({ withMarkers: true });

  // Check that we get markers for a few iterations of the timer that runs in
  // the child frame.
  for (let i = 0; i < 3; i++) {
    // That's the time the child frame waits before changing styles.
    yield wait(300);
    let markers = yield once(front, "markers");
    ok(markers.length, "Markers were received for operations in the child frame");
  }

  info("Stop timeline marker recording");
  yield front.stop();
  yield client.close();
  gBrowser.removeCurrentTab();
});

function wait(ms) {
  return new Promise(resolve =>
    setTimeout(resolve, ms));
}
