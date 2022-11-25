/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests the behaviour of the overflowable nav-bar with Unified
 * Extensions enabled and disabled.
 */

"use strict";

loadTestSubscript("head_unified_extensions.js");

const NUM_EXTENSIONS = 5;
const OVERFLOW_WINDOW_WIDTH_PX = 450;
const DEFAULT_WIDGET_IDS = [
  "home-button",
  "library-button",
  "zoom-controls",
  "search-container",
  "sidebar-button",
];

add_setup(async function() {
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
    _deferred: PromiseUtils.defer(),

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
    const originalWindowWidth = win.outerWidth;

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
      _deferred: PromiseUtils.defer(),

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
      win.resizeTo(originalWindowWidth, win.outerHeight);
      await BrowserTestUtils.waitForEvent(win, "resize");

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

async function verifyExtensionWidget(win, widget, unifiedExtensionsEnabled) {
  Assert.ok(widget, "expected widget");

  Assert.equal(
    widget.getAttribute("unified-extensions"),
    unifiedExtensionsEnabled ? "true" : "false",
    `expected unified-extensions attribute to be ${String(
      unifiedExtensionsEnabled
    )}`
  );

  let actionButton = widget.firstElementChild;
  Assert.ok(
    actionButton.classList.contains("unified-extensions-item-action"),
    "expected action class on the button"
  );

  let menuButton = widget.lastElementChild;
  Assert.ok(
    menuButton.classList.contains("unified-extensions-item-open-menu"),
    "expected open-menu class on the button"
  );

  let contents = actionButton.querySelector(
    ".unified-extensions-item-contents"
  );

  if (unifiedExtensionsEnabled) {
    Assert.ok(
      contents,
      `expected contents element when unifiedExtensionsEnabled=${unifiedExtensionsEnabled}`
    );
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
  } else {
    Assert.ok(
      !contents,
      `expected no contents element when unifiedExtensionsEnabled=${unifiedExtensionsEnabled}`
    );

    Assert.ok(
      actionButton.getAttribute("label")?.startsWith("Extension "),
      "expected button's label to not be empty"
    );
  }
}

/**
 * Tests that overflowed browser actions go to the Unified Extensions
 * panel, and default toolbar items go into the default overflow
 * panel.
 */
add_task(async function test_overflowable_toolbar() {
  let win = await promiseEnableUnifiedExtensions();
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
        await verifyExtensionWidget(win, child, true);
      }

      let extensionWidgetID = AppUiTestInternals.getBrowserActionWidgetId(
        extensionIDs.at(-1)
      );
      movedNode = CustomizableUI.getWidget(extensionWidgetID).forWindow(win)
        .node;
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

/**
 * Tests that if Unified Extensions are disabled, all overflowed items
 * in the toolbar go to the default overflow panel.
 */
add_task(async function test_overflowable_toolbar_legacy() {
  let win = await promiseDisableUnifiedExtensions();

  await withWindowOverflowed(win, {
    whenOverflowed: async (defaultList, unifiedExtensionList, extensionIDs) => {
      // First, ensure that all default items are in the default overflow list.
      // (though there might be more items from the nav-bar in there that
      // already existed in the nav-bar before we put the default widgets in
      // there as well).
      const defaultListIDs = getChildrenIDs(defaultList);
      for (const widgetID of DEFAULT_WIDGET_IDS) {
        Assert.ok(
          defaultListIDs.includes(widgetID),
          `Default overflow list should have ${widgetID}`
        );
      }
      // Next, ensure that all of the browser_action buttons from the
      // WebExtensions are there as well.
      for (const extensionID of extensionIDs) {
        const widget = defaultList.querySelector(
          `[data-extensionid='${extensionID}']`
        );
        Assert.ok(widget, `Default list should have ${extensionID}`);
        await verifyExtensionWidget(win, widget, false);
      }

      Assert.equal(
        unifiedExtensionList.children.length,
        0,
        "Unified Extension overflow list should be empty."
      );
    },
  });

  await BrowserTestUtils.closeWindow(win);
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_menu_button() {
  let win = await promiseEnableUnifiedExtensions();

  await withWindowOverflowed(win, {
    whenOverflowed: async (defaultList, unifiedExtensionList, extensionIDs) => {
      Assert.ok(
        unifiedExtensionList.children.length,
        "Should have items in the Unified Extension list."
      );

      const firstExtensionWidget = unifiedExtensionList.children[0];
      Assert.ok(firstExtensionWidget, "expected extension widget");

      // Open the extension panel.
      await openExtensionsPanel(win);

      // When hovering the cog/menu button, we should change the message below
      // the extension name. Let's verify this on the first extension.
      info("verifying message when hovering the menu button");
      const item = getUnifiedExtensionsItem(
        win,
        firstExtensionWidget.dataset.extensionid
      );
      Assert.ok(item, "expected an item for the extension");

      const actionButton = item.querySelector(
        ".unified-extensions-item-action"
      );
      Assert.ok(actionButton, "expected action button");

      const menuButton = item.querySelector(
        ".unified-extensions-item-open-menu"
      );
      Assert.ok(menuButton, "expected 'open menu' button");

      const message = item.querySelector(
        ".unified-extensions-item-message-default"
      );
      const messageHover = item.querySelector(
        ".unified-extensions-item-message-hover"
      );
      Assert.ok(
        BrowserTestUtils.is_visible(message),
        "expected message to be visible"
      );
      Assert.ok(
        message.textContent !== "",
        "expected message on hover to not be empty"
      );
      Assert.ok(
        BrowserTestUtils.is_hidden(messageHover),
        "expected 'hover message' to be hidden"
      );

      let hovered = BrowserTestUtils.waitForEvent(menuButton, "mouseover");
      EventUtils.synthesizeMouseAtCenter(
        menuButton,
        { type: "mouseover" },
        win
      );
      await hovered;
      is(
        item.getAttribute("secondary-button-hovered"),
        "true",
        "expected secondary-button-hovered attr on the item"
      );
      Assert.ok(
        BrowserTestUtils.is_hidden(
          item.querySelector(".unified-extensions-item-message-default")
        ),
        "expected message to be hidden"
      );
      Assert.ok(
        BrowserTestUtils.is_visible(messageHover),
        "expected 'hover message' to be visible"
      );
      Assert.deepEqual(
        win.document.l10n.getAttributes(messageHover),
        { id: "unified-extensions-item-message-manage", args: null },
        "expected correct l10n attributes for the message displayed on secondary button hovered"
      );
      Assert.ok(
        messageHover.textContent !== "",
        "expected message on hover to not be empty"
      );

      let notHovered = BrowserTestUtils.waitForEvent(menuButton, "mouseout");
      // Move mouse somewhere else...
      EventUtils.synthesizeMouseAtCenter(item, { type: "mouseover" }, win);
      await notHovered;
      Assert.ok(
        !item.hasAttribute("secondary-button-hovered"),
        "expected no secondary-button-hovered attr on the item"
      );
      Assert.ok(
        BrowserTestUtils.is_visible(
          item.querySelector(".unified-extensions-item-message-default")
        ),
        "expected message to be visible"
      );
      Assert.ok(
        BrowserTestUtils.is_hidden(messageHover),
        "expected 'hover message' to be hidden"
      );

      // Close and re-open the extension panel to switch to keyboard navigation.
      await closeExtensionsPanel(win);
      await openExtensionsPanel(win);

      info("moving focus to first extension with keyboard navigation");
      let focused = BrowserTestUtils.waitForEvent(actionButton, "focus");
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      await focused;
      is(
        actionButton,
        win.document.activeElement,
        "expected action button of the first extension item to be focused"
      );
      Assert.ok(
        !item.hasAttribute("secondary-button-hovered"),
        "expected no secondary-button-hovered attr on the item"
      );
      Assert.ok(
        BrowserTestUtils.is_visible(
          item.querySelector(".unified-extensions-item-message-default")
        ),
        "expected message to be visible"
      );
      Assert.ok(
        BrowserTestUtils.is_hidden(messageHover),
        "expected 'hover message' to be hidden"
      );

      info("moving focus to menu button");
      focused = BrowserTestUtils.waitForEvent(menuButton, "focus");
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      await focused;
      is(
        menuButton,
        win.document.activeElement,
        "expected menu button of the first extension to be focused"
      );
      Assert.ok(
        item.hasAttribute("secondary-button-hovered"),
        "expected secondary-button-hovered attr on the item"
      );
      Assert.ok(
        BrowserTestUtils.is_hidden(
          item.querySelector(".unified-extensions-item-message-default")
        ),
        "expected message to be hidden"
      );
      Assert.ok(
        BrowserTestUtils.is_visible(messageHover),
        "expected 'hover message' to be visible"
      );

      await closeExtensionsPanel(win);
    },
  });

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_context_menu() {
  let win = await promiseEnableUnifiedExtensions();

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
        win,
        firstExtensionWidget.dataset.extensionid
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
        win,
        secondExtensionWidget.dataset.extensionid
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
        win,
        thirdExtensionWidget.dataset.extensionid
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

add_task(async function test_action_button() {
  let win = await promiseEnableUnifiedExtensions();

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
            win,
            firstExtensionWidget.dataset.extensionid
          );
          Assert.ok(item, "expected an item for the extension");

          const actionButton = item.querySelector(
            ".unified-extensions-item-action"
          );
          Assert.ok(actionButton, "expected action button");

          const menuButton = item.querySelector(
            ".unified-extensions-item-open-menu"
          );
          Assert.ok(menuButton, "expected menu button");

          const message = item.querySelector(
            ".unified-extensions-item-message-default"
          );
          Assert.deepEqual(
            win.document.l10n.getAttributes(message),
            { id: "origin-controls-state-when-clicked", args: null },
            "expected correct l10n attributes for the message displayed by default"
          );
          Assert.ok(
            message.textContent !== "",
            "expected message to not be empty"
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
          // Since this first extension has `activeTab` we expect a different
          // message on hover/focus.
          Assert.deepEqual(
            win.document.l10n.getAttributes(message),
            { id: "origin-controls-state-hover-run-visit-only", args: null },
            "expected correct l10n attributes for the message displayed when focusing the action button"
          );
          Assert.ok(
            message.textContent !== "",
            "expected message to not be empty"
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
          // When focusing a different button like the menu button, we expect
          // the default message to be the initial one.
          Assert.deepEqual(
            win.document.l10n.getAttributes(message),
            { id: "origin-controls-state-when-clicked", args: null },
            "expected correct l10n attributes for the message displayed by default"
          );

          await closeExtensionsPanel(win);

          info("verify message when hovering the action button");
          await openExtensionsPanel(win);
          Assert.deepEqual(
            win.document.l10n.getAttributes(message),
            { id: "origin-controls-state-when-clicked", args: null },
            "expected correct l10n attributes for the message displayed by default"
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
          // Since this first extension has `activeTab` we expect a different
          // message on hover/focus.
          Assert.deepEqual(
            win.document.l10n.getAttributes(message),
            { id: "origin-controls-state-hover-run-visit-only", args: null },
            "expected correct l10n attributes for the message displayed when hovering the action button"
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
          Assert.deepEqual(
            win.document.l10n.getAttributes(message),
            { id: "origin-controls-state-when-clicked", args: null },
            "expected correct l10n attributes for the message displayed by default"
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
  let win = await promiseEnableUnifiedExtensions();
  let movedNode;
  let extensionWidgetID;

  await withWindowOverflowed(win, {
    beforeOverflowed: async extensionIDs => {
      // Before we overflow the toolbar, let's move the last item to the addons
      // panel.
      extensionWidgetID = AppUiTestInternals.getBrowserActionWidgetId(
        extensionIDs.at(-1)
      );

      movedNode = CustomizableUI.getWidget(extensionWidgetID).forWindow(win)
        .node;

      CustomizableUI.addWidgetToArea(
        extensionWidgetID,
        CustomizableUI.AREA_ADDONS
      );
    },
    whenOverflowed: async (defaultList, unifiedExtensionList, extensionIDs) => {
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
    },
  });

  await BrowserTestUtils.closeWindow(win);
});
