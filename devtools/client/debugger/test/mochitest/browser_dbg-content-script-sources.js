/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that the content scripts are listed in the source tree.

"use strict";

add_task(async function() {
  await pushPref("devtools.chrome.enabled", true);
  const extension = await installAndStartContentScriptExtension();

  let dbg = await initDebugger(
    "doc-content-script-sources.html",
    "content_script.js"
  );
  await selectSource(dbg, "content_script.js");
  await closeTab(dbg, "content_script.js");

  // Destroy the toolbox and repeat the test in a new toolbox
  // and ensures that the content script is still listed.
  await dbg.toolbox.destroy();

  const toolbox = await openToolboxForTab(gBrowser.selectedTab, "jsdebugger");
  dbg = createDebuggerContext(toolbox);
  await waitForSources(dbg, "content_script.js");
  await selectSource(dbg, "content_script.js");

  await addBreakpoint(dbg, "content_script.js", 2);

  for (let i = 1; i < 3; i++) {
    info(`Reloading tab (${i} time)`);
    gBrowser.reloadTab(gBrowser.selectedTab);
    await waitForPaused(dbg);
    await waitForSelectedSource(dbg, "content_script.js");

    ok(
      findElementWithSelector(dbg, ".sources-list .focused"),
      "Source is focused"
    );
    assertPausedAtSourceAndLine(
      dbg,
      findSource(dbg, "content_script.js").id,
      2
    );
    await resume(dbg);
  }

  await closeTab(dbg, "content_script.js");

  await extension.unload();
});
