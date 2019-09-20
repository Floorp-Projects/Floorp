/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test event logpoints when replaying.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_events.html", {
    waitForRecording: true,
  });

  const console = await getDebuggerSplitConsole(dbg);
  const hud = console.hud;

  await dbg.actions.addEventListenerBreakpoints(["event.mouse.mousedown"]);
  await dbg.actions.toggleEventLogging();

  const msg = await waitForMessage(hud, "mousedown");

  // The message's inline preview should contain useful properties.
  const regexps = [
    /target: HTMLDivElement/,
    /clientX: \d+/,
    /clientY: \d+/,
    /layerX: \d+/,
    /layerY: \d+/,
  ];
  for (const regexp of regexps) {
    ok(regexp.test(msg.textContent), `Message text includes ${regexp}`);
  }

  // When expanded, other properties should be visible.
  await checkMessageObjectContents(msg, ["altKey: false", "bubbles: true"]);

  await shutdownDebugger(dbg);
});
