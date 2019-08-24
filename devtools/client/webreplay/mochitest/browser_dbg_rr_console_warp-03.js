/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

const BrowserTest = {
  gTestPath,
  ok,
  registerCleanupFunction,
  waitForExplicitFinish,
  BrowserTestUtils,
};

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/webconsole/test/browser/head.js",
  BrowserTest
);

// Test evaluating various expressions in the console after time warping.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_rr_error.html", {
    waitForRecording: true,
  });

  const { toolbox } = dbg;
  const console = await toolbox.selectTool("webconsole");
  const hud = console.hud;

  await warpToMessage(hud, dbg, "Number 5");

  BrowserTest.execute(hud, "Error('helo')");
  await waitFor(() => findMessage(hud, "helo"));

  BrowserTest.execute(
    hud,
    `
function f() {
  throw Error("there");
}
f();
`
  );
  await BrowserTest.checkMessageStack(hud, "Error: there", [3, 5]);

  await shutdownDebugger(dbg);
});
