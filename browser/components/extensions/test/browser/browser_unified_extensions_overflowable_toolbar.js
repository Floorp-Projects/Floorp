/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests the behaviour of the overflowable nav-bar with Unified
 * Extensions enabled and disabled.
 */

"use strict";

loadTestSubscript("head_unified_extensions.js");

requestLongerTimeout(2);

const NUM_EXTENSIONS = 5;
const OVERFLOW_WINDOW_WIDTH_PX = 450;
const DEFAULT_WIDGET_IDS = [
  "home-button",
  "library-button",
  "zoom-controls",
  "search-container",
  "sidebar-button",
];
const OVERFLOWED_EXTENSIONS_LIST_ID = "overflowed-extensions-list";

add_setup(async function () {
  // To make it easier to control things that will overflow, we'll start by
  // removing that's removable out of the nav-bar and adding just a fixed
  // set of items (DEFAULT_WIDGET_IDS) at the end of the nav-bar.
  let existingWidgetIDs = CustomizableUI.getWidgetIdsInArea(
    CustomizableUI.AREA_NAVBAR
  );
  for (let widgetID of existingWidgetIDs) {
    if (CustomizableUI.isWidgetRemovable(widgetID)) {
      CustomizableUI.removeWidgetFromArea(widgetID);
    }
  }
  for (const widgetID of DEFAULT_WIDGET_IDS) {
    CustomizableUI.addWidgetToArea(widgetID, CustomizableUI.AREA_NAVBAR);
  }

  registerCleanupFunction(async () => {
    await CustomizableUI.reset();
  });
});

/**
 * Returns the IDs of the children of parent.
 *
 * @param {Element} parent
 * @returns {string[]} the IDs of the children
 */
function getChildrenIDs(parent) {
  return Array.from(parent.children).map(child => child.id);
}

/**
 * Returns a NodeList of all non-hidden menu, menuitem and menuseparators
 * that are direct descendants of popup.
 *
 * @param {Element} popup
 * @returns {NodeList} the visible items.
 */
function getVisibleMenuItems(popup) {
  return popup.querySelectorAll(
    ":scope > :is(menu, menuitem, menuseparator):not([hidden])"
  );
}

/**
 * This helper function does most of the heavy lifting for these tests.
 * It does the following in order:
 *
 * 1. Registers and enables NUM_EXTENSIONS test WebExtensions that add
 *    browser_action buttons to the nav-bar.
 * 2. Resizes the window to force things after the URL bar to overflow.
 * 3. Calls an async test function to analyze the overflow lists.
 * 4. Restores the window's original width, ensuring that the IDs of the
 *    nav-bar match the original set.
 * 5. Unloads all of the test WebExtensions
 *
 * @param {DOMWindow} win The browser window to perform the test on.
 * @param {object} options Additional options when running this test.
 * @param {Function} options.beforeOverflowed This optional async function will
 *     be run after the extensions are created and added to the toolbar, but
 *     before the toolbar overflows. The function is called with the following
 *     arguments:
 *
 *     {string[]} extensionIDs: The IDs of the test WebExtensions.
 *
 *     The return value of the function is ignored.
 * @param {Function} options.whenOverflowed This optional async function will
 *     run once the window is in the overflow state. The function is called
 *     with the following arguments:
 *
 *     {Element} defaultList: The DOM element that holds overflowed default
 *       items.
 *     {Element} unifiedExtensionList: The DOM element that holds overflowed
 *       WebExtension browser_actions when Unified Extensions is enabled.
 *     {string[]} extensionIDs: The IDs of the test WebExtensions.
 *
 *     The return value of the function is ignored.
 * @param {Function} options.afterUnderflowed This optional async function will
 *     be run after the window is expanded and the toolbar has underflowed, but
 *     before the extensions are removed. This function is not passed any
 *     arguments. The return value of the function is ignored.
 *
 */
async function withWindowOverflowed(
  win,
  {
    beforeOverflowed = async () => {},
    whenOverflowed = async () => {},
    afterUnderflowed = async () => {},
  } = {}
) {
  const doc = win.document;
  doc.documentElement.removeAttribute("persist");
  const navbar = doc.getElementById(CustomizableUI.AREA_NAVBAR);

  await ensureMaximizedWindow(win);

  // The OverflowableToolbar operates asynchronously at times, so we will
  // poll a widget's overflowedItem attribute to detect whether or not the
  // widgets have finished being moved. We'll use the first widget that
  // we added to the nav-bar, as this should be the left-most item in the
  // set that we added.
  const signpostWidgetID = "home-button";
  // We'll also force the signpost widget to be extra-wide to ensure that it
  // overflows after we shrink the window.
  CustomizableUI.getWidget(signpostWidgetID).forWindow(win).node.style =
    "width: 150px";

  const extWithMenuBrowserAction = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Extension #0",
      browser_specific_settings: {
        gecko: { id: "unified-extensions-overflowable-toolbar@ext-0" },
      },
      browser_action: {
        default_area: "navbar",
      },
      // We pass `activeTab` to have a different permission message when
      // hovering the primary/action button.
      permissions: ["activeTab", "contextMenus"],
    },
    background() {
      browser.contextMenus.create(
        {
          id: "some-menu-id",
          title: "Click me!",
          contexts: ["all"],
        },
        () => browser.test.sendMessage("menu-created")
      );
    },
    useAddonManager: "temporary",
  });

  const extWithSubMenuBrowserAction = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Extension #1",
      browser_specific_settings: {
        gecko: { id: "unified-extensions-overflowable-toolbar@ext-1" },
      },
      browser_action: {
        default_area: "navbar",
      },
      permissions: ["contextMenus"],
    },
    background() {
      browser.contextMenus.create({
        id: "some-menu-id",
        title: "Open sub-menu",
        contexts: ["all"],
      });
      browser.contextMenus.create(
        {
          id: "some-sub-menu-id",
          parentId: "some-menu-id",
          title: "Click me!",
          contexts: ["all"],
        },
        () => browser.test.sendMessage("menu-created")
      );
    },
    useAddonManager: "temporary",
  });

  const manifests = [];
  for (let i = 2; i < NUM_EXTENSIONS; ++i) {
    manifests.push({
      name: `Extension #${i}`,
      browser_action: {
        default_area: "navbar",
      },
      browser_specific_settings: {
        gecko: { id: `unified-extensions-overflowable-toolbar@ext-${i}` },
      },
    });
  }

  const extensions = [
    extWithMenuBrowserAction,
    extWithSubMenuBrowserAction,
    ...createExtensions(manifests),
  ];

  // Adding browser actions is asynchronous, so this CustomizableUI listener
  // is used to make sure that the browser action widgets have finished getting
  // added.
  let listener = {
    _remainingBrowserActions: NUM_EXTENSIONS,
    _deferred: Promise.withResolvers(),

    get promise() {
      return this._deferred.promise;
    },

    onWidgetAdded(widgetID, area) {
      if (widgetID.endsWith("-browser-action")) {
        this._remainingBrowserActions--;
      }
      if (!this._remainingBrowserActions) {
        this._deferred.resolve();
      }
    },
  };
  CustomizableUI.addListener(listener);
  // Start all the extensions sequentially.
  for (const extension of extensions) {
    await extension.startup();
  }
  await Promise.all([
    extWithMenuBrowserAction.awaitMessage("menu-created"),
    extWithSubMenuBrowserAction.awaitMessage("menu-created"),
  ]);
  await listener.promise;
  CustomizableUI.removeListener(listener);

  const extensionIDs = extensions.map(extension => extension.id);

  try {
    info("Running beforeOverflowed task");
    await beforeOverflowed(extensionIDs);
  } finally {
    // The beforeOverflowed task may have moved some items out from the navbar,
    // so only listen for overflows for items still in there.
    const browserActionIDs = extensionIDs.map(id =>
      AppUiTestInternals.getBrowserActionWidgetId(id)
    );
    const browserActionsInNavBar = browserActionIDs.filter(widgetID => {
      let placement = CustomizableUI.getPlacementOfWidget(widgetID);
      return placement.area == CustomizableUI.AREA_NAVBAR;
    });

    let widgetOverflowListener = {
      _remainingOverflowables:
        browserActionsInNavBar.length + DEFAULT_WIDGET_IDS.length,
      _deferred: Promise.withResolvers(),

      get promise() {
        return this._deferred.promise;
      },

      onWidgetOverflow(widgetNode, areaNode) {
        this._remainingOverflowables--;
        if (!this._remainingOverflowables) {
          this._deferred.resolve();
        }
      },
    };
    CustomizableUI.addListener(widgetOverflowListener);

    win.resizeTo(OVERFLOW_WINDOW_WIDTH_PX, win.outerHeight);
    await widgetOverflowListener.promise;
    CustomizableUI.removeListener(widgetOverflowListener);

    Assert.ok(
      navbar.hasAttribute("overflowing"),
      "Should have an overflowing toolbar."
    );

    const defaultList = doc.getElementById(
      navbar.getAttribute("default-overflowtarget")
    );

    const unifiedExtensionList = doc.getElementById(
      navbar.getAttribute("addon-webext-overflowtarget")
    );

    try {
      info("Running whenOverflowed task");
      await whenOverflowed(defaultList, unifiedExtensionList, extensionIDs);
    } finally {
      await ensureMaximizedWindow(win);

      // Notably, we don't wait for the nav-bar to not have the "overflowing"
      // attribute. This is because we might be running in an environment
      // where the nav-bar was overflowing to begin with. Let's just hope that
      // our sign-post widget has stopped overflowing.
      await TestUtils.waitForCondition(() => {
        return !doc
          .getElementById(signpostWidgetID)
          .hasAttribute("overflowedItem");
      });

      try {
        info("Running afterUnderflowed task");
        await afterUnderflowed();
      } finally {
        await Promise.all(extensions.map(extension => extension.unload()));
      }
    }
  }
}

async function verifyExtensionWidget(widget, win = window) {
  Assert.ok(widget, "expected widget");

  let actionButton = widget.querySelector(
    ".unified-extensions-item-action-button"
  );
  Assert.ok(
    actionButton.classList.contains("unified-extensions-item-action-button"),
    "expected action class on the button"
  );
  ok(
    actionButton.classList.contains("subviewbutton"),
    "expected the .subviewbutton CSS class on the action button in the panel"
  );
  ok(
    !actionButton.classList.contains("toolbarbutton-1"),
    "expected no .toolbarbutton-1 CSS class on the action button in the panel"
  );

  let menuButton = widget.lastElementChild;
  Assert.ok(
    menuButton.classList.contains("unified-extensions-item-menu-button"),
    "expected class on the button"
  );
  ok(
    menuButton.classList.contains("subviewbutton"),
    "expected the .subviewbutton CSS class on the menu button in the panel"
  );
  ok(
    !menuButton.classList.contains("toolbarbutton-1"),
    "expected no .toolbarbutton-1 CSS class on the menu button in the panel"
  );

  let contents = actionButton.querySelector(
    ".unified-extensions-item-contents"
  );

  Assert.ok(contents, "expected contents element");
  // This is needed to correctly position the contents (vbox) element in the
  // toolbarbutton.
  Assert.equal(
    contents.getAttribute("move-after-stack"),
    "true",
    "expected move-after-stack attribute to be set"
  );
  // Make sure the contents element is inserted after the stack one (which is
  // automagically created by the toolbarbutton element).
  Assert.deepEqual(
    Array.from(actionButton.childNodes.values()).map(
      child => child.classList[0]
    ),
    [
      // The stack (which contains the extension icon) should be the first
      // child.
      "toolbarbutton-badge-stack",
      // This is the widget label, which is hidden with CSS.
      "toolbarbutton-text",
      // This is the contents element, which displays the extension name and
      // messages.
      "unified-extensions-item-contents",
    ],
    "expected the correct order for the children of the action button"
  );

  let name = contents.querySelector(".unified-extensions-item-name");
  Assert.ok(name, "expected name element");
  Assert.ok(
    name.textContent.startsWith("Extension "),
    "expected name to not be empty"
  );
  Assert.ok(
    contents.querySelector(".unified-extensions-item-message-default"),
    "expected message default element"
  );
  Assert.ok(
    contents.querySelector(".unified-extensions-item-message-hover"),
    "expected message hover element"
  );

  Assert.equal(
    win.document.l10n.getAttributes(menuButton).id,
    "unified-extensions-item-open-menu",
    "expected l10n id attribute for the extension"
  );
  Assert.deepEqual(
    Object.keys(win.document.l10n.getAttributes(menuButton).args),
    ["extensionName"],
    "expected l10n args attribute for the extension"
  );
  Assert.ok(
    win.document.l10n
      .getAttributes(menuButton)
      .args.extensionName.startsWith("Extension "),
    "expected l10n args attribute to start with the correct name"
  );
  Assert.ok(
    menuButton.getAttribute("aria-label") !== "",
    "expected menu button to have non-empty localized content"
  );
}

/**
 * Tests that overflowed browser actions go to the Unified Extensions
 * panel, and default toolbar items go into the default overflow
 * panel.
 */
add_task(async function test_overflowable_toolbar() {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  let movedNode;

  await withWindowOverflowed(win, {
    whenOverflowed: async (defaultList, unifiedExtensionList, extensionIDs) => {
      // Ensure that there are 5 items in the Unified Extensions overflow
      // list, and the default widgets should all be in the default overflow
      // list (though there might be more items from the nav-bar in there that
      // already existed in the nav-bar before we put the default widgets in
      // there as well).
      let defaultListIDs = getChildrenIDs(defaultList);
      for (const widgetID of DEFAULT_WIDGET_IDS) {
        Assert.ok(
          defaultListIDs.includes(widgetID),
          `Default overflow list should have ${widgetID}`
        );
      }

      Assert.ok(
        unifiedExtensionList.children.length,
        "Should have items in the Unified Extension list."
      );

      for (const child of Array.from(unifiedExtensionList.children)) {
        Assert.ok(
          extensionIDs.includes(child.dataset.extensionid),
          `Unified Extensions overflow list should have ${child.dataset.extensionid}`
        );
        await verifyExtensionWidget(child, win);
      }

      let extensionWidgetID = AppUiTestInternals.getBrowserActionWidgetId(
        extensionIDs.at(-1)
      );
      movedNode =
        CustomizableUI.getWidget(extensionWidgetID).forWindow(win).node;
      Assert.equal(movedNode.getAttribute("cui-areatype"), "toolbar");

      CustomizableUI.addWidgetToArea(
        extensionWidgetID,
        CustomizableUI.AREA_ADDONS
      );

      Assert.equal(
        movedNode.getAttribute("cui-areatype"),
        "panel",
        "The moved browser action button should have the right cui-areatype set."
      );
    },
    afterUnderflowed: async () => {
      // Ensure that the moved node's parent is still the add-ons panel.
      Assert.equal(
        movedNode.parentElement.id,
        CustomizableUI.AREA_ADDONS,
        "The browser action should still be in the addons panel"
      );
      CustomizableUI.addWidgetToArea(movedNode.id, CustomizableUI.AREA_NAVBAR);
    },
  });

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_context_menu() {
  let win = await BrowserTestUtils.openNewBrowserWindow();

  await withWindowOverflowed(win, {
    whenOverflowed: async (defaultList, unifiedExtensionList, extensionIDs) => {
      Assert.ok(
        unifiedExtensionList.children.length,
        "Should have items in the Unified Extension list."
      );

      // Open the extension panel.
      await openExtensionsPanel(win);

      // Let's verify the context menus for the following extensions:
      //
      // - the first one defines a menu in the background script
      // - the second one defines a menu with submenu
      // - the third extension has no menu

      info("extension with browser action and a menu");
      const firstExtensionWidget = unifiedExtensionList.children[0];
      Assert.ok(firstExtensionWidget, "expected extension widget");
      let contextMenu = await openUnifiedExtensionsContextMenu(
        firstExtensionWidget.dataset.extensionid,
        win
      );
      Assert.ok(contextMenu, "expected a context menu");
      let visibleItems = getVisibleMenuItems(contextMenu);

      // The context menu for the extension that declares a browser action menu
      // should have the menu item created by the extension, a menu separator, the control
      // for pinning the browser action to the toolbar, a menu separator and the 3 default menu items.
      is(
        visibleItems.length,
        7,
        "expected a custom context menu item, a menu separator, the pin to " +
          "toolbar menu item, a menu separator, and the 3 default menu items"
      );

      const [item, separator] = visibleItems;
      is(
        item.getAttribute("label"),
        "Click me!",
        "expected menu item as first child"
      );
      is(
        separator.tagName,
        "menuseparator",
        "expected separator after last menu item created by the extension"
      );

      await closeChromeContextMenu(contextMenu.id, null, win);

      info("extension with browser action and a menu with submenu");
      const secondExtensionWidget = unifiedExtensionList.children[1];
      Assert.ok(secondExtensionWidget, "expected extension widget");
      contextMenu = await openUnifiedExtensionsContextMenu(
        secondExtensionWidget.dataset.extensionid,
        win
      );
      visibleItems = getVisibleMenuItems(contextMenu);
      is(visibleItems.length, 7, "expected 7 menu items");
      const popup = await openSubmenu(visibleItems[0]);
      is(popup.children.length, 1, "expected 1 submenu item");
      is(
        popup.children[0].getAttribute("label"),
        "Click me!",
        "expected menu item"
      );
      // The number of items in the (main) context menu should remain the same.
      visibleItems = getVisibleMenuItems(contextMenu);
      is(visibleItems.length, 7, "expected 7 menu items");
      await closeChromeContextMenu(contextMenu.id, null, win);

      info("extension with no browser action and no menu");
      // There is no context menu created by this extension, so there should
      // only be 3 menu items corresponding to the default manage/remove/report
      // items.
      const thirdExtensionWidget = unifiedExtensionList.children[2];
      Assert.ok(thirdExtensionWidget, "expected extension widget");
      contextMenu = await openUnifiedExtensionsContextMenu(
        thirdExtensionWidget.dataset.extensionid,
        win
      );
      Assert.ok(contextMenu, "expected a context menu");
      visibleItems = getVisibleMenuItems(contextMenu);
      is(visibleItems.length, 5, "expected 5 menu items");

      await closeChromeContextMenu(contextMenu.id, null, win);

      // We can close the unified extensions panel now.
      await closeExtensionsPanel(win);
    },
  });

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_message_deck() {
  let win = await BrowserTestUtils.openNewBrowserWindow();

  await withWindowOverflowed(win, {
    whenOverflowed: async (defaultList, unifiedExtensionList, extensionIDs) => {
      Assert.ok(
        unifiedExtensionList.children.length,
        "Should have items in the Unified Extension list."
      );

      const firstExtensionWidget = unifiedExtensionList.children[0];
      Assert.ok(firstExtensionWidget, "expected extension widget");
      Assert.ok(
        firstExtensionWidget.dataset.extensionid,
        "expected data attribute for extension ID"
      );

      // Navigate to a page where `activeTab` is useful.
      await BrowserTestUtils.withNewTab(
        { gBrowser: win.gBrowser, url: "https://example.com/" },
        async () => {
          // Open the extension panel.
          await openExtensionsPanel(win);

          info("verify message when focusing the action button");
          const item = getUnifiedExtensionsItem(
            firstExtensionWidget.dataset.extensionid,
            win
          );
          Assert.ok(item, "expected an item for the extension");

          const actionButton = item.querySelector(
            ".unified-extensions-item-action-button"
          );
          Assert.ok(actionButton, "expected action button");

          const menuButton = item.querySelector(
            ".unified-extensions-item-menu-button"
          );
          Assert.ok(menuButton, "expected menu button");

          const messageDeck = item.querySelector(
            ".unified-extensions-item-message-deck"
          );
          Assert.ok(messageDeck, "expected message deck");
          is(
            messageDeck.selectedIndex,
            win.gUnifiedExtensions.MESSAGE_DECK_INDEX_DEFAULT,
            "expected selected message in the deck to be the default message"
          );

          const defaultMessage = item.querySelector(
            ".unified-extensions-item-message-default"
          );
          Assert.deepEqual(
            win.document.l10n.getAttributes(defaultMessage),
            { id: "origin-controls-state-when-clicked", args: null },
            "expected correct l10n attributes for the default message"
          );
          Assert.ok(
            defaultMessage.textContent !== "",
            "expected default message to not be empty"
          );

          const hoverMessage = item.querySelector(
            ".unified-extensions-item-message-hover"
          );
          Assert.deepEqual(
            win.document.l10n.getAttributes(hoverMessage),
            { id: "origin-controls-state-hover-run-visit-only", args: null },
            "expected correct l10n attributes for the hover message"
          );
          Assert.ok(
            hoverMessage.textContent !== "",
            "expected hover message to not be empty"
          );

          const hoverMenuButtonMessage = item.querySelector(
            ".unified-extensions-item-message-hover-menu-button"
          );
          Assert.deepEqual(
            win.document.l10n.getAttributes(hoverMenuButtonMessage),
            { id: "unified-extensions-item-message-manage", args: null },
            "expected correct l10n attributes for the message when hovering the menu button"
          );
          Assert.ok(
            hoverMenuButtonMessage.textContent !== "",
            "expected message for when the menu button is hovered to not be empty"
          );

          // 1. Focus the action button of the first extension in the panel.
          let focused = BrowserTestUtils.waitForEvent(actionButton, "focus");
          EventUtils.synthesizeKey("VK_TAB", {}, win);
          await focused;
          is(
            actionButton,
            win.document.activeElement,
            "expected action button of the first extension item to be focused"
          );
          is(
            messageDeck.selectedIndex,
            win.gUnifiedExtensions.MESSAGE_DECK_INDEX_HOVER,
            "expected selected message in the deck to be the hover message"
          );

          // 2. Focus the menu button, causing the action button to lose focus.
          focused = BrowserTestUtils.waitForEvent(menuButton, "focus");
          EventUtils.synthesizeKey("VK_TAB", {}, win);
          await focused;
          is(
            menuButton,
            win.document.activeElement,
            "expected menu button of the first extension item to be focused"
          );
          is(
            messageDeck.selectedIndex,
            win.gUnifiedExtensions.MESSAGE_DECK_INDEX_MENU_HOVER,
            "expected selected message in the deck to be the message when focusing the menu button"
          );

          await closeExtensionsPanel(win);

          info("verify message when hovering the action button");
          await openExtensionsPanel(win);

          is(
            messageDeck.selectedIndex,
            win.gUnifiedExtensions.MESSAGE_DECK_INDEX_DEFAULT,
            "expected selected message in the deck to be the default message"
          );

          // 1. Hover the action button of the first extension in the panel.
          let hovered = BrowserTestUtils.waitForEvent(
            actionButton,
            "mouseover"
          );
          EventUtils.synthesizeMouseAtCenter(
            actionButton,
            { type: "mouseover" },
            win
          );
          await hovered;
          is(
            messageDeck.selectedIndex,
            win.gUnifiedExtensions.MESSAGE_DECK_INDEX_HOVER,
            "expected selected message in the deck to be the hover message"
          );

          // 2. Hover the menu button, causing the action button to no longer
          // be hovered.
          hovered = BrowserTestUtils.waitForEvent(menuButton, "mouseover");
          EventUtils.synthesizeMouseAtCenter(
            menuButton,
            { type: "mouseover" },
            win
          );
          await hovered;
          is(
            messageDeck.selectedIndex,
            win.gUnifiedExtensions.MESSAGE_DECK_INDEX_MENU_HOVER,
            "expected selected message in the deck to be the message when hovering the menu button"
          );

          await closeExtensionsPanel(win);
        }
      );
    },
  });

  await BrowserTestUtils.closeWindow(win);
});

/**
 * Tests that if we pin a browser action button listed in the addons panel
 * to the toolbar when that button would immediately overflow, that the
 * button is put into the addons panel overflow list.
 */
add_task(async function test_pinning_to_toolbar_when_overflowed() {
  let win = await BrowserTestUtils.openNewBrowserWindow();

  let movedNode;
  let extensionWidgetID;
  let actionButton;
  let menuButton;

  await withWindowOverflowed(win, {
    beforeOverflowed: async extensionIDs => {
      // Before we overflow the toolbar, let's move the last item to the addons
      // panel.
      extensionWidgetID = AppUiTestInternals.getBrowserActionWidgetId(
        extensionIDs.at(-1)
      );

      movedNode =
        CustomizableUI.getWidget(extensionWidgetID).forWindow(win).node;

      actionButton = movedNode.querySelector(
        ".unified-extensions-item-action-button"
      );
      ok(
        actionButton.classList.contains("toolbarbutton-1"),
        "expected .toolbarbutton-1 CSS class on the action button in the navbar"
      );
      ok(
        !actionButton.classList.contains("subviewbutton"),
        "expected no .subviewbutton CSS class on the action button in the navbar"
      );

      menuButton = movedNode.querySelector(
        ".unified-extensions-item-menu-button"
      );
      ok(
        menuButton.classList.contains("toolbarbutton-1"),
        "expected .toolbarbutton-1 CSS class on the menu button in the navbar"
      );
      ok(
        !menuButton.classList.contains("subviewbutton"),
        "expected no .subviewbutton CSS class on the menu button in the navbar"
      );

      CustomizableUI.addWidgetToArea(
        extensionWidgetID,
        CustomizableUI.AREA_ADDONS
      );

      ok(
        actionButton.classList.contains("subviewbutton"),
        "expected .subviewbutton CSS class on the action button in the panel"
      );
      ok(
        !actionButton.classList.contains("toolbarbutton-1"),
        "expected no .toolbarbutton-1 CSS class on the action button in the panel"
      );
      ok(
        menuButton.classList.contains("subviewbutton"),
        "expected .subviewbutton CSS class on the menu button in the panel"
      );
      ok(
        !menuButton.classList.contains("toolbarbutton-1"),
        "expected no .toolbarbutton-1 CSS class on the menu button in the panel"
      );
    },
    whenOverflowed: async (defaultList, unifiedExtensionList, extensionIDs) => {
      ok(
        actionButton.classList.contains("subviewbutton"),
        "expected .subviewbutton CSS class on the action button in the panel"
      );
      ok(
        !actionButton.classList.contains("toolbarbutton-1"),
        "expected no .toolbarbutton-1 CSS class on the action button in the panel"
      );
      ok(
        menuButton.classList.contains("subviewbutton"),
        "expected .subviewbutton CSS class on the menu button in the panel"
      );
      ok(
        !menuButton.classList.contains("toolbarbutton-1"),
        "expected no .toolbarbutton-1 CSS class on the menu button in the panel"
      );

      // Now that the window is overflowed, let's move the widget in the addons
      // panel back to the navbar. This should cause the widget to overflow back
      // into the addons panel.
      CustomizableUI.addWidgetToArea(
        extensionWidgetID,
        CustomizableUI.AREA_NAVBAR
      );
      await TestUtils.waitForCondition(() => {
        return movedNode.hasAttribute("overflowedItem");
      });
      Assert.equal(
        movedNode.parentElement,
        unifiedExtensionList,
        "Should have overflowed the extension button to the right list."
      );

      ok(
        actionButton.classList.contains("subviewbutton"),
        "expected .subviewbutton CSS class on the action button in the panel"
      );
      ok(
        !actionButton.classList.contains("toolbarbutton-1"),
        "expected no .toolbarbutton-1 CSS class on the action button in the panel"
      );
      ok(
        menuButton.classList.contains("subviewbutton"),
        "expected .subviewbutton CSS class on the menu button in the panel"
      );
      ok(
        !menuButton.classList.contains("toolbarbutton-1"),
        "expected no .toolbarbutton-1 CSS class on the menu button in the panel"
      );
    },
  });

  await BrowserTestUtils.closeWindow(win);
});

/**
 * This test verifies that, when an extension placed in the toolbar is
 * overflowed into the addons panel and context-clicked, it shows the "Pin to
 * Toolbar" item as checked, and that unchecking this menu item inserts the
 * extension into the dedicated addons area of the panel, and that the item
 * then does not underflow.
 */
add_task(async function test_unpin_overflowed_widget() {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  let extensionID;

  await withWindowOverflowed(win, {
    whenOverflowed: async (defaultList, unifiedExtensionList, extensionIDs) => {
      const firstExtensionWidget = unifiedExtensionList.children[0];
      Assert.ok(firstExtensionWidget, "expected an extension widget");
      extensionID = firstExtensionWidget.dataset.extensionid;

      let movedNode = CustomizableUI.getWidget(
        firstExtensionWidget.id
      ).forWindow(win).node;
      Assert.equal(
        movedNode.getAttribute("cui-areatype"),
        "toolbar",
        "expected extension widget to be in the toolbar"
      );
      Assert.ok(
        movedNode.hasAttribute("overflowedItem"),
        "expected extension widget to be overflowed"
      );
      let actionButton = movedNode.querySelector(
        ".unified-extensions-item-action-button"
      );
      ok(
        actionButton.classList.contains("subviewbutton"),
        "expected the .subviewbutton CSS class on the action button in the panel"
      );
      ok(
        !actionButton.classList.contains("toolbarbutton-1"),
        "expected no .toolbarbutton-1 CSS class on the action button in the panel"
      );
      let menuButton = movedNode.querySelector(
        ".unified-extensions-item-menu-button"
      );
      ok(
        menuButton.classList.contains("subviewbutton"),
        "expected the .subviewbutton CSS class on the menu button in the panel"
      );
      ok(
        !menuButton.classList.contains("toolbarbutton-1"),
        "expected no .toolbarbutton-1 CSS class on the menu button in the panel"
      );

      // Open the panel, then the context menu of the extension widget, verify
      // the 'Pin to Toolbar' menu item, then click on this menu item to
      // uncheck it (i.e. unpin the extension).
      await openExtensionsPanel(win);
      const contextMenu = await openUnifiedExtensionsContextMenu(
        extensionID,
        win
      );
      Assert.ok(contextMenu, "expected a context menu");

      const pinToToolbar = contextMenu.querySelector(
        ".unified-extensions-context-menu-pin-to-toolbar"
      );
      Assert.ok(pinToToolbar, "expected a 'Pin to Toolbar' menu item");
      Assert.ok(
        !pinToToolbar.hidden,
        "expected 'Pin to Toolbar' to be visible"
      );
      Assert.equal(
        pinToToolbar.getAttribute("checked"),
        "true",
        "expected 'Pin to Toolbar' to be checked"
      );

      // Uncheck "Pin to Toolbar" menu item. Clicking a menu item in the
      // context menu closes the unified extensions panel automatically.
      const hidden = BrowserTestUtils.waitForEvent(
        win.gUnifiedExtensions.panel,
        "popuphidden",
        true
      );
      contextMenu.activateItem(pinToToolbar);
      await hidden;

      // We expect the widget to no longer be overflowed.
      await TestUtils.waitForCondition(() => {
        return !movedNode.hasAttribute("overflowedItem");
      });

      Assert.equal(
        movedNode.parentElement.id,
        CustomizableUI.AREA_ADDONS,
        "expected extension widget to have been unpinned and placed in the addons area"
      );
      Assert.equal(
        movedNode.getAttribute("cui-areatype"),
        "panel",
        "expected extension widget to be in the unified extensions panel"
      );
    },
    afterUnderflowed: async () => {
      await openExtensionsPanel(win);

      const item = getUnifiedExtensionsItem(extensionID, win);
      Assert.ok(
        item,
        "expected extension widget to be listed in the unified extensions panel"
      );
      let actionButton = item.querySelector(
        ".unified-extensions-item-action-button"
      );
      ok(
        actionButton.classList.contains("subviewbutton"),
        "expected the .subviewbutton CSS class on the action button in the panel"
      );
      ok(
        !actionButton.classList.contains("toolbarbutton-1"),
        "expected no .toolbarbutton-1 CSS class on the action button in the panel"
      );
      let menuButton = item.querySelector(
        ".unified-extensions-item-menu-button"
      );
      ok(
        menuButton.classList.contains("subviewbutton"),
        "expected the .subviewbutton CSS class on the menu button in the panel"
      );
      ok(
        !menuButton.classList.contains("toolbarbutton-1"),
        "expected no .toolbarbutton-1 CSS class on the menu button in the panel"
      );

      await closeExtensionsPanel(win);
    },
  });

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_overflow_with_a_second_window() {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  // Open a second window that will stay maximized. We want to be sure that
  // overflowing a widget in one window isn't going to affect the other window
  // since we have an instance (of a CUI widget) per window.
  let secondWin = await BrowserTestUtils.openNewBrowserWindow();
  await ensureMaximizedWindow(secondWin);
  await BrowserTestUtils.openNewForegroundTab(
    secondWin.gBrowser,
    "https://example.com/"
  );

  // Make sure the first window is the active window.
  let windowActivePromise = new Promise(resolve => {
    if (Services.focus.activeWindow == win) {
      resolve();
    } else {
      win.addEventListener(
        "activate",
        () => {
          resolve();
        },
        { once: true }
      );
    }
  });
  win.focus();
  await windowActivePromise;

  let extensionWidgetID;
  let aNode;
  let aNodeInSecondWindow;

  await withWindowOverflowed(win, {
    beforeOverflowed: async extensionIDs => {
      extensionWidgetID = AppUiTestInternals.getBrowserActionWidgetId(
        extensionIDs.at(-1)
      );

      // This is the DOM node for the current window that is overflowed.
      aNode = CustomizableUI.getWidget(extensionWidgetID).forWindow(win).node;
      Assert.ok(
        !aNode.hasAttribute("overflowedItem"),
        "expected extension widget to NOT be overflowed"
      );

      let actionButton = aNode.querySelector(
        ".unified-extensions-item-action-button"
      );
      ok(
        actionButton.classList.contains("toolbarbutton-1"),
        "expected .toolbarbutton-1 CSS class on the action button"
      );
      ok(
        !actionButton.classList.contains("subviewbutton"),
        "expected no .subviewbutton CSS class on the action button"
      );

      let menuButton = aNode.querySelector(
        ".unified-extensions-item-menu-button"
      );
      ok(
        menuButton.classList.contains("toolbarbutton-1"),
        "expected .toolbarbutton-1 CSS class on the menu button"
      );
      ok(
        !menuButton.classList.contains("subviewbutton"),
        "expected no .subviewbutton CSS class on the menu button"
      );

      // This is the DOM node of the same CUI widget but in the maximized
      // window opened before.
      aNodeInSecondWindow =
        CustomizableUI.getWidget(extensionWidgetID).forWindow(secondWin).node;

      let actionButtonInSecondWindow = aNodeInSecondWindow.querySelector(
        ".unified-extensions-item-action-button"
      );
      ok(
        actionButtonInSecondWindow.classList.contains("toolbarbutton-1"),
        "expected .toolbarbutton-1 CSS class on the action button in the second window"
      );
      ok(
        !actionButtonInSecondWindow.classList.contains("subviewbutton"),
        "expected no .subviewbutton CSS class on the action button in the second window"
      );

      let menuButtonInSecondWindow = aNodeInSecondWindow.querySelector(
        ".unified-extensions-item-menu-button"
      );
      ok(
        menuButtonInSecondWindow.classList.contains("toolbarbutton-1"),
        "expected .toolbarbutton-1 CSS class on the menu button in the second window"
      );
      ok(
        !menuButtonInSecondWindow.classList.contains("subviewbutton"),
        "expected no .subviewbutton CSS class on the menu button in the second window"
      );
    },
    whenOverflowed: async (defaultList, unifiedExtensionList, extensionIDs) => {
      // The DOM node should have been overflowed.
      Assert.ok(
        aNode.hasAttribute("overflowedItem"),
        "expected extension widget to be overflowed"
      );
      Assert.equal(
        aNode.getAttribute("widget-id"),
        extensionWidgetID,
        "expected the CUI widget ID to be set on the DOM node"
      );

      // When the node is overflowed, we swap the CSS class on the action
      // and menu buttons since the node is now placed in the extensions panel.
      let actionButton = aNode.querySelector(
        ".unified-extensions-item-action-button"
      );
      ok(
        actionButton.classList.contains("subviewbutton"),
        "expected the .subviewbutton CSS class on the action button"
      );
      ok(
        !actionButton.classList.contains("toolbarbutton-1"),
        "expected no .toolbarbutton-1 CSS class on the action button"
      );
      let menuButton = aNode.querySelector(
        ".unified-extensions-item-menu-button"
      );
      ok(
        menuButton.classList.contains("subviewbutton"),
        "expected the .subviewbutton CSS class on the menu button"
      );
      ok(
        !menuButton.classList.contains("toolbarbutton-1"),
        "expected no .toolbarbutton-1 CSS class on the menu button"
      );

      // The DOM node in the other window should not have been overflowed.
      Assert.ok(
        !aNodeInSecondWindow.hasAttribute("overflowedItem"),
        "expected extension widget to NOT be overflowed in the other window"
      );
      Assert.equal(
        aNodeInSecondWindow.getAttribute("widget-id"),
        extensionWidgetID,
        "expected the CUI widget ID to be set on the DOM node"
      );

      // We expect no CSS class changes for the node in the other window.
      let actionButtonInSecondWindow = aNodeInSecondWindow.querySelector(
        ".unified-extensions-item-action-button"
      );
      ok(
        actionButtonInSecondWindow.classList.contains("toolbarbutton-1"),
        "expected .toolbarbutton-1 CSS class on the action button in the second window"
      );
      ok(
        !actionButtonInSecondWindow.classList.contains("subviewbutton"),
        "expected no .subviewbutton CSS class on the action button in the second window"
      );
      let menuButtonInSecondWindow = aNodeInSecondWindow.querySelector(
        ".unified-extensions-item-menu-button"
      );
      ok(
        menuButtonInSecondWindow.classList.contains("toolbarbutton-1"),
        "expected .toolbarbutton-1 CSS class on the menu button in the second window"
      );
      ok(
        !menuButtonInSecondWindow.classList.contains("subviewbutton"),
        "expected no .subviewbutton CSS class on the menu button in the second window"
      );
    },
    afterUnderflowed: async () => {
      // After underflow, we expect the CSS class on the action and menu
      // buttons of the DOM node of the current window to be updated.
      let actionButton = aNode.querySelector(
        ".unified-extensions-item-action-button"
      );
      ok(
        actionButton.classList.contains("toolbarbutton-1"),
        "expected .toolbarbutton-1 CSS class on the action button in the panel"
      );
      ok(
        !actionButton.classList.contains("subviewbutton"),
        "expected no .subviewbutton CSS class on the action button in the panel"
      );
      let menuButton = aNode.querySelector(
        ".unified-extensions-item-menu-button"
      );
      ok(
        menuButton.classList.contains("toolbarbutton-1"),
        "expected .toolbarbutton-1 CSS class on the menu button in the panel"
      );
      ok(
        !menuButton.classList.contains("subviewbutton"),
        "expected no .subviewbutton CSS class on the menu button in the panel"
      );

      // The DOM node of the other window should not be changed.
      let actionButtonInSecondWindow = aNodeInSecondWindow.querySelector(
        ".unified-extensions-item-action-button"
      );
      ok(
        actionButtonInSecondWindow.classList.contains("toolbarbutton-1"),
        "expected .toolbarbutton-1 CSS class on the action button in the second window"
      );
      ok(
        !actionButtonInSecondWindow.classList.contains("subviewbutton"),
        "expected no .subviewbutton CSS class on the action button in the second window"
      );
      let menuButtonInSecondWindow = aNodeInSecondWindow.querySelector(
        ".unified-extensions-item-menu-button"
      );
      ok(
        menuButtonInSecondWindow.classList.contains("toolbarbutton-1"),
        "expected .toolbarbutton-1 CSS class on the menu button in the second window"
      );
      ok(
        !menuButtonInSecondWindow.classList.contains("subviewbutton"),
        "expected no .subviewbutton CSS class on the menu button in the second window"
      );
    },
  });

  await BrowserTestUtils.closeWindow(win);
  await BrowserTestUtils.closeWindow(secondWin);
});

add_task(async function test_overflow_with_extension_in_collapsed_area() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const bookmarksToolbar = win.document.getElementById(
    CustomizableUI.AREA_BOOKMARKS
  );

  let movedNode;
  let extensionWidgetID;
  let extensionWidgetPosition;

  await withWindowOverflowed(win, {
    beforeOverflowed: async extensionIDs => {
      // Before we overflow the toolbar, let's move the last item to the
      // (visible) bookmarks toolbar.
      extensionWidgetID = AppUiTestInternals.getBrowserActionWidgetId(
        extensionIDs.at(-1)
      );

      movedNode =
        CustomizableUI.getWidget(extensionWidgetID).forWindow(win).node;

      // Ensure that the toolbar is currently visible.
      await promiseSetToolbarVisibility(bookmarksToolbar, true);

      // Move an extension to the bookmarks toolbar.
      CustomizableUI.addWidgetToArea(
        extensionWidgetID,
        CustomizableUI.AREA_BOOKMARKS
      );

      Assert.equal(
        movedNode.parentElement.id,
        CustomizableUI.AREA_BOOKMARKS,
        "expected extension widget to be in the bookmarks toolbar"
      );
      Assert.ok(
        !movedNode.hasAttribute("artificallyOverflowed"),
        "expected node to not have any artificallyOverflowed prop"
      );

      extensionWidgetPosition =
        CustomizableUI.getPlacementOfWidget(extensionWidgetID).position;

      // At this point we have an extension in the bookmarks toolbar, and this
      // toolbar is visible. We are going to resize the window (width) AND
      // collapse the toolbar to verify that the extension placed in the
      // bookmarks toolbar is overflowed in the panel without any side effects.
    },
    whenOverflowed: async () => {
      // Ensure that the toolbar is currently collapsed.
      await promiseSetToolbarVisibility(bookmarksToolbar, false);

      Assert.equal(
        movedNode.parentElement.id,
        OVERFLOWED_EXTENSIONS_LIST_ID,
        "expected extension widget to be in the extensions panel"
      );
      Assert.ok(
        movedNode.getAttribute("artificallyOverflowed"),
        "expected node to be artifically overflowed"
      );

      // At this point the extension is in the panel because it was overflowed
      // after the bookmarks toolbar has been collapsed. The window is also
      // narrow, but we are going to restore the initial window size. Since the
      // visibility of the bookmarks toolbar hasn't changed, the extension
      // should still be in the panel.
    },
    afterUnderflowed: async () => {
      Assert.equal(
        movedNode.parentElement.id,
        OVERFLOWED_EXTENSIONS_LIST_ID,
        "expected extension widget to still be in the extensions panel"
      );
      Assert.ok(
        movedNode.getAttribute("artificallyOverflowed"),
        "expected node to still be artifically overflowed"
      );

      // Ensure that the toolbar is visible again, which should move the
      // extension back to where it was initially.
      await promiseSetToolbarVisibility(bookmarksToolbar, true);

      Assert.equal(
        movedNode.parentElement.id,
        CustomizableUI.AREA_BOOKMARKS,
        "expected extension widget to be in the bookmarks toolbar"
      );
      Assert.ok(
        !movedNode.hasAttribute("artificallyOverflowed"),
        "expected node to not have any artificallyOverflowed prop"
      );
      Assert.equal(
        CustomizableUI.getPlacementOfWidget(extensionWidgetID).position,
        extensionWidgetPosition,
        "expected the extension to be back at the same position in the bookmarks toolbar"
      );
    },
  });

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_overflowed_extension_cannot_be_moved() {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  let extensionID;

  await withWindowOverflowed(win, {
    whenOverflowed: async (defaultList, unifiedExtensionList, extensionIDs) => {
      const secondExtensionWidget = unifiedExtensionList.children[1];
      Assert.ok(secondExtensionWidget, "expected an extension widget");
      extensionID = secondExtensionWidget.dataset.extensionid;

      await openExtensionsPanel(win);
      const contextMenu = await openUnifiedExtensionsContextMenu(
        extensionID,
        win
      );
      Assert.ok(contextMenu, "expected a context menu");

      const moveUp = contextMenu.querySelector(
        ".unified-extensions-context-menu-move-widget-up"
      );
      Assert.ok(moveUp, "expected 'move up' item in the context menu");
      Assert.ok(moveUp.hidden, "expected 'move up' item to be hidden");

      const moveDown = contextMenu.querySelector(
        ".unified-extensions-context-menu-move-widget-down"
      );
      Assert.ok(moveDown, "expected 'move down' item in the context menu");
      Assert.ok(moveDown.hidden, "expected 'move down' item to be hidden");

      await closeChromeContextMenu(contextMenu.id, null, win);
      await closeExtensionsPanel(win);
    },
  });

  await BrowserTestUtils.closeWindow(win);
});
