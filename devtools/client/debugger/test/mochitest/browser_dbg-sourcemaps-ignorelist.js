/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that sources on the sourcemaps ignore list are ignored on debugger load
// when the 'Ignore Known Third-party Scripts' setting is enabled.

"use strict";

add_task(async function () {
  const sources = [
    "bundle.js",
    "original-1.js",
    "original-2.js",
    "original-3.js",
    "original-4.js",
    "original-5.js",
  ];
  const dbg = await initDebugger("doc-sourcemaps-ignorelist.html", ...sources);

  info("Click the settings menu to ignore the third party scripts");
  await toggleDebbuggerSettingsMenuItem(dbg, {
    className: ".debugger-settings-menu-item-enable-sourcemap-ignore-list",
    isChecked: false,
  });
  await waitForDispatch(dbg.store, "ENABLE_SOURCEMAP_IGNORELIST");

  info(
    "Reload to hit breakpoints in the original-2.js and original-3.js files"
  );
  const onReloaded = reload(dbg, ...sources);
  await waitForPaused(dbg, null, { shouldWaitForLoadedScopes: false });

  info("Assert paused in original-2.js as original-1.js is ignored");
  const original2Source = findSource(dbg, "original-2.js");
  assertPausedAtSourceAndLine(dbg, original2Source.id, 2);

  await resume(dbg);
  await waitForPaused(dbg, null, { shouldWaitForLoadedScopes: false });

  info("Assert paused in original-4.js as original-3.js is ignored");
  const original4Source = findSource(dbg, "original-4.js");
  assertPausedAtSourceAndLine(dbg, original4Source.id, 2);

  await resume(dbg);
  await onReloaded;

  info("Click the settings menu to stop ignoring the third party scripts");
  await toggleDebbuggerSettingsMenuItem(dbg, {
    className: ".debugger-settings-menu-item-enable-sourcemap-ignore-list",
    isChecked: true,
  });
  await waitForDispatch(dbg.store, "ENABLE_SOURCEMAP_IGNORELIST");

  info("Reload to hit breakpoints in all the original-[x].js files");
  const onReloaded2 = reload(dbg, "original-1.js");
  await waitForPaused(dbg, null, { shouldWaitForLoadedScopes: false });

  info("Assert paused in original-1.js as it is no longer ignored");
  const original1Source = findSource(dbg, "original-1.js");
  assertPausedAtSourceAndLine(dbg, original1Source.id, 2);

  const originalSources = ["original-2.js", "original-3.js", "original-4.js"];
  for (const fileName of originalSources) {
    await resume(dbg);
    await waitForPaused(dbg, null, { shouldWaitForLoadedScopes: false });

    const originalSource = findSource(dbg, fileName);
    assertPausedAtSourceAndLine(dbg, originalSource.id, 2);
  }
  await resume(dbg);

  await onReloaded2;
  await dbg.toolbox.closeToolbox();
});
