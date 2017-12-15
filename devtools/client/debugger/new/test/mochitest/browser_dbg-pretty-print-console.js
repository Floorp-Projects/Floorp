/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that pretty-printing updates console messages.

add_task(async function() {
  const dbg = await initDebugger("doc-minified.html");
  invokeInTab("arithmetic");

  info("Switch to console and check message");
  const toolbox = dbg.toolbox;
  const console = await toolbox.selectTool("webconsole");
  const hud = console.hud;

  let node = await waitFor(() =>
    hud.ui.outputNode.querySelector(".frame-link-source")
  );
  const initialLocation = "math.min.js:3:65";
  is(node.textContent, initialLocation, "location is correct in minified code");

  info("Switch back to debugger and pretty-print");
  await toolbox.selectTool("jsdebugger");
  await selectSource(dbg, "math.min.js", 2);
  clickElement(dbg, "prettyPrintButton");

  await waitForSource(dbg, "math.min.js:formatted");
  const ppSrc = findSource(dbg, "math.min.js:formatted");

  ok(ppSrc, "Pretty-printed source exists");

  info("Switch back to console and check message");
  node = await waitFor(() => {
    // Wait until the message updates.
    const found = hud.ui.outputNode.querySelector(".frame-link-source");
    if (found.textContent == initialLocation) {
      return null;
    }
    return found;
  });

  is(
    node.textContent,
    "math.min.js:formatted:22",
    "location is correct in minified code"
  );
});
