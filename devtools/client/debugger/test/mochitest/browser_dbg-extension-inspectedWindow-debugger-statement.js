/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test that web extensions' inspectedWindow.eval() doesn't break debugger/console

"use strict";

// Test debugger statement in page, with devtools opened to debugger panel
add_task(async function () {
  const extension = await installAndStartExtension();

  const dbg = await initDebugger("doc-scripts.html");

  await extension.awaitMessage("loaded");

  info("Evaluating debugger statement in page");
  const evalFinished = invokeInTab("nestedC");
  await waitForPaused(dbg);

  info("resuming once");
  await resume(dbg);

  // bug 1728290: WebExtension target used to trigger the thread actor and also pause a second time on the debugger statement.
  // This would prevent the evaluation from completing.
  info("waiting for invoked function to complete");
  await evalFinished;

  await closeTabAndToolbox();
  await extension.unload();
});

// Test debugger statement in webconsole
add_task(async function () {
  const extension = await installAndStartExtension();

  // Test again with debugger panel closed
  const toolbox = await openNewTabAndToolbox(
    EXAMPLE_URL + "doc-scripts.html",
    "webconsole"
  );
  await extension.awaitMessage("loaded");

  info("Evaluating debugger statement in console");
  const onSelected = toolbox.once("jsdebugger-selected");
  const evalFinished = invokeInTab("nestedC");

  await onSelected;
  const dbg = createDebuggerContext(toolbox);
  await waitForPaused(dbg);

  info("resuming once");
  await resume(dbg);

  await evalFinished;

  await closeTabAndToolbox();
  await extension.unload();
});

async function installAndStartExtension() {
  async function devtools_page() {
    await globalThis.browser.devtools.inspectedWindow.eval("");
    globalThis.browser.test.sendMessage("loaded");
  }

  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      devtools_page: "devtools_page.html",
    },
    files: {
      "devtools_page.html": `<!DOCTYPE html>
      <html>
       <head>
         <meta charset="utf-8">
       </head>
       <body>
         <script src="devtools_page.js"></script>
       </body>
      </html>`,
      "devtools_page.js": devtools_page,
    },
  });

  await extension.startup();

  return extension;
}
