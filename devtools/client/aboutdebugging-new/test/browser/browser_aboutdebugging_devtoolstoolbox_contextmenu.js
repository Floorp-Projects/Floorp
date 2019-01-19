/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-collapsibilities.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-collapsibilities.js", this);

/**
 * Test context menu on about:devtools-toolbox page.
 */
add_task(async function() {
  info("Force all debug target panes to be expanded");
  prepareCollapsibilitiesTest();

  const { document, tab, window } = await openAboutDebugging();

  info("Show about:devtools-toolbox page");
  const target = findDebugTargetByText("about:debugging", document);
  ok(target, "about:debugging tab target appeared");
  const inspectButton = target.querySelector(".js-debug-target-inspect-button");
  ok(inspectButton, "Inspect button for about:debugging appeared");
  inspectButton.click();
  await Promise.all([
    waitUntil(() => tab.nextElementSibling),
    waitForRequestsToSettle(window.AboutDebugging.store),
    gDevTools.once("toolbox-ready"),
  ]);

  info("Wait for about:devtools-toolbox tab will be selected");
  const devtoolsTab = tab.nextElementSibling;
  await waitUntil(() => gBrowser.selectedTab === devtoolsTab);
  info("Check whether the menu item which opens devtools is disabled");
  const rootDocument = devtoolsTab.ownerDocument;
  await assertContextMenu(rootDocument, gBrowser.selectedBrowser,
                          ".debug-target-info", false);

  info("Force to select about:debugging page");
  gBrowser.selectedTab = tab;
  info("Check whether the menu item which opens devtools is enabled");
  await assertContextMenu(rootDocument, gBrowser.selectedBrowser, "#mount", true);

  await removeTab(devtoolsTab);
  await Promise.all([
    waitForRequestsToSettle(window.AboutDebugging.store),
    gDevTools.once("toolbox-destroyed"),
  ]);
  await removeTab(tab);
});

async function assertContextMenu(rootDocument, browser, targetSelector, shouldBeEnabled) {
  // Show content context menu.
  const contextMenu = rootDocument.getElementById("contentAreaContextMenu");
  const popupShownPromise = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  BrowserTestUtils.synthesizeMouseAtCenter(targetSelector,
                                           { type: "contextmenu" }, browser);
  await popupShownPromise;

  // Check hidden attribute of #context-inspect.
  const inspectMenuItem = rootDocument.getElementById("context-inspect");
  is(inspectMenuItem.hidden, !shouldBeEnabled,
     '"hidden" attribute of #context-inspect should be correct');

  // Hide content context menu.
  const popupHiddenPromise = BrowserTestUtils.waitForEvent(contextMenu, "popuphidden");
  contextMenu.hidePopup();
  await popupHiddenPromise;
}
