"use strict";

// This is a test for PageActions.jsm, specifically the context menus.

// Initialization.  Must run first.
add_task(async function init() {
  // The page action urlbar button, and therefore the panel, is only shown when
  // the current tab is actionable -- i.e., a normal web page.  about:blank is
  // not, so open a new tab first thing, and close it when this test is done.
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "http://example.com/",
  });

  await disableNonReleaseActions();
  registerCleanupFunction(async () => {
    BrowserTestUtils.removeTab(tab);
  });

  // Ensure screenshots is really disabled (bug 1498738)
  const addon = await AddonManager.getAddonByID("screenshots@mozilla.org");
  await addon.disable({ allowSystemAddons: true });
});

// Opens the context menu on a non-built-in action.  (The context menu for
// built-in actions is tested in browser_page_action_menu.js.)
add_task(async function contextMenu() {
  Services.telemetry.clearEvents();

  // Add a test action.
  let action = PageActions.addAction(
    new PageActions.Action({
      id: "test-contextMenu",
      title: "Test contextMenu",
      pinnedToUrlbar: true,
    })
  );

  // Open the panel and then open the context menu on the action's item.
  await promiseOpenPageActionPanel();
  let panelButton = BrowserPageActions.panelButtonNodeForActionID(action.id);
  let contextMenuPromise = promisePanelShown("pageActionContextMenu");
  EventUtils.synthesizeMouseAtCenter(panelButton, {
    type: "contextmenu",
    button: 2,
  });
  await contextMenuPromise;

  // The context menu should show the "remove" item and the "manage" item. The
  // 4th item is "remove extension" but it is hidden in this test case because
  // the page action isn't bound to an addon.  Click the "remove" item.
  let menuItems = collectContextMenuItems();
  Assert.equal(menuItems.length, 4, "Context menu has 4 children");
  Assert.equal(
    menuItems[0].label,
    "Remove from Address Bar",
    "Context menu is in the 'remove' state"
  );
  Assert.equal(
    menuItems[1].localName,
    "menuseparator",
    "menuseparator is present"
  );
  Assert.equal(
    menuItems[2].label,
    "Manage Extension\u2026",
    "'Manage' item is present"
  );
  Assert.equal(
    menuItems[3].label,
    "Remove Extension",
    "'Remove' item is present"
  );
  Assert.ok(menuItems[3].hidden, "'Remove' item is hidden");

  contextMenuPromise = promisePanelHidden("pageActionContextMenu");
  EventUtils.synthesizeMouseAtCenter(menuItems[0], {});
  await contextMenuPromise;

  // The action should be removed from the urlbar.
  await BrowserTestUtils.waitForCondition(() => {
    return !BrowserPageActions.urlbarButtonNodeForActionID(action.id);
  }, "Waiting for urlbar button to be removed");

  // Open the context menu again on the action's button in the panel.  (The
  // panel should still be open.)
  contextMenuPromise = promisePanelShown("pageActionContextMenu");
  EventUtils.synthesizeMouseAtCenter(panelButton, {
    type: "contextmenu",
    button: 2,
  });
  await contextMenuPromise;

  // The context menu should show the "add" item and the "manage" item. The 4th
  // item is "remove extension" but it is hidden.  Click the "add" item.
  menuItems = collectContextMenuItems();
  Assert.equal(menuItems.length, 4, "Context menu has 4 children");
  Assert.equal(
    menuItems[0].label,
    "Add to Address Bar",
    "Context menu is in the 'add' state"
  );
  Assert.equal(
    menuItems[1].localName,
    "menuseparator",
    "menuseparator is present"
  );
  Assert.equal(
    menuItems[2].label,
    "Manage Extension\u2026",
    "'Manage' item is present"
  );
  Assert.equal(
    menuItems[3].label,
    "Remove Extension",
    "'Remove' item is present"
  );
  Assert.ok(menuItems[3].hidden, "'Remove' item is hidden");

  contextMenuPromise = promisePanelHidden("pageActionContextMenu");
  EventUtils.synthesizeMouseAtCenter(menuItems[0], {});
  await contextMenuPromise;

  // The action should be added back to the urlbar.
  await BrowserTestUtils.waitForCondition(() => {
    return BrowserPageActions.urlbarButtonNodeForActionID(action.id);
  }, "Waiting for urlbar button to be added back");

  // Open the context menu again on the action's button in the panel. (The
  // panel should still be open.)
  contextMenuPromise = promisePanelShown("pageActionContextMenu");
  EventUtils.synthesizeMouseAtCenter(panelButton, {
    type: "contextmenu",
    button: 2,
  });
  await contextMenuPromise;

  // The context menu should show the "remove" item and the "manage" item. The
  // 4th item is "remove extension" but it is hidden.  Click the "manage" item.
  // about:addons should open.
  menuItems = collectContextMenuItems();
  Assert.equal(menuItems.length, 4, "Context menu has 4 children");
  Assert.equal(
    menuItems[0].label,
    "Remove from Address Bar",
    "Context menu is in the 'remove' state"
  );
  Assert.equal(
    menuItems[1].localName,
    "menuseparator",
    "menuseparator is present"
  );
  Assert.equal(
    menuItems[2].label,
    "Manage Extension\u2026",
    "'Manage' item is present"
  );
  Assert.equal(
    menuItems[3].label,
    "Remove Extension",
    "'Remove' item is present"
  );
  Assert.ok(menuItems[3].hidden, "'Remove' item is hidden");

  // Click the "manage" item, about:addons should open.
  contextMenuPromise = promisePanelHidden("pageActionContextMenu");
  let aboutAddonsPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    "about:addons"
  );
  EventUtils.synthesizeMouseAtCenter(menuItems[2], {});
  let values = await Promise.all([aboutAddonsPromise, contextMenuPromise]);
  let aboutAddonsTab = values[0];
  BrowserTestUtils.removeTab(aboutAddonsTab);

  // Open the context menu on the action's urlbar button.
  let urlbarButton = BrowserPageActions.urlbarButtonNodeForActionID(action.id);
  contextMenuPromise = promisePanelShown("pageActionContextMenu");
  EventUtils.synthesizeMouseAtCenter(urlbarButton, {
    type: "contextmenu",
    button: 2,
  });
  await contextMenuPromise;

  // The context menu should show the "remove" item and the "manage" item. The
  // 4th item is "remove extension" but it is hidden.  Click the "manage" item.
  // about:addons should open.
  menuItems = collectContextMenuItems();
  Assert.equal(menuItems.length, 4, "Context menu has 4 children");
  Assert.equal(
    menuItems[0].label,
    "Remove from Address Bar",
    "Context menu is in the 'remove' state"
  );
  Assert.equal(
    menuItems[1].localName,
    "menuseparator",
    "menuseparator is present"
  );
  Assert.equal(
    menuItems[2].label,
    "Manage Extension\u2026",
    "'Manage' item is present"
  );
  Assert.equal(
    menuItems[3].label,
    "Remove Extension",
    "'Remove' item is present"
  );
  Assert.ok(menuItems[3].hidden, "'Remove' item is hidden");

  contextMenuPromise = promisePanelHidden("pageActionContextMenu");
  EventUtils.synthesizeMouseAtCenter(menuItems[0], {});
  await contextMenuPromise;

  // The action should be removed from the urlbar.
  await BrowserTestUtils.waitForCondition(() => {
    return !BrowserPageActions.urlbarButtonNodeForActionID(action.id);
  }, "Waiting for urlbar button to be removed");

  // Open the panel and then open the context menu on the action's item.
  await promiseOpenPageActionPanel();
  contextMenuPromise = promisePanelShown("pageActionContextMenu");
  EventUtils.synthesizeMouseAtCenter(panelButton, {
    type: "contextmenu",
    button: 2,
  });
  await contextMenuPromise;

  // The context menu should show the "remove" item and the "manage" item. The
  // 4th item is "remove extension" but it is hidden.  Click the "remove" item.
  menuItems = collectContextMenuItems();
  Assert.equal(menuItems.length, 4, "Context menu has 4 children");
  Assert.equal(
    menuItems[0].label,
    "Add to Address Bar",
    "Context menu is in the 'add' state"
  );
  Assert.equal(
    menuItems[1].localName,
    "menuseparator",
    "menuseparator is present"
  );
  Assert.equal(
    menuItems[2].label,
    "Manage Extension\u2026",
    "'Manage' item is present"
  );
  Assert.equal(
    menuItems[3].label,
    "Remove Extension",
    "'Remove' item is present"
  );
  Assert.ok(menuItems[3].hidden, "'Remove' item is hidden");

  contextMenuPromise = promisePanelHidden("pageActionContextMenu");
  EventUtils.synthesizeMouseAtCenter(menuItems[0], {});
  await contextMenuPromise;

  // The action should be added back to the urlbar.
  await BrowserTestUtils.waitForCondition(() => {
    return BrowserPageActions.urlbarButtonNodeForActionID(action.id);
  }, "Waiting for urlbar button to be added back");

  // Open the context menu on the action's urlbar button.
  urlbarButton = BrowserPageActions.urlbarButtonNodeForActionID(action.id);
  contextMenuPromise = promisePanelShown("pageActionContextMenu");
  EventUtils.synthesizeMouseAtCenter(urlbarButton, {
    type: "contextmenu",
    button: 2,
  });
  await contextMenuPromise;

  // The context menu should show the "add" item and the "manage" item. The 4th
  // item is "remove extension" but it is hidden. Click the "add" item.
  menuItems = collectContextMenuItems();
  Assert.equal(menuItems.length, 4, "Context menu has 4 children");
  Assert.equal(
    menuItems[0].label,
    "Remove from Address Bar",
    "Context menu is in the 'remove' state"
  );
  Assert.equal(
    menuItems[1].localName,
    "menuseparator",
    "menuseparator is present"
  );
  Assert.equal(
    menuItems[2].label,
    "Manage Extension\u2026",
    "'Manage' item is present"
  );
  Assert.equal(
    menuItems[3].label,
    "Remove Extension",
    "'Remove' item is present"
  );
  Assert.ok(menuItems[3].hidden, "'Remove' item is hidden");

  // Click the "manage" item, about:addons should open.
  contextMenuPromise = promisePanelHidden("pageActionContextMenu");
  aboutAddonsPromise = BrowserTestUtils.waitForNewTab(gBrowser, "about:addons");
  EventUtils.synthesizeMouseAtCenter(menuItems[2], {});
  values = await Promise.all([aboutAddonsPromise, contextMenuPromise]);
  aboutAddonsTab = values[0];
  BrowserTestUtils.removeTab(aboutAddonsTab);

  // Done, clean up.
  action.remove();

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
    ["pageAction", null, { action: "manage" }],
    ["pageAction", null, { action: "manage" }],
  ]);

  // urlbar tests that run after this one can break if the mouse is left over
  // the area where the urlbar popup appears, which seems to happen due to the
  // above synthesized mouse events.  Move it over the urlbar.
  EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, { type: "mousemove" });
  gURLBar.focus();
});

// The context menu shouldn't open on separators in the panel.
add_task(async function contextMenuOnSeparator() {
  // Open the panel and get the bookmark separator.
  await promiseOpenPageActionPanel();
  let separator = BrowserPageActions.panelButtonNodeForActionID(
    PageActions.ACTION_ID_BOOKMARK_SEPARATOR
  );
  Assert.ok(separator, "The bookmark separator should be in the panel");

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
