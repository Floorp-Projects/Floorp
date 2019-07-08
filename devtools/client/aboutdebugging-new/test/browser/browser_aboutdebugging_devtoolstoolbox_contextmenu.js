/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-collapsibilities.js */
Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "helper-collapsibilities.js",
  this
);

/**
 * Test context menu on about:devtools-toolbox page.
 */
add_task(async function() {
  info("Force all debug target panes to be expanded");
  prepareCollapsibilitiesTest();

  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);
  const { devtoolsBrowser, devtoolsTab } = await openAboutDevtoolsToolbox(
    document,
    tab,
    window
  );

  info("Check whether the menu item which opens devtools is disabled");
  const rootDocument = devtoolsTab.ownerDocument;
  await assertContextMenu(
    rootDocument,
    devtoolsBrowser,
    ".debug-target-info",
    false
  );

  info("Force to select about:debugging page");
  gBrowser.selectedTab = tab;
  info("Check whether the menu item which opens devtools is enabled");
  await assertContextMenu(rootDocument, devtoolsBrowser, "#mount", true);

  await closeAboutDevtoolsToolbox(document, devtoolsTab, window);
  await removeTab(tab);
});

async function assertContextMenu(
  rootDocument,
  browser,
  targetSelector,
  shouldBeEnabled
) {
  if (shouldBeEnabled) {
    await assertContextMenuEnabled(rootDocument, browser, targetSelector);
  } else {
    await assertContextMenuDisabled(rootDocument, browser, targetSelector);
  }
}

async function assertContextMenuDisabled(
  rootDocument,
  browser,
  targetSelector
) {
  const contextMenu = rootDocument.getElementById("contentAreaContextMenu");
  let isPopupShown = false;
  const listener = () => {
    isPopupShown = true;
  };
  contextMenu.addEventListener("popupshown", listener);
  BrowserTestUtils.synthesizeMouseAtCenter(
    targetSelector,
    { type: "contextmenu" },
    browser
  );
  await wait(1000);
  ok(!isPopupShown, `Context menu should not be shown`);
  contextMenu.removeEventListener("popupshown", listener);
}

async function assertContextMenuEnabled(rootDocument, browser, targetSelector) {
  // Show content context menu.
  const contextMenu = rootDocument.getElementById("contentAreaContextMenu");
  const popupShownPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );
  BrowserTestUtils.synthesizeMouseAtCenter(
    targetSelector,
    { type: "contextmenu" },
    browser
  );
  await popupShownPromise;
  ok(true, `Context menu should be shown`);

  // Hide content context menu.
  const popupHiddenPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popuphidden"
  );
  contextMenu.hidePopup();
  await popupHiddenPromise;
}
