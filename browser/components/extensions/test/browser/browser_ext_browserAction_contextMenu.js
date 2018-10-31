/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

let extData = {
  manifest: {
    "permissions": ["contextMenus"],
    "browser_action": {
      "default_popup": "popup.html",
    },
  },
  useAddonManager: "temporary",

  files: {
    "popup.html": `
      <!DOCTYPE html>
      <html>
      <head><meta charset="utf-8"/>
      </head>
      <body>
      <span id="text">A Test Popup</span>
      <img id="testimg" src="data:image/svg+xml,<svg></svg>" height="10" width="10">
      </body></html>
    `,
  },

  background: function() {
    browser.contextMenus.create({
      id: "clickme-page",
      title: "Click me!",
      contexts: ["all"],
    });
  },
};

let contextMenuItems = {
  "context-navigation": "hidden",
  "context-sep-navigation": "hidden",
  "context-viewsource": "",
  "context-viewinfo": "disabled",
  "inspect-separator": "hidden",
  "context-inspect": "hidden",
  "context-inspect-a11y": "hidden",
  "context-bookmarkpage": "hidden",
};

add_task(async function browseraction_popup_contextmenu() {
  let extension = ExtensionTestUtils.loadExtension(extData);
  await extension.startup();

  await clickBrowserAction(extension, window);

  let contentAreaContextMenu = await openContextMenuInPopup(extension);
  let item = contentAreaContextMenu.getElementsByAttribute("label", "Click me!");
  is(item.length, 1, "contextMenu item for page was found");
  await closeContextMenu(contentAreaContextMenu);

  await extension.unload();
});

add_task(async function browseraction_popup_contextmenu_hidden_items() {
  let extension = ExtensionTestUtils.loadExtension(extData);
  await extension.startup();

  await clickBrowserAction(extension);

  let contentAreaContextMenu = await openContextMenuInPopup(extension, "#text");

  let item, state;
  for (const itemID in contextMenuItems) {
    item = contentAreaContextMenu.querySelector(`#${itemID}`);
    state = contextMenuItems[itemID];

    if (state !== "") {
      ok(item[state], `${itemID} is ${state}`);

      if (state !== "hidden") {
        ok(!item.hidden, `Disabled ${itemID} is not hidden`);
      }
    } else {
      ok(!item.hidden, `${itemID} is not hidden`);
      ok(!item.disabled, `${itemID} is not disabled`);
    }
  }

  await closeContextMenu(contentAreaContextMenu);

  await extension.unload();
});

add_task(async function browseraction_popup_image_contextmenu() {
  let extension = ExtensionTestUtils.loadExtension(extData);
  await extension.startup();

  await clickBrowserAction(extension);

  let contentAreaContextMenu = await openContextMenuInPopup(extension, "#testimg");

  let item = contentAreaContextMenu.querySelector("#context-viewimageinfo");
  ok(!item.hidden);
  ok(item.disabled);

  await closeContextMenu(contentAreaContextMenu);

  await extension.unload();
});

function openContextMenu(menuId, targetId) {
  return openChromeContextMenu(menuId, "#" + CSS.escape(targetId));
}

function waitForElementShown(element) {
  let win = element.ownerGlobal;
  let dwu = win.windowUtils;
  return BrowserTestUtils.waitForCondition(() => {
    info("Waiting for overflow button to have non-0 size");
    let bounds = dwu.getBoundsWithoutFlushing(element);
    return bounds.width > 0 && bounds.height > 0;
  });
}

add_task(async function browseraction_contextmenu_manage_extension() {
  let id = "addon_id@example.com";
  let buttonId = `${makeWidgetId(id)}-browser-action`;
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "applications": {
        "gecko": {id},
      },
      "browser_action": {},
      "options_ui": {
        "page": "options.html",
      },
    },
    useAddonManager: "temporary",
    files: {
      "options.html": `<script src="options.js"></script>`,
      "options.js": `browser.test.sendMessage("options-loaded");`,
    },
  });

  function checkVisibility(menu, visible) {
    let removeExtension = menu.querySelector(".customize-context-removeExtension");
    let manageExtension = menu.querySelector(".customize-context-manageExtension");
    let separator = removeExtension.nextElementSibling;

    info(`Check visibility`);
    let expected = visible ? "visible" : "hidden";
    is(removeExtension.hidden, !visible, `Remove Extension should be ${expected}`);
    is(manageExtension.hidden, !visible, `Manage Extension should be ${expected}`);
    is(separator.hidden, !visible, `Separator after Manage Extension should be ${expected}`);
  }

  async function testContextMenu(menuId, customizing) {
    info(`Open browserAction context menu in ${menuId}`);
    let menu = await openContextMenu(menuId, buttonId);
    await checkVisibility(menu, true);

    info(`Choosing 'Manage Extension' in ${menuId} should load options`);
    let optionsLoaded = extension.awaitMessage("options-loaded");
    let manageExtension = menu.querySelector(".customize-context-manageExtension");
    await closeChromeContextMenu(menuId, manageExtension);
    await optionsLoaded;

    info(`Remove the opened tab, and await customize mode to be restored if necessary`);
    let tab = gBrowser.selectedTab;
    is(tab.linkedBrowser.currentURI.spec, "about:addons");
    if (customizing) {
      let customizationReady = BrowserTestUtils.waitForEvent(gNavToolbox, "customizationready");
      gBrowser.removeTab(tab);
      await customizationReady;
    } else {
      gBrowser.removeTab(tab);
    }

    return menu;
  }

  async function main(customizing) {
    if (customizing) {
      info("Enter customize mode");
      let customizationReady = BrowserTestUtils.waitForEvent(gNavToolbox, "customizationready");
      gCustomizeMode.enter();
      await customizationReady;
    }

    info("Test toolbar context menu in browserAction");
    let toolbarCtxMenu = await testContextMenu("toolbar-context-menu", customizing);

    info("Check toolbar context menu in another button");
    let otherButtonId = "home-button";
    await openContextMenu(toolbarCtxMenu.id, otherButtonId);
    checkVisibility(toolbarCtxMenu, false);
    toolbarCtxMenu.hidePopup();

    info("Check toolbar context menu without triggerNode");
    toolbarCtxMenu.openPopup();
    checkVisibility(toolbarCtxMenu, false);
    toolbarCtxMenu.hidePopup();

    info("Pin the browserAction and another button to the overflow menu");
    CustomizableUI.addWidgetToArea(buttonId, CustomizableUI.AREA_FIXED_OVERFLOW_PANEL);
    CustomizableUI.addWidgetToArea(otherButtonId, CustomizableUI.AREA_FIXED_OVERFLOW_PANEL);

    info("Wait until the overflow menu is ready");
    let overflowButton = document.getElementById("nav-bar-overflow-button");
    let icon = document.getAnonymousElementByAttribute(overflowButton, "class", "toolbarbutton-icon");
    await waitForElementShown(icon);

    if (!customizing) {
      info("Open overflow menu");
      let menu = document.getElementById("widget-overflow");
      let shown = BrowserTestUtils.waitForEvent(menu, "popupshown");
      overflowButton.click();
      await shown;
    }

    info("Check overflow menu context menu in another button");
    let overflowMenuCtxMenu = await openContextMenu("customizationPanelItemContextMenu", otherButtonId);
    checkVisibility(overflowMenuCtxMenu, false);
    overflowMenuCtxMenu.hidePopup();

    info("Test overflow menu context menu in browserAction");
    await testContextMenu(overflowMenuCtxMenu.id, customizing);

    info("Restore initial state");
    CustomizableUI.addWidgetToArea(buttonId, CustomizableUI.AREA_NAVBAR);
    CustomizableUI.addWidgetToArea(otherButtonId, CustomizableUI.AREA_NAVBAR);

    if (customizing) {
      info("Exit customize mode");
      let afterCustomization = BrowserTestUtils.waitForEvent(gNavToolbox, "aftercustomization");
      gCustomizeMode.exit();
      await afterCustomization;
    }
  }

  await extension.startup();

  info("Add a dummy tab to prevent about:addons from being loaded in the initial about:blank tab");
  let dummyTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com", true, true);

  info("Run tests in normal mode");
  await main(false);

  info("Run tests in customize mode");
  await main(true);

  info("Close the dummy tab and finish");
  gBrowser.removeTab(dummyTab);
  await extension.unload();
});

add_task(async function browseraction_contextmenu_remove_extension() {
  let id = "addon_id@example.com";
  let name = "Awesome Add-on";
  let buttonId = `${makeWidgetId(id)}-browser-action`;
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name,
      "applications": {
        "gecko": {id},
      },
      "browser_action": {},
    },
    useAddonManager: "temporary",
  });
  let brand = Services.strings.createBundle("chrome://branding/locale/brand.properties")
    .GetStringFromName("brandShorterName");
  let {prompt} = Services;
  let promptService = {
    _response: 1,
    QueryInterface: ChromeUtils.generateQI([Ci.nsIPromptService]),
    confirmEx: function(...args) {
      promptService._confirmExArgs = args;
      return promptService._response;
    },
  };
  Services.prompt = promptService;
  registerCleanupFunction(() => {
    Services.prompt = prompt;
  });

  async function testContextMenu(menuId, customizing) {
    info(`Open browserAction context menu in ${menuId}`);
    let menu = await openContextMenu(menuId, buttonId);

    info(`Choosing 'Remove Extension' in ${menuId} should show confirm dialog`);
    let removeExtension = menu.querySelector(".customize-context-removeExtension");
    await closeChromeContextMenu(menuId, removeExtension);
    is(promptService._confirmExArgs[1], `Remove ${name}`);
    is(promptService._confirmExArgs[2], `Remove ${name} from ${brand}?`);
    is(promptService._confirmExArgs[4], "Remove");
    return menu;
  }

  async function main(customizing) {
    if (customizing) {
      info("Enter customize mode");
      let customizationReady = BrowserTestUtils.waitForEvent(gNavToolbox, "customizationready");
      gCustomizeMode.enter();
      await customizationReady;
    }

    info("Test toolbar context menu in browserAction");
    await testContextMenu("toolbar-context-menu", customizing);

    info("Pin the browserAction and another button to the overflow menu");
    CustomizableUI.addWidgetToArea(buttonId, CustomizableUI.AREA_FIXED_OVERFLOW_PANEL);

    info("Wait until the overflow menu is ready");
    let overflowButton = document.getElementById("nav-bar-overflow-button");
    let icon = document.getAnonymousElementByAttribute(overflowButton, "class", "toolbarbutton-icon");
    await waitForElementShown(icon);

    if (!customizing) {
      info("Open overflow menu");
      let menu = document.getElementById("widget-overflow");
      let shown = BrowserTestUtils.waitForEvent(menu, "popupshown");
      overflowButton.click();
      await shown;
    }

    info("Test overflow menu context menu in browserAction");
    await testContextMenu("customizationPanelItemContextMenu", customizing);

    info("Restore initial state");
    CustomizableUI.addWidgetToArea(buttonId, CustomizableUI.AREA_NAVBAR);

    if (customizing) {
      info("Exit customize mode");
      let afterCustomization = BrowserTestUtils.waitForEvent(gNavToolbox, "aftercustomization");
      gCustomizeMode.exit();
      await afterCustomization;
    }
  }

  await extension.startup();

  info("Run tests in normal mode");
  await main(false);

  info("Run tests in customize mode");
  await main(true);

  let addon = await AddonManager.getAddonByID(id);
  ok(addon, "Addon is still installed");

  promptService._response = 0;
  let uninstalled = new Promise((resolve) => {
    AddonManager.addAddonListener({
      onUninstalled(addon) {
        is(addon.id, id, "The expected add-on has been uninstalled");
        AddonManager.removeAddonListener(this);
        resolve();
      },
    });
  });
  await testContextMenu("toolbar-context-menu", false);
  await uninstalled;

  addon = await AddonManager.getAddonByID(id);
  ok(!addon, "Addon has been uninstalled");

  await extension.unload();
});
