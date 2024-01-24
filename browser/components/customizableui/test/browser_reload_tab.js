"use strict";

/**
 * Check that customize mode doesn't break when its tab is reloaded.
 */
add_task(async function reload_tab() {
  let initialTab = gBrowser.selectedTab;
  let customizeTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  gCustomizeMode.setTab(customizeTab);
  let customizationContainer = document.getElementById(
    "customization-container"
  );

  is(
    customizationContainer.clientWidth,
    0,
    "Customization container shouldn't be visible (X)"
  );
  is(
    customizationContainer.clientHeight,
    0,
    "Customization container shouldn't be visible (Y)"
  );

  let customizePromise = BrowserTestUtils.waitForEvent(
    gNavToolbox,
    "customizationready"
  );
  gCustomizeMode.enter();
  await customizePromise;

  let tabReloaded = new Promise(resolve => {
    gBrowser.addTabsProgressListener({
      async onLocationChange(aBrowser) {
        if (customizeTab.linkedBrowser == aBrowser) {
          gBrowser.removeTabsProgressListener(this);
          await Promise.resolve();
          resolve();
        }
      },
    });
  });
  gBrowser.reloadTab(customizeTab);
  await tabReloaded;

  is(
    gBrowser.getIcon(customizeTab),
    "chrome://browser/skin/customize.svg",
    "Tab should have customize icon"
  );
  is(
    customizeTab.getAttribute("customizemode"),
    "true",
    "Tab should be in customize mode"
  );
  Assert.greater(
    customizationContainer.clientWidth,
    0,
    "Customization container should be visible (X)"
  );
  Assert.greater(
    customizationContainer.clientHeight,
    0,
    "Customization container should be visible (Y)"
  );

  customizePromise = BrowserTestUtils.waitForEvent(
    gNavToolbox,
    "aftercustomization"
  );
  await BrowserTestUtils.switchTab(gBrowser, initialTab);
  await customizePromise;

  customizePromise = BrowserTestUtils.waitForEvent(
    gNavToolbox,
    "customizationready"
  );
  await BrowserTestUtils.switchTab(gBrowser, customizeTab);
  await customizePromise;

  is(
    gBrowser.getIcon(customizeTab),
    "chrome://browser/skin/customize.svg",
    "Tab should still have customize icon"
  );
  is(
    customizeTab.getAttribute("customizemode"),
    "true",
    "Tab should still be in customize mode"
  );
  Assert.greater(
    customizationContainer.clientWidth,
    0,
    "Customization container should still be visible (X)"
  );
  Assert.greater(
    customizationContainer.clientHeight,
    0,
    "Customization container should still be visible (Y)"
  );

  await endCustomizing();
});
