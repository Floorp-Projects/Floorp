/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

async function waitForConsoleLink(dbg, text) {
  const toolbox = dbg.toolbox;
  const console = await toolbox.selectTool("webconsole");
  const hud = console.hud;

  return waitFor(() => {
    // Wait until the message updates.
    const found = hud.ui.outputNode.querySelector(".frame-link-source");
    if (!found) {
      return false;
    }

    const linkText = found.textContent;
    if (!text) {
      return linkText;
    }

    return linkText == text ? linkText : null;
  });
}

// Tests that pretty-printing updates console messages.
add_task(async function() {
  const dbg = await initDebugger("doc-minified.html");
  invokeInTab("arithmetic");

  info("Switch to console and check message");
  await waitForConsoleLink(dbg, "math.min.js:3:73");

  info("Switch back to debugger and pretty-print");
  await dbg.toolbox.selectTool("jsdebugger");
  await selectSource(dbg, "math.min.js", 2);

  clickElement(dbg, "prettyPrintButton");
  await waitForSelectedSource(dbg, "math.min.js:formatted");

  info("Switch back to console and check message");
  await waitForConsoleLink(dbg, "math.min.js:formatted:22");
  ok(true);
});
