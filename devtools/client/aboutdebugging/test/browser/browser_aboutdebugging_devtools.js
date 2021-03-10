/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-collapsibilities.js */
Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "helper-collapsibilities.js",
  this
);

/**
 * Check that DevTools are not closed when leaving This Firefox runtime page.
 */

add_task(async function() {
  info("Force all debug target panes to be expanded");
  prepareCollapsibilitiesTest();

  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  const connectSidebarItem = findSidebarItemByText("Setup", document);
  const connectLink = connectSidebarItem.querySelector(".qa-sidebar-link");
  ok(connectSidebarItem, "Found the Connect sidebar item");

  info("Open devtools on the current about:debugging tab");
  const toolbox = await openToolboxForTab(tab, "inspector");
  const inspector = toolbox.getPanel("inspector");

  info("DevTools starts workers, wait for requests to settle");
  const store = window.AboutDebugging.store;
  await waitForAboutDebuggingRequests(store);

  info("Click on the Connect item in the sidebar");
  connectLink.click();
  await aboutDebugging_waitForDispatch(store, "UNWATCH_RUNTIME_SUCCESS");

  info("Wait until Connect page is displayed");
  await waitUntil(() => document.querySelector(".qa-connect-page"));

  const markupViewElement = inspector.panelDoc.getElementById("markup-box");
  ok(markupViewElement, "Inspector is still rendered");

  // We explicitely destroy the toolbox in order to ensure waiting for its full destruction
  // and avoid leak / pending requests
  info("Destroy the opened toolbox");
  await toolbox.destroy();

  await removeTab(tab);
});
