/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

// Test the timeline front receives markers events for operations that occur in
// iframes.

const {TimelineFront} = require("devtools/shared/fronts/timeline");

add_task(async function() {
  await addTab(MAIN_DOMAIN + "timeline-iframe-parent.html");

  initDebuggerServer();
  const client = new DebuggerClient(DebuggerServer.connectPipe());
  const form = await connectDebuggerClient(client);
  const front = TimelineFront(client, form);

  info("Start timeline marker recording");
  await front.start({ withMarkers: true });

  // Check that we get markers for a few iterations of the timer that runs in
  // the child frame.
  for (let i = 0; i < 3; i++) {
    // That's the time the child frame waits before changing styles.
    await wait(300);
    const markers = await once(front, "markers");
    ok(markers.length, "Markers were received for operations in the child frame");
  }

  info("Stop timeline marker recording");
  await front.stop();
  await client.close();
  gBrowser.removeCurrentTab();
});

function wait(ms) {
  return new Promise(resolve =>
    setTimeout(resolve, ms));
}
