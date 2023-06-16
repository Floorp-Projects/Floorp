/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Test reload via an href link, which refers to the same document.
 * It seems to cause different codepath compared to F5.
 */

"use strict";

add_task(async function () {
  const dbg = await initDebugger(
    "doc-reload-link.html",
    "doc-reload-link.html"
  );

  info("Add a breakpoint that will be hit on reload");
  await addBreakpoint(dbg, "doc-reload-link.html", 3);

  for (let i = 0; i < 5; i++) {
    const onReloaded = waitForReload(dbg.commands);

    info(
      "Reload via a link, this causes special race condition different from F5"
    );
    await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
      const reloadLink = content.document.querySelector("a");
      reloadLink.click();
    });

    info("Wait for paused\n");
    await waitForPaused(dbg);

    info("Check paused location\n");
    const source = findSource(dbg, "doc-reload-link.html");
    assertPausedAtSourceAndLine(dbg, source.id, 3);

    await resume(dbg);

    info("Wait for completion of the page load");
    // This help ensure that the page loaded correctly and prevent pending request at teardown
    await onReloaded;
  }
});

async function waitForReload(commands) {
  let resolve;
  const onReloaded = new Promise(r => (resolve = r));
  const { resourceCommand } = commands;
  const { DOCUMENT_EVENT } = resourceCommand.TYPES;
  const onAvailable = resources => {
    if (resources.find(resource => resource.name == "dom-complete")) {
      resourceCommand.unwatchResources([DOCUMENT_EVENT], { onAvailable });
      resolve();
    }
  };
  // Wait for watchResources completion before reloading, otherwise we might miss the dom-complete event
  // if watchResources is still pending while the reload already started and finished loading the document early.
  await resourceCommand.watchResources([DOCUMENT_EVENT], {
    onAvailable,
    ignoreExistingResources: true,
  });
  return onReloaded;
}
