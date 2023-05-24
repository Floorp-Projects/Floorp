/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

async function testSidebarKeyToggle(key, options, expectedSidebarId) {
  EventUtils.synthesizeMouseAtCenter(gURLBar.textbox, {});
  let promiseShown = BrowserTestUtils.waitForEvent(window, "SidebarShown");
  EventUtils.synthesizeKey(key, options);
  await promiseShown;
  Assert.equal(
    document.getElementById("sidebar-box").getAttribute("sidebarcommand"),
    expectedSidebarId
  );
  EventUtils.synthesizeKey(key, options);
  Assert.ok(!SidebarUI.isOpen);
}

add_task(async function test_sidebar_keys() {
  registerCleanupFunction(() => SidebarUI.hide());

  await testSidebarKeyToggle("b", { accelKey: true }, "viewBookmarksSidebar");

  let options = { accelKey: true, shiftKey: AppConstants.platform == "macosx" };
  await testSidebarKeyToggle("h", options, "viewHistorySidebar");
});

add_task(async function test_sidebar_in_customize_mode() {
  // Test bug 1756385 - widgets to appear unchecked in customize mode. Test that
  // the sidebar button widget doesn't appear checked, and that the sidebar
  // button toggle is inert while in customize mode.
  let { CustomizableUI } = ChromeUtils.importESModule(
    "resource:///modules/CustomizableUI.sys.mjs"
  );
  registerCleanupFunction(() => SidebarUI.hide());

  let placement = CustomizableUI.getPlacementOfWidget("sidebar-button");
  if (!(placement?.area == CustomizableUI.AREA_NAVBAR)) {
    CustomizableUI.addWidgetToArea(
      "sidebar-button",
      CustomizableUI.AREA_NAVBAR,
      0
    );
    CustomizableUI.ensureWidgetPlacedInWindow("sidebar-button", window);
    registerCleanupFunction(function () {
      CustomizableUI.removeWidgetFromArea("sidebar-button");
    });
  }

  let widgetIcon = CustomizableUI.getWidget("sidebar-button")
    .forWindow(window)
    .node?.querySelector(".toolbarbutton-icon");
  // Get the alpha value of the sidebar toggle widget's background
  let getBGAlpha = () =>
    InspectorUtils.colorToRGBA(
      getComputedStyle(widgetIcon).getPropertyValue("background-color")
    ).a;

  let promiseShown = BrowserTestUtils.waitForEvent(window, "SidebarShown");
  SidebarUI.show("viewBookmarksSidebar");
  await promiseShown;

  Assert.greater(
    getBGAlpha(),
    0,
    "Sidebar widget background should appear checked"
  );

  // Enter customize mode. This should disable the toggle and make the sidebar
  // toggle widget appear unchecked.
  let customizationReadyPromise = BrowserTestUtils.waitForEvent(
    gNavToolbox,
    "customizationready"
  );
  gCustomizeMode.enter();
  await customizationReadyPromise;

  Assert.equal(
    getBGAlpha(),
    0,
    "Sidebar widget background should appear unchecked"
  );

  // Attempt toggle - should fail in customize mode.
  await SidebarUI.toggle();
  ok(SidebarUI.isOpen, "Sidebar is still open");

  // Exit customize mode. This should re-enable the toggle and make the sidebar
  // toggle widget appear checked again, since toggle() didn't hide the sidebar.
  let afterCustomizationPromise = BrowserTestUtils.waitForEvent(
    gNavToolbox,
    "aftercustomization"
  );
  gCustomizeMode.exit();
  await afterCustomizationPromise;

  Assert.greater(
    getBGAlpha(),
    0,
    "Sidebar widget background should appear checked again"
  );

  await SidebarUI.toggle();
  ok(!SidebarUI.isOpen, "Sidebar is closed");
  Assert.equal(
    getBGAlpha(),
    0,
    "Sidebar widget background should appear unchecked"
  );
});
