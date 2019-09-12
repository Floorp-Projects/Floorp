/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that the application panel only displays service workers from the
 * current domain.
 */

const EMPTY_URL = URL_ROOT + "resources/service-workers/empty.html";

add_task(async function() {
  await enableApplicationPanel();

  const { panel, tab } = await openNewTabAndApplicationPanel(EMPTY_URL);
  const doc = panel.panelWin.document;

  selectPage(panel, "service-workers");

  await waitUntil(() => doc.querySelector(".js-worker-list-empty") !== null);
  ok(true, "No service workers are shown for an empty page");

  // close the tab
  info("Closing the tab.");
  await BrowserTestUtils.removeTab(tab);
});
