/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests opening the console first, clicking a link
// opens the editor at the correct location.

"use strict";

add_task(async function () {
  const toolbox = await initPane("doc-script-switching.html", "webconsole");
  const node = await waitForLink(toolbox, "hi");
  node.click();

  await waitFor(() => toolbox.getPanel("jsdebugger"));
  const dbg = createDebuggerContext(toolbox);
  await waitForElementWithSelector(dbg, ".CodeMirror-code > .highlight-line");
  assertHighlightLocation(dbg, "script-switching-02.js", 18);
});

async function waitForLink(toolbox, messageText) {
  return waitFor(async () => {
    const [message] = await findConsoleMessages(toolbox, messageText);
    if (!message) {
      return false;
    }
    return message.querySelector(".frame-link-source");
  });
}
