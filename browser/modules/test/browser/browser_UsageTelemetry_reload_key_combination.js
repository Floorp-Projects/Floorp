/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This file tests page reload key combination telemetry
 */

"use strict";

const TAB_URL = "https://example.com";

var accelKey = "ctrlKey";
if (AppConstants.platform == "macosx") {
  accelKey = "metaKey";
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["toolkit.cosmeticAnimations.enabled", false]],
  });
});

add_task(async function test_pageReloadOnlyF5() {
  let histogram = TelemetryTestUtils.getAndClearHistogram(
    "FX_PAGE_RELOAD_KEY_COMBO"
  );

  await BrowserTestUtils.withNewTab(TAB_URL, async browser => {
    let p = BrowserTestUtils.browserLoaded(browser);
    EventUtils.synthesizeKey("VK_F5");
    await p;

    TelemetryTestUtils.assertHistogram(histogram, 0, 1);
  });
});

if (AppConstants.platform != "macosx") {
  add_task(async function test_pageReloadAccelF5() {
    let histogram = TelemetryTestUtils.getAndClearHistogram(
      "FX_PAGE_RELOAD_KEY_COMBO"
    );

    await BrowserTestUtils.withNewTab(TAB_URL, async browser => {
      let p = BrowserTestUtils.browserLoaded(browser);
      EventUtils.synthesizeKey("VK_F5", { accelKey: true });
      await p;

      TelemetryTestUtils.assertHistogram(histogram, 1, 1);
    });
  });
}

add_task(async function test_pageReloadAccelR() {
  let histogram = TelemetryTestUtils.getAndClearHistogram(
    "FX_PAGE_RELOAD_KEY_COMBO"
  );

  await BrowserTestUtils.withNewTab(TAB_URL, async browser => {
    let p = BrowserTestUtils.browserLoaded(browser);
    EventUtils.synthesizeKey("r", { accelKey: true });
    await p;

    TelemetryTestUtils.assertHistogram(histogram, 2, 1);
  });
});

add_task(async function test_pageReloadAccelShiftR() {
  let histogram = TelemetryTestUtils.getAndClearHistogram(
    "FX_PAGE_RELOAD_KEY_COMBO"
  );

  await BrowserTestUtils.withNewTab(TAB_URL, async browser => {
    let p = BrowserTestUtils.browserLoaded(browser);
    EventUtils.synthesizeKey("r", { accelKey: true, shiftKey: true });
    await p;

    TelemetryTestUtils.assertHistogram(histogram, 3, 1);
  });
});

add_task(async function test_pageReloadToolbar() {
  let histogram = TelemetryTestUtils.getAndClearHistogram(
    "FX_PAGE_RELOAD_KEY_COMBO"
  );

  await BrowserTestUtils.withNewTab(TAB_URL, async browser => {
    let reloadButton = document.getElementById("reload-button");
    let p = BrowserTestUtils.browserLoaded(browser);

    await BrowserTestUtils.waitForCondition(() => {
      return !reloadButton.disabled;
    });

    EventUtils.synthesizeMouseAtCenter(reloadButton, {});
    await p;

    TelemetryTestUtils.assertHistogram(histogram, 4, 1);
  });
});

add_task(async function test_pageReloadShiftToolbar() {
  let histogram = TelemetryTestUtils.getAndClearHistogram(
    "FX_PAGE_RELOAD_KEY_COMBO"
  );

  await BrowserTestUtils.withNewTab(TAB_URL, async browser => {
    let reloadButton = document.getElementById("reload-button");
    let p = BrowserTestUtils.browserLoaded(browser);

    await BrowserTestUtils.waitForCondition(() => {
      return !reloadButton.disabled;
    });

    EventUtils.synthesizeMouseAtCenter(reloadButton, { shiftKey: true });
    await p;

    TelemetryTestUtils.assertHistogram(histogram, 5, 1);
  });
});

add_task(async function test_pageReloadAccelToolbar() {
  let histogram = TelemetryTestUtils.getAndClearHistogram(
    "FX_PAGE_RELOAD_KEY_COMBO"
  );

  await BrowserTestUtils.withNewTab(TAB_URL, async browser => {
    let reloadButton = document.getElementById("reload-button");

    await BrowserTestUtils.waitForCondition(() => {
      return !reloadButton.disabled;
    });

    EventUtils.synthesizeMouseAtCenter(reloadButton, { accelKey: true });

    TelemetryTestUtils.assertHistogram(histogram, 6, 1);
    // Accel + Toolbar would open an extra tab, so we need to call removeTab twice
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  });
});

add_task(async function test_pageReloadAuxiliaryToobar() {
  let histogram = TelemetryTestUtils.getAndClearHistogram(
    "FX_PAGE_RELOAD_KEY_COMBO"
  );

  await BrowserTestUtils.withNewTab(TAB_URL, async browser => {
    let reloadButton = document.getElementById("reload-button");

    await BrowserTestUtils.waitForCondition(() => {
      return !reloadButton.disabled;
    });

    EventUtils.synthesizeMouseAtCenter(reloadButton, { button: 1 });

    TelemetryTestUtils.assertHistogram(histogram, 7, 1);
    // Auxiliary + Toolbar would open an extra tab, so we need to call removeTab twice
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  });
});
