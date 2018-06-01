/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the timeline front's start/stop/isRecording methods work in a
// simple use case, and that markers events are sent when operations occur.
// Note that this test isn't concerned with which markers are actually recorded,
// just that markers are recorded at all.
// Trying to check marker types here may lead to intermittents, see bug 1066474.

const {TimelineFront} = require("devtools/shared/fronts/timeline");

add_task(async function() {
  await addTab("data:text/html;charset=utf-8,mop");

  initDebuggerServer();
  const client = new DebuggerClient(DebuggerServer.connectPipe());
  const form = await connectDebuggerClient(client);
  const front = TimelineFront(client, form);

  ok(front, "The TimelineFront was created");

  let isActive = await front.isRecording();
  ok(!isActive, "The TimelineFront is not initially recording");

  info("Flush any pending reflows");
  ContentTask.spawn(gBrowser.selectedBrowser, {}, () => {
    // forceSyncReflow
    content.document.body.innerHeight;
  });

  info("Start recording");
  await front.start({ withMarkers: true });

  isActive = await front.isRecording();
  ok(isActive, "The TimelineFront is now recording");

  info("Change some style on the page to cause style/reflow/paint");
  let onMarkers = once(front, "markers");
  ContentTask.spawn(gBrowser.selectedBrowser, {}, () => {
    content.document.body.style.padding = "10px";
  });
  let markers = await onMarkers;

  ok(true, "The markers event was fired");
  ok(markers.length > 0, "Markers were returned");

  info("Flush pending reflows again");
  ContentTask.spawn(gBrowser.selectedBrowser, {}, () => {
    // forceSyncReflow
    content.document.body.innerHeight;
  });

  info("Change some style on the page to cause style/paint");
  onMarkers = once(front, "markers");
  ContentTask.spawn(gBrowser.selectedBrowser, {}, () => {
    content.document.body.style.backgroundColor = "red";
  });
  markers = await onMarkers;

  ok(markers.length > 0, "markers were returned");

  await front.stop();

  isActive = await front.isRecording();
  ok(!isActive, "Not recording after stop()");

  await client.close();
  gBrowser.removeCurrentTab();
});
