/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// The test has a lot of interactions between debugger and console panels which
// might take more than 30s to complete on a slow machine.
requestLongerTimeout(2);

// Tests that pretty-printing updates console messages.
add_task(async function() {
  const dbg = await initDebugger("doc-minified.html");
  invokeInTab("arithmetic");

  info("Switch to console and check message");
  const minifiedLink = await waitForConsoleLink(dbg, "math.min.js:3:73");

  info("Click on the link to open the debugger");
  minifiedLink.click();
  await waitForSelectedSource(dbg, "math.min.js");
  await waitForSelectedLocation(dbg, 3);

  info("Click on pretty print button and wait for the file to be formatted");
  clickElement(dbg, "prettyPrintButton");
  await waitForSelectedSource(dbg, "math.min.js:formatted");

  info("Switch back to console and check message");
  const formattedLink = await waitForConsoleLink(
    dbg,
    "math.min.js:formatted:22"
  );
  ok(true, "Message location was updated as expected");

  info(
    "Click on the link again and check the debugger opens in formatted file"
  );
  formattedLink.click();
  await selectSource(dbg, "math.min.js:formatted");
  await waitForSelectedLocation(dbg, 22);
});

async function waitForConsoleLink(dbg, text) {
  const toolbox = dbg.toolbox;
  const console = await toolbox.selectTool("webconsole");
  const hud = console.hud;

  return waitFor(() => {
    // Wait until the message updates.
    const linkEl = hud.ui.outputNode.querySelector(".frame-link-source");
    if (!linkEl) {
      return false;
    }

    const linkText = linkEl.textContent;
    return linkText == text ? linkEl : null;
  });
}
