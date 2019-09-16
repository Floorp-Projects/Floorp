/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Inspecting objects can lead to uncaught rejections when shutting down.
PromiseTestUtils.whitelistRejectionsGlobally(
  /can't be sent as the connection just closed/
);

function findNode(dbg, text) {
  for (let index = 0; ; index++) {
    const elem = findElement(dbg, "scopeNode", index);
    if (elem && elem.innerText == text) {
      return elem;
    }
  }
}

function toggleNode(dbg, text) {
  return toggleObjectInspectorNode(findNode(dbg, text));
}

// Test that objects show up correctly in the scope pane.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_rr_objects.html", {
    waitForRecording: true,
  });

  const console = await getDebuggerSplitConsole(dbg);
  const hud = console.hud;

  await warpToMessage(hud, dbg, "Done");

  // We should be able to expand the window and see its properties.
  await toggleNode(dbg, "<this>");
  findNode(dbg, "bar()");
  findNode(dbg, "baz()");

  await shutdownDebugger(dbg);
});
