/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

async function openContextMenuInOptionsPage(optionsBrowser) {
  let contentAreaContextMenu = document.getElementById("contentAreaContextMenu");
  let popupShownPromise = BrowserTestUtils.waitForEvent(contentAreaContextMenu, "popupshown");

  info("Trigger context menu in the extension options page");

  // Instead of BrowserTestUtils.synthesizeMouseAtCenter, we are dispatching a contextmenu
  // event directly on the target element to prevent intermittent failures on debug builds
  // (especially linux32-debug), see Bug 1519808 for a rationale.
  ContentTask.spawn(optionsBrowser, null, () => {
    let el = content.document.querySelector("a");
    el.dispatchEvent(new content.MouseEvent("contextmenu", {
      bubbles: true,
      cancelable: true,
      view: el.ownerGlobal,
    }));
  });

  info("Wait the context menu to be shown");
  await popupShownPromise;

  return contentAreaContextMenu;
}

async function contextMenuClosed(contextMenu) {
  info("Wait context menu popup to be closed");
  await closeContextMenu(contextMenu);
  is(contextMenu.state, "closed", "The context menu popup has been closed");
}

add_task(async function test_tab_options_popups() {
  async function backgroundScript() {
    browser.menus.onShown.addListener(info => {
      browser.test.sendMessage("extension-menus-onShown", info);
    });

    await browser.menus.create({id: "sidebaronly", title: "sidebaronly", viewTypes: ["sidebar"]});
    await browser.menus.create({id: "tabonly", title: "tabonly", viewTypes: ["tab"]});
    await browser.menus.create({id: "anypage", title: "anypage"});

    browser.runtime.openOptionsPage();
  }

  function optionsScript() {
    browser.test.sendMessage("options-page:loaded", document.documentURI);
  }

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",

    manifest: {
      "permissions": ["tabs", "menus"],
      "options_ui": {
        "page": "options.html",
      },
    },
    files: {
      "options.html": `<!DOCTYPE html>
        <html>
          <head>
            <meta charset="utf-8">
            <script src="options.js" type="text/javascript"></script>
          </head>
          <body style="height: 100px;">
            <h1>Extensions Options</h1>
            <a href="http://mochi.test:8888/">options page link</a>
          </body>
        </html>`,
      "options.js": optionsScript,
    },
    background: backgroundScript,
  });

  const aboutAddonsTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:addons");

  await extension.startup();

  const pageUrl = await extension.awaitMessage("options-page:loaded");

  const optionsBrowser = getInlineOptionsBrowser(gBrowser.selectedBrowser);

  const contentAreaContextMenu = await openContextMenuInOptionsPage(optionsBrowser);

  let contextMenuItemIds = [
    "context-openlinkintab",
    "context-openlinkprivate",
    "context-copylink",
  ];

  // Test that the "open link in container" menu is available if the containers are enabled
  // (which is the default on Nightly, but not on Beta).
  if (Services.prefs.getBoolPref("privacy.userContext.enabled")) {
    contextMenuItemIds.push("context-openlinkinusercontext-menu");
  }

  for (const itemID of contextMenuItemIds) {
    const item = contentAreaContextMenu.querySelector(`#${itemID}`);

    ok(!item.hidden, `${itemID} should not be hidden`);
    ok(!item.disabled, `${itemID} should not be disabled`);
  }

  const menuDetails = await extension.awaitMessage("extension-menus-onShown");

  isnot(menuDetails.targetElementId, undefined, "Got a targetElementId in the menu details");
  delete menuDetails.targetElementId;


  Assert.deepEqual(menuDetails, {
    menuIds: ["anypage"],
    contexts: ["link", "all"],
    viewType: undefined,
    frameId: 0,
    editable: false,
    linkText: "options page link",
    linkUrl: "http://mochi.test:8888/",
    pageUrl,
  }, "Got the expected menu details from menus.onShown");

  await contextMenuClosed(contentAreaContextMenu);

  BrowserTestUtils.removeTab(aboutAddonsTab);

  await extension.unload();
});

add_task(async function overrideContext_in_options_page() {
  function optionsScript() {
    document.addEventListener("contextmenu", () => {
      browser.menus.overrideContext({});
      browser.test.sendMessage("contextmenu-overridden");
    }, {once: true});
    browser.test.sendMessage("options-page:loaded");
  }

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      "permissions": ["tabs", "menus", "menus.overrideContext"],
      "options_ui": {
        "page": "options.html",
      },
    },
    files: {
      "options.html": `<!DOCTYPE html>
        <html>
          <head>
            <meta charset="utf-8">
            <script src="options.js" type="text/javascript"></script>
          </head>
          <body style="height: 100px;">
            <h1>Extensions Options</h1>
            <a href="http://mochi.test:8888/">options page link</a>
          </body>
        </html>`,
      "options.js": optionsScript,
    },
    async background() {
      // Expected to match and be shown.
      await new Promise(resolve => {
        browser.menus.create({id: "bg_1_1", title: "bg_1_1"});
        browser.menus.create({id: "bg_1_2", title: "bg_1_2"});
        // Expected to not match and be hidden.
        browser.menus.create(
          {id: "bg_1_3", title: "bg_1_3", targetUrlPatterns: ["*://nomatch/*"]},
          // menus.create returns a number and gets a callback, the order
          // is deterministic and so we just need to wait for the last one.
          resolve);
      });
      browser.runtime.openOptionsPage();
    },
  });

  const aboutAddonsTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:addons");

  await extension.startup();
  await extension.awaitMessage("options-page:loaded");

  const optionsBrowser = getInlineOptionsBrowser(gBrowser.selectedBrowser);
  const contentAreaContextMenu = await openContextMenuInOptionsPage(optionsBrowser);

  await extension.awaitMessage("contextmenu-overridden");

  const allVisibleMenuItems = Array.from(contentAreaContextMenu.children).filter(elem => {
    return !elem.hidden;
  }).map(elem => elem.id);

  Assert.deepEqual(allVisibleMenuItems, [
    `${makeWidgetId(extension.id)}-menuitem-_bg_1_1`,
    `${makeWidgetId(extension.id)}-menuitem-_bg_1_2`,
  ], "Expected only extension menu items");

  await contextMenuClosed(contentAreaContextMenu);

  BrowserTestUtils.removeTab(aboutAddonsTab);

  await extension.unload();
});
