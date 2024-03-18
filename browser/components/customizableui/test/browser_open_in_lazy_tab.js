"use strict";

/**
 * Check that customize mode can be loaded in a lazy tab.
 */
add_task(async function open_customize_mode_in_lazy_tab() {
  let tab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
    createLazyBrowser: true,
  });
  gCustomizeMode.setTab(tab);

  is(tab.linkedPanel, null, "Tab should be lazy");

  let title = gNavigatorBundle.getFormattedString("customizeMode.tabTitle", [
    document.getElementById("bundle_brand").getString("brandShortName"),
  ]);
  is(tab.label, title, "Tab should have correct title");

  let customizePromise = BrowserTestUtils.waitForEvent(
    gNavToolbox,
    "customizationready"
  );
  gCustomizeMode.enter();
  await customizePromise;

  is(
    tab.getAttribute("customizemode"),
    "true",
    "Tab should be in customize mode"
  );

  let customizationContainer = document.getElementById(
    "customization-container"
  );
  is(
    customizationContainer.hidden,
    false,
    "Customization container should be visible"
  );

  await endCustomizing();
});
