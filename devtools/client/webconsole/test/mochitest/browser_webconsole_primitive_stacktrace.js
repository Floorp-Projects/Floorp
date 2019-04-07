/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that stack traces are shown when primitive values are thrown instead of
// error objects.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/mochitest/" +
                 "test-primitive-stacktrace.html";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  await checkMessage("hello", 14, 3);
  await checkMessage("1,2,3", 20, 1);

  async function checkMessage(text, line, numFrames) {
    const msgNode = await waitFor(() => findMessage(hud, text));
    ok(!msgNode.classList.contains("open"), `Error logged not expanded`);

    const button = msgNode.querySelector(".collapse-button");
    button.click();

    const framesNode = await waitFor(() => msgNode.querySelector(".frames"));
    const frameNodes = framesNode.querySelectorAll(".frame");
    ok(frameNodes.length == numFrames);
    ok(frameNodes[0].querySelector(".line").textContent == "" + line);
  }
});
