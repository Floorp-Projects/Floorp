/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function setPref() {
  const COMPONENT_PREF = "screenshots.browser.component.enabled";
  await SpecialPowers.pushPrefEnv({
    set: [[COMPONENT_PREF, false]],
  });
  ok(!Services.prefs.getBoolPref(COMPONENT_PREF), "Extension enabled");
});

add_task(async function test_fullpageScreenshot() {
  CustomizableUI.addWidgetToArea(
    "screenshot-button",
    CustomizableUI.AREA_NAVBAR
  );
  let screenshotBtn = document.getElementById("screenshot-button");
  Assert.ok(screenshotBtn, "The screenshots button was added to the nav bar");

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);

      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      await new Promise(r => setTimeout(r, 700));

      helper.triggerUIFromToolbar();

      // await new Promise(r => setTimeout(r, 7000));

      await SpecialPowers.spawn(
        browser,
        ["#firefox-screenshots-preselection-iframe"],
        async function(iframeSelector) {
          info(
            `in waitForUIContent content function, iframeSelector: ${iframeSelector}`
          );
          let iframe;
          await ContentTaskUtils.waitForCondition(() => {
            iframe = content.document.querySelector(iframeSelector);
            if (!iframe || !ContentTaskUtils.is_visible(iframe)) {
              info("in waitForUIContent, no visible iframe yet");
              return false;
            }
            return true;
          });
          // wait a frame for the screenshots UI to finish any init
          await new content.Promise(res => content.requestAnimationFrame(res));
        }
      );

      helper.triggerUIFromToolbar();
    }
  );
});

add_task(async function setPref() {
  const COMPONENT_PREF = "screenshots.browser.component.enabled";
  await SpecialPowers.pushPrefEnv({
    set: [[COMPONENT_PREF, true]],
  });
  ok(Services.prefs.getBoolPref(COMPONENT_PREF), "Extension enabled");
});

add_task(async function test_fullpageScreenshot() {
  CustomizableUI.addWidgetToArea(
    "screenshot-button",
    CustomizableUI.AREA_NAVBAR
  );
  let screenshotBtn = document.getElementById("screenshot-button");
  Assert.ok(screenshotBtn, "The screenshots button was added to the nav bar");

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);

      // click toolbar button so panel shows
      helper.triggerUIFromToolbar();

      let panel = gBrowser.selectedBrowser.ownerDocument.querySelector(
        "#screenshotsPagePanel"
      );
      await BrowserTestUtils.waitForMutationCondition(
        panel,
        { attributes: true },
        () => {
          return BrowserTestUtils.is_visible(panel);
        }
      );
      ok(BrowserTestUtils.is_visible(panel), "Panel buttons are visible");

      helper.triggerUIFromToolbar();

      await BrowserTestUtils.waitForMutationCondition(
        panel,
        { attributes: true },
        () => {
          return BrowserTestUtils.is_hidden(panel);
        }
      );
      ok(BrowserTestUtils.is_hidden(panel), "Panel buttons are not visible");
    }
  );
});

add_task(async function setPref() {
  const SCREENSHOTS_PREF = "extensions.screenshots.disabled";
  await SpecialPowers.pushPrefEnv({
    set: [[SCREENSHOTS_PREF, true]],
  });
  ok(Services.prefs.getBoolPref(SCREENSHOTS_PREF), "Extension enabled");
});

add_task(async function test_fullpageScreenshot() {
  CustomizableUI.addWidgetToArea(
    "screenshot-button",
    CustomizableUI.AREA_NAVBAR
  );
  let screenshotBtn = document.getElementById("screenshot-button");
  Assert.ok(screenshotBtn, "The screenshots button was added to the nav bar");

  await BrowserTestUtils.withNewTab(TEST_PAGE, () => {
    Assert.equal(screenshotBtn.disabled, true, "Screenshots button is disable");
  });
});

add_task(async function setPref() {
  const SCREENSHOTS_PREF = "extensions.screenshots.disabled";
  await SpecialPowers.pushPrefEnv({
    set: [[SCREENSHOTS_PREF, false]],
  });
  ok(!Services.prefs.getBoolPref(SCREENSHOTS_PREF), "Extension enabled");
});
