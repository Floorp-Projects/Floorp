/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests opening the console first, clicking a link
// opens the editor at the correct location.

async function waitForLink(toolbox) {
  const { hud } = toolbox.getPanel("webconsole");

  return waitFor(() => hud.ui.outputNode.querySelector(".frame-link-source"));
}

add_task(async function() {
  const toolbox = await initPane("doc-script-switching.html", "webconsole");
  const node = await waitForLink(toolbox);
  node.click();

  await waitFor(() => toolbox.getPanel("jsdebugger"));
  const dbg = createDebuggerContext(toolbox);
  await waitForElement(dbg, ".CodeMirror-code > .highlight-line");
  assertHighlightLocation(dbg, "script-switching-02", 14);
});
