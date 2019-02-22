/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-collapsibilities.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-collapsibilities.js", this);

/**
 * Test the status of menu items when open about:devtools-toolbox.
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
  info("Check whether the menu items are disabled");
  const rootDocument = devtoolsTab.ownerDocument;
  await assertMenusItems(rootDocument, false);

  info("Force to select about:debugging page");
  gBrowser.selectedTab = tab;
  info("Check whether the menu items are enabled");
  await assertMenusItems(rootDocument, true);

  await closeAboutDevtoolsToolbox(devtoolsTab, window);
  await removeTab(tab);
});

async function assertMenusItems(rootDocument, shouldBeEnabled) {
  const menuItem = rootDocument.getElementById("menu_devToolbox");
  // Wait for hidden attribute changed since the menu items will update asynchronously.
  await waitUntil(() => menuItem.hidden === !shouldBeEnabled);

  assertMenuItem(rootDocument, "menu_devToolbox", shouldBeEnabled);

  for (const toolDefinition of gDevTools.getToolDefinitionArray()) {
    if (!toolDefinition.inMenu) {
      continue;
    }

    assertMenuItem(rootDocument, "menuitem_" + toolDefinition.id, shouldBeEnabled);
  }
}

function assertMenuItem(rootDocument, menuItemId, shouldBeEnabled) {
  const menuItem = rootDocument.getElementById(menuItemId);
  is(menuItem.hidden, !shouldBeEnabled,
     `"hidden" attribute of menu item(${ menuItemId }) should be correct`);
}
