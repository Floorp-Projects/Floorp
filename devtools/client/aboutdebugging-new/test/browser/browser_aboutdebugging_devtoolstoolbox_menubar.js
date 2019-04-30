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
  await selectThisFirefoxPage(document, window.AboutDebugging.store);
  const { devtoolsTab, devtoolsWindow } =
    await openAboutDevtoolsToolbox(document, tab, window);

  info("Check whether the menu items are disabled");
  const rootDocument = devtoolsTab.ownerDocument;
  await assertMenusItems(rootDocument, false);

  info("Select the inspector");
  const toolbox = getToolbox(devtoolsWindow);
  await toolbox.selectTool("inspector");

  info("Click on the console item");
  const onConsoleLoaded = toolbox.once("webconsole-ready");
  const webconsoleMenuItem = rootDocument.getElementById("menuitem_webconsole");
  webconsoleMenuItem.click();

  info("Wait until about:devtools-toolbox switches to the console");
  await onConsoleLoaded;

  info("Force to select about:debugging page");
  gBrowser.selectedTab = tab;
  info("Check whether the menu items are enabled");
  await assertMenusItems(rootDocument, true);

  await closeAboutDevtoolsToolbox(document, devtoolsTab, window);
  await removeTab(tab);
});

async function assertMenusItems(rootDocument, shouldBeEnabled) {
  info("Wait for the Toggle Tools menu-item hidden attribute to change");
  const menuItem = rootDocument.getElementById("menu_devToolbox");
  await waitUntil(() => menuItem.hidden === !shouldBeEnabled);

  info("Check that the state of the Toggle Tools menu-item depends on the page");
  assertMenuItem(rootDocument, "menu_devToolbox", shouldBeEnabled);

  info("Check that the tools menu-items are always enabled regardless of the page");
  for (const toolDefinition of gDevTools.getToolDefinitionArray()) {
    if (!toolDefinition.inMenu) {
      continue;
    }

    assertMenuItem(rootDocument, "menuitem_" + toolDefinition.id, true);
  }
}

function assertMenuItem(rootDocument, menuItemId, shouldBeEnabled) {
  const menuItem = rootDocument.getElementById(menuItemId);
  is(menuItem.hidden, !shouldBeEnabled,
     `"hidden" attribute of menu item(${ menuItemId }) should be correct`);
}
