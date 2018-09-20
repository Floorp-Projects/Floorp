/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from debug-target-pane_collapsibilities_head.js */

"use strict";

/**
 * Test for preference of DebugTargetPane collapsibilities.
 */

Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "debug-target-pane_collapsibilities_head.js", this);

add_task(async function() {
  prepareCollapsibilitiesTest();

  const { document, tab } = await openAboutDebugging();

  info("Collapse all pane");
  for (const { title } of TARGET_PANES) {
    const debugTargetPaneEl = getDebugTargetPane(title, document);
    await toggleCollapsibility(debugTargetPaneEl);
  }

  info("Check preference of collapsibility after closing about:debugging page");
  await removeTab(tab);
  // Wait until unmount.
  await waitUntil(() => document.querySelector(".app") === null);

  for (const { pref } of TARGET_PANES) {
    is(Services.prefs.getBoolPref(pref), true, `${ pref } preference should be true`);
  }
});
