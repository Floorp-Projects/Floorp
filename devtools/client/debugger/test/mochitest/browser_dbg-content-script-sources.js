/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Tests that the content scripts are listed in the source tree.

add_task(async function() {
  await pushPref("devtools.chrome.enabled", true);
  const extension = await installAndStartExtension();

  let dbg = await initDebugger("doc-content-script-sources.html");
  await clickElement(dbg, "sourceDirectoryLabel", 2);


  await selectContentScriptSources(dbg);
  await closeTab(dbg, "content_script.js");

  // Destroy the toolbox and repeat the test in a new toolbox
  // and ensures that the content script is still listed.
  await dbg.toolbox.destroy();
  const toolbox = await openToolboxForTab(gBrowser.selectedTab, "jsdebugger");
  dbg = createDebuggerContext(toolbox);

  await clickElement(dbg, "sourceDirectoryLabel", 2);

  await selectContentScriptSources(dbg);

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
    assertPausedLocation(dbg);
    assertDebugLine(dbg, 2);
    await resume(dbg);
  }

  await closeTab(dbg, "content_script.js");

  await extension.unload();
});

async function selectContentScriptSources(dbg) {
  await waitForSources(dbg, "content_script.js");

  // Select a source.
  await selectSource(dbg, "content_script.js");

  ok(
    findElementWithSelector(dbg, ".sources-list .focused"),
    "Source is focused"
  );
}

async function installAndStartExtension() {
  function contentScript() {
    console.log("content script loads");

    // This listener prevents the source from being garbage collected
    // and be missing from the scripts returned by `dbg.findScripts()`
    // in `ThreadActor._discoverSources`.
    window.onload = () => {};
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          js: ["content_script.js"],
          matches: ["http://example.com/*"],
          run_at: "document_start"
        }
      ]
    },
    files: {
      "content_script.js": contentScript
    }
  });

  await extension.startup();

  return extension;
}