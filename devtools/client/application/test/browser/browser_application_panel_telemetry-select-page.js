/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function() {
  await enableApplicationPanel();

  const TAB_URL = URL_ROOT + "resources/service-workers/empty.html";
  const { panel, tab, commands } = await openNewTabAndApplicationPanel(TAB_URL);
  const doc = panel.panelWin.document;

  setupTelemetryTest();

  // make sure the default page is opened and then select a different one
  await waitUntil(() => doc.querySelector(".js-service-workers-page") !== null);
  ok(true, "Service Workers page was loaded per default.");
  selectPage(panel, "manifest");

  checkTelemetryEvent({ method: "select_page", page_type: "manifest" });

  // close the tab
  info("Closing the tab.");
  await commands.client.waitForRequestsToSettle();
  await BrowserTestUtils.removeTab(tab);
});
