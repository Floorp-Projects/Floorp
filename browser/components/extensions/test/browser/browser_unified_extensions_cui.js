/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

loadTestSubscript("head_unified_extensions.js");

/**
 * Tests that if the addons panel is somehow open when customization mode is
 * invoked, that the panel is hidden.
 */
add_task(async function test_hide_panel_when_customizing() {
  await openExtensionsPanel();

  let panel = gUnifiedExtensions.panel;
  Assert.equal(panel.state, "open");

  let panelHidden = BrowserTestUtils.waitForPopupEvent(panel, "hidden");
  CustomizableUI.dispatchToolboxEvent("customizationstarting", {});
  await panelHidden;
  Assert.equal(panel.state, "closed");
  CustomizableUI.dispatchToolboxEvent("aftercustomization", {});
});

/**
 * Tests that if a browser action is in a collapsed toolbar area, like the
 * bookmarks toolbar, that its DOM node is overflowed in the extensions panel.
 */
add_task(async function test_extension_in_collapsed_area() {
  const extensions = createExtensions(
    [
      {
        name: "extension1",
        browser_action: { default_area: "navbar", default_popup: "popup.html" },
        browser_specific_settings: {
          gecko: { id: "unified-extensions-cui@ext-1" },
        },
      },
      {
        name: "extension2",
        browser_action: { default_area: "navbar" },
        browser_specific_settings: {
          gecko: { id: "unified-extensions-cui@ext-2" },
        },
      },
    ],
    {
      files: {
        "popup.html": `<!DOCTYPE html>
          <html>
            <body>
              <h1>test popup</h1>
              <script src="popup.js"></script>
            <body>
          </html>
        `,
        "popup.js": function () {
          browser.test.sendMessage("test-popup-opened");
        },
      },
    }
  );
  await Promise.all(extensions.map(extension => extension.startup()));

  await openExtensionsPanel();
  for (const extension of extensions) {
    let item = getUnifiedExtensionsItem(extension.id);
    Assert.ok(
      !item,
      `extension with ID=${extension.id} should not appear in the panel`
    );
  }
  await closeExtensionsPanel();

  // Move an extension to the bookmarks toolbar.
  const bookmarksToolbar = document.getElementById(
    CustomizableUI.AREA_BOOKMARKS
  );
  const firstExtensionWidgetID = AppUiTestInternals.getBrowserActionWidgetId(
    extensions[0].id
  );
  CustomizableUI.addWidgetToArea(
    firstExtensionWidgetID,
    CustomizableUI.AREA_BOOKMARKS
  );

  // Ensure that the toolbar is currently collapsed.
  await promiseSetToolbarVisibility(bookmarksToolbar, false);

  await openExtensionsPanel();
  let item = getUnifiedExtensionsItem(extensions[0].id);
  Assert.ok(
    item,
    "extension placed in the collapsed toolbar should appear in the panel"
  );

  // NOTE: ideally we would simply call `AppUiTestDelegate.clickBrowserAction()`
  // but, unfortunately, that does internally call `showBrowserAction()`, which
  // explicitly assert the group areaType that would hit a failure in this test
  // because we are moving it to AREA_BOOKMARKS.
  let widget = getBrowserActionWidget(extensions[0]).forWindow(window);
  ok(widget, "Got a widget for the extension button overflowed into the panel");
  widget.node.firstElementChild.click();

  const promisePanelBrowser = AppUiTestDelegate.awaitExtensionPanel(
    window,
    extensions[0].id,
    true
  );
  await extensions[0].awaitMessage("test-popup-opened");
  const extPanelBrowser = await promisePanelBrowser;
  ok(extPanelBrowser, "Got a action panel browser");
  closeBrowserAction(extensions[0]);

  // Now, make the toolbar visible.
  await promiseSetToolbarVisibility(bookmarksToolbar, true);

  await openExtensionsPanel();
  for (const extension of extensions) {
    let item = getUnifiedExtensionsItem(extension.id);
    Assert.ok(
      !item,
      `extension with ID=${extension.id} should not appear in the panel`
    );
  }
  await closeExtensionsPanel();

  // Hide the bookmarks toolbar again.
  await promiseSetToolbarVisibility(bookmarksToolbar, false);

  await openExtensionsPanel();
  item = getUnifiedExtensionsItem(extensions[0].id);
  Assert.ok(item, "extension should reappear in the panel");
  await closeExtensionsPanel();

  // We now empty the bookmarks toolbar but we keep the extension widget.
  for (const widgetId of CustomizableUI.getWidgetIdsInArea(
    CustomizableUI.AREA_BOOKMARKS
  ).filter(widgetId => widgetId !== firstExtensionWidgetID)) {
    CustomizableUI.removeWidgetFromArea(widgetId);
  }

  // We make the bookmarks toolbar visible again. At this point, the extension
  // widget should be re-inserted in this toolbar.
  await promiseSetToolbarVisibility(bookmarksToolbar, true);

  await openExtensionsPanel();
  for (const extension of extensions) {
    let item = getUnifiedExtensionsItem(extension.id);
    Assert.ok(
      !item,
      `extension with ID=${extension.id} should not appear in the panel`
    );
  }
  await closeExtensionsPanel();

  await Promise.all(extensions.map(extension => extension.unload()));
  await CustomizableUI.reset();
});
