"use strict";

// This is a test for PageActions.jsm, specifically the context menus.

XPCOMUtils.defineLazyModuleGetters(this, {
  ExtensionCommon: "resource://gre/modules/ExtensionCommon.jsm",
});

// Initialization.  Must run first.
add_task(async function init() {
  // The page action urlbar button, and therefore the panel, is only shown when
  // the current tab is actionable -- i.e., a normal web page.  about:blank is
  // not, so open a new tab first thing, and close it when this test is done.
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "http://example.com/",
  });
  registerCleanupFunction(async () => {
    BrowserTestUtils.removeTab(tab);
  });

  await initPageActionsTest();
});

// Opens the context menu on a non-built-in action.  (The context menu for
// built-in actions is tested in browser_page_action_menu.js.)
add_task(async function contextMenu() {
  Services.telemetry.clearEvents();

  // Add an extension with a page action so we can test its context menu.
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Page action test",
      page_action: { show_matches: ["<all_urls>"] },
    },
    useAddonManager: "temporary",
  });
  await extension.startup();
  let actionId = ExtensionCommon.makeWidgetId(extension.id);

  // Open the main panel.
  await promiseOpenPageActionPanel();
  let panelButton = BrowserPageActions.panelButtonNodeForActionID(actionId);

  let contextMenuPromise;
  let menuItems;

  if (!gProton) {
    // Open the context menu on the action's button in the main panel.
    contextMenuPromise = promisePanelShown("pageActionContextMenu");
    EventUtils.synthesizeMouseAtCenter(panelButton, {
      type: "contextmenu",
      button: 2,
    });
    await contextMenuPromise;
    menuItems = collectContextMenuItems();
    Assert.deepEqual(
      makeMenuItemSpecs(menuItems),
      makeContextMenuItemSpecs(true)
    );

    // Click the "remove from address bar" context menu item.
    contextMenuPromise = promisePanelHidden("pageActionContextMenu");
    EventUtils.synthesizeMouseAtCenter(menuItems[0], {});
    await contextMenuPromise;

    // The action should be removed from the urlbar.
    await BrowserTestUtils.waitForCondition(() => {
      return !BrowserPageActions.urlbarButtonNodeForActionID(actionId);
    }, "Waiting for urlbar button to be removed");

    // Open the context menu again on the action's button in the panel. (The
    // panel should still be open.)
    contextMenuPromise = promisePanelShown("pageActionContextMenu");
    EventUtils.synthesizeMouseAtCenter(panelButton, {
      type: "contextmenu",
      button: 2,
    });
    await contextMenuPromise;
    menuItems = collectContextMenuItems();
    Assert.deepEqual(
      makeMenuItemSpecs(menuItems),
      makeContextMenuItemSpecs(false)
    );

    // Click the "add to address bar" context menu item.
    contextMenuPromise = promisePanelHidden("pageActionContextMenu");
    EventUtils.synthesizeMouseAtCenter(menuItems[0], {});
    await contextMenuPromise;

    // The action should be added back to the urlbar.
    await BrowserTestUtils.waitForCondition(() => {
      return BrowserPageActions.urlbarButtonNodeForActionID(actionId);
    }, "Waiting for urlbar button to be added back");
  }

  // Open the context menu again on the action's button in the panel. (The
  // panel should still be open.)
  contextMenuPromise = promisePanelShown("pageActionContextMenu");
  EventUtils.synthesizeMouseAtCenter(panelButton, {
    type: "contextmenu",
    button: 2,
  });
  await contextMenuPromise;
  menuItems = collectContextMenuItems();
  Assert.deepEqual(
    makeMenuItemSpecs(menuItems),
    makeContextMenuItemSpecs(true)
  );

  // Click the "manage extension" context menu item. about:addons should open.
  let manageItemIndex = gProton ? 0 : 2;
  contextMenuPromise = promisePanelHidden("pageActionContextMenu");
  let aboutAddonsPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    "about:addons"
  );
  EventUtils.synthesizeMouseAtCenter(menuItems[manageItemIndex], {});
  let values = await Promise.all([aboutAddonsPromise, contextMenuPromise]);
  let aboutAddonsTab = values[0];
  BrowserTestUtils.removeTab(aboutAddonsTab);

  // Wait for the urlbar button to become visible again after about:addons is
  // closed and the test tab becomes selected.
  await BrowserTestUtils.waitForCondition(() => {
    return BrowserPageActions.urlbarButtonNodeForActionID(actionId);
  }, "Waiting for urlbar button to be added back");

  let urlbarButton;

  if (!gProton) {
    // Open the context menu on the action's urlbar button.
    urlbarButton = BrowserPageActions.urlbarButtonNodeForActionID(actionId);
    contextMenuPromise = promisePanelShown("pageActionContextMenu");
    EventUtils.synthesizeMouseAtCenter(urlbarButton, {
      type: "contextmenu",
      button: 2,
    });
    await contextMenuPromise;
    menuItems = collectContextMenuItems();
    Assert.deepEqual(
      makeMenuItemSpecs(menuItems),
      makeContextMenuItemSpecs(true)
    );

    // Click the "remove from address bar" context menu item.
    contextMenuPromise = promisePanelHidden("pageActionContextMenu");
    EventUtils.synthesizeMouseAtCenter(menuItems[0], {});
    await contextMenuPromise;

    // The action should be removed from the urlbar.
    await BrowserTestUtils.waitForCondition(() => {
      return !BrowserPageActions.urlbarButtonNodeForActionID(actionId);
    }, "Waiting for urlbar button to be removed");

    // Open the panel and then open the context menu on the action's button.
    await promiseOpenPageActionPanel();
    contextMenuPromise = promisePanelShown("pageActionContextMenu");
    EventUtils.synthesizeMouseAtCenter(panelButton, {
      type: "contextmenu",
      button: 2,
    });
    await contextMenuPromise;
    menuItems = collectContextMenuItems();
    Assert.deepEqual(
      makeMenuItemSpecs(menuItems),
      makeContextMenuItemSpecs(false)
    );

    // Click the "add to address bar" context menu item.
    contextMenuPromise = promisePanelHidden("pageActionContextMenu");
    EventUtils.synthesizeMouseAtCenter(menuItems[0], {});
    await contextMenuPromise;

    // The action should be added back to the urlbar.
    await BrowserTestUtils.waitForCondition(() => {
      return BrowserPageActions.urlbarButtonNodeForActionID(actionId);
    }, "Waiting for urlbar button to be added back");
  }

  // Open the context menu on the action's urlbar button.
  urlbarButton = BrowserPageActions.urlbarButtonNodeForActionID(actionId);
  contextMenuPromise = promisePanelShown("pageActionContextMenu");
  EventUtils.synthesizeMouseAtCenter(urlbarButton, {
    type: "contextmenu",
    button: 2,
  });
  await contextMenuPromise;
  menuItems = collectContextMenuItems();
  Assert.deepEqual(
    makeMenuItemSpecs(menuItems),
    makeContextMenuItemSpecs(true)
  );

  // Click the "manage" context menu item. about:addons should open.
  contextMenuPromise = promisePanelHidden("pageActionContextMenu");
  aboutAddonsPromise = BrowserTestUtils.waitForNewTab(gBrowser, "about:addons");
  EventUtils.synthesizeMouseAtCenter(menuItems[manageItemIndex], {});
  values = await Promise.all([aboutAddonsPromise, contextMenuPromise]);
  aboutAddonsTab = values[0];
  BrowserTestUtils.removeTab(aboutAddonsTab);

  // Wait for the urlbar button to become visible again after about:addons is
  // closed and the test tab becomes selected.
  await BrowserTestUtils.waitForCondition(() => {
    return BrowserPageActions.urlbarButtonNodeForActionID(actionId);
  }, "Waiting for urlbar button to be added back");

  // Open the context menu on the action's urlbar button.
  urlbarButton = BrowserPageActions.urlbarButtonNodeForActionID(actionId);
  contextMenuPromise = promisePanelShown("pageActionContextMenu");
  EventUtils.synthesizeMouseAtCenter(urlbarButton, {
    type: "contextmenu",
    button: 2,
  });
  await contextMenuPromise;
  menuItems = collectContextMenuItems();
  Assert.deepEqual(
    makeMenuItemSpecs(menuItems),
    makeContextMenuItemSpecs(true)
  );

  // Below we'll click the "remove extension" context menu item, which first
  // opens a prompt using the prompt service and requires confirming the prompt.
  // Set up a mock prompt service that returns 0 to indicate that the user
  // pressed the OK button.
  let { prompt } = Services;
  let promptService = {
    QueryInterface: ChromeUtils.generateQI(["nsIPromptService"]),
    confirmEx() {
      return 0;
    },
  };
  Services.prompt = promptService;
  registerCleanupFunction(() => {
    Services.prompt = prompt;
  });

  // Now click the "remove extension" context menu item.
  let removeItemIndex = manageItemIndex + 1;
  contextMenuPromise = promisePanelHidden("pageActionContextMenu");
  let promiseUninstalled = promiseAddonUninstalled(extension.id);
  EventUtils.synthesizeMouseAtCenter(menuItems[removeItemIndex], {});
  await contextMenuPromise;
  await promiseUninstalled;
  let addonId = extension.id;
  await extension.unload();
  Services.prompt = prompt;

  // Check the telemetry was collected properly.
  let snapshot = Services.telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
    true
  );
  ok(
    snapshot.parent && !!snapshot.parent.length,
    "Got parent telemetry events in the snapshot"
  );
  let relatedEvents = snapshot.parent
    .filter(
      ([timestamp, category, method]) =>
        category == "addonsManager" && method == "action"
    )
    .map(relatedEvent => relatedEvent.slice(3, 6));
  Assert.deepEqual(relatedEvents, [
    ["pageAction", null, { addonId, action: "manage" }],
    ["pageAction", null, { addonId, action: "manage" }],
    ["pageAction", "accepted", { addonId, action: "uninstall" }],
  ]);

  // urlbar tests that run after this one can break if the mouse is left over
  // the area where the urlbar popup appears, which seems to happen due to the
  // above synthesized mouse events.  Move it over the urlbar.
  EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, { type: "mousemove" });
  gURLBar.focus();
});

// The context menu shouldn't open on separators in the panel.
add_task(async function contextMenuOnSeparator() {
  // Add a non-built-in action so the built-in separator will appear in the
  // panel.
  let action = PageActions.addAction(
    new PageActions.Action({
      id: "contextMenuOnSeparator",
      title: "contextMenuOnSeparator",
      pinnedToUrlbar: true,
    })
  );

  // Open the panel and get the built-in separator.
  await promiseOpenPageActionPanel();
  let separator = BrowserPageActions.panelButtonNodeForActionID(
    PageActions.ACTION_ID_BUILT_IN_SEPARATOR
  );
  Assert.ok(separator, "The built-in separator should be in the panel");

  // Context-click it.  popupshowing should be fired, but by the time the event
  // reaches this listener, preventDefault should have been called on it.
  let showingPromise = BrowserTestUtils.waitForEvent(
    document.getElementById("pageActionContextMenu"),
    "popupshowing",
    false
  );
  EventUtils.synthesizeMouseAtCenter(separator, {
    type: "contextmenu",
    button: 2,
  });
  let event = await showingPromise;
  Assert.ok(
    event.defaultPrevented,
    "defaultPrevented should be true on popupshowing event"
  );

  // Click the main button to hide the main panel.
  EventUtils.synthesizeMouseAtCenter(BrowserPageActions.mainButtonNode, {});
  await promisePageActionPanelHidden();

  action.remove();

  // urlbar tests that run after this one can break if the mouse is left over
  // the area where the urlbar popup appears, which seems to happen due to the
  // above synthesized mouse events.  Move it over the urlbar.
  EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, { type: "mousemove" });
  gURLBar.focus();
});

function collectContextMenuItems() {
  let contextMenu = document.getElementById("pageActionContextMenu");
  return Array.prototype.filter.call(contextMenu.children, node => {
    return window.getComputedStyle(node).visibility == "visible";
  });
}

function makeMenuItemSpecs(elements) {
  return elements.map(e =>
    e.localName == "menuseparator" ? {} : { label: e.label }
  );
}

function makeContextMenuItemSpecs(actionInUrlbar = false) {
  let items = [
    { label: "Manage Extension\u2026" },
    { label: "Remove Extension" },
  ];
  if (!gProton) {
    items.unshift(
      {
        label: actionInUrlbar
          ? "Remove from Address Bar"
          : "Add to Address Bar",
      },
      {} // separator
    );
  }
  return items;
}

function promiseAddonUninstalled(addonId) {
  return new Promise(resolve => {
    let listener = {};
    listener.onUninstalled = addon => {
      if (addon.id == addonId) {
        AddonManager.removeAddonListener(listener);
        resolve();
      }
    };
    AddonManager.addAddonListener(listener);
  });
}
