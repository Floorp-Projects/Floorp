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
 * @param {function} taskFn The async function to run once the window is in
 *   the overflow state. The function is called with the following arguments:
 *
 *     {Element} defaultList: The DOM element that holds overflowed default
 *       items.
 *     {Element} unifiedExtensionList: The DOM element that holds overflowed
 *       WebExtension browser_actions when Unified Extensions is enabled.
 *     {string[]} extensionIDs: The IDs of the test WebExtensions.
 *
 *   The function is expected to return a Promise that does not resolve
 *   with anything.
 */
async function withWindowOverflowed(win, taskFn) {
  const doc = win.document;
  doc.documentElement.removeAttribute("persist");
  const navbar = doc.getElementById(CustomizableUI.AREA_NAVBAR);

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

  const manifests = [];
  for (let i = 0; i < NUM_EXTENSIONS; ++i) {
    manifests.push({
      name: `Extension #${i}`,
      browser_action: {},
    });
  }

  const extensions = createExtensions(manifests);

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
  await Promise.all(extensions.map(extension => extension.startup()));
  await listener.promise;
  CustomizableUI.removeListener(listener);

  const originalWindowWidth = win.outerWidth;
  win.resizeTo(OVERFLOW_WINDOW_WIDTH_PX, win.outerHeight);
  const extensionIDs = extensions.map(extension => extension.id);

  await TestUtils.waitForCondition(() => {
    return (
      navbar.hasAttribute("overflowing") &&
      doc.getElementById(signpostWidgetID).getAttribute("overflowedItem") ==
        "true" &&
      doc
        .querySelector(`[data-extensionid='${extensionIDs[0]}']`)
        ?.getAttribute("overflowedItem") == "true"
    );
  });
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
    await taskFn(defaultList, unifiedExtensionList, extensionIDs);
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

    await Promise.all(extensions.map(extension => extension.unload()));
  }
}

function verifyExtensionWidget(widget, unifiedExtensionsEnabled) {
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

  await withWindowOverflowed(
    win,
    async (defaultList, unifiedExtensionList, extensionIDs) => {
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
        verifyExtensionWidget(child, true);
      }
    }
  );

  await BrowserTestUtils.closeWindow(win);
});

/**
 * Tests that if Unified Extensions are disabled, all overflowed items
 * in the toolbar go to the default overflow panel.
 */
add_task(async function test_overflowable_toolbar_legacy() {
  let win = await promiseDisableUnifiedExtensions();

  await withWindowOverflowed(
    win,
    async (defaultList, unifiedExtensionList, extensionIDs) => {
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
        verifyExtensionWidget(widget, false);
      }

      Assert.equal(
        unifiedExtensionList.children.length,
        0,
        "Unified Extension overflow list should be empty."
      );
    }
  );

  await BrowserTestUtils.closeWindow(win);
});
