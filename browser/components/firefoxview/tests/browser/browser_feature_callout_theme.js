/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { FeatureCallout } = ChromeUtils.importESModule(
  "resource:///modules/FeatureCallout.sys.mjs"
);

async function testCallout(config) {
  const featureCallout = new FeatureCallout(config);
  const testMessage = getCalloutMessageById(
    "FIREFOX_VIEW_FEATURE_TOUR_2_NO_CWS"
  );
  const screen = testMessage.message.content.screens.find(s => s.id);
  screen.parent_selector = "body";
  const sandbox = createSandboxWithCalloutTriggerStub(testMessage, config.page);
  featureCallout.showFeatureCallout();
  await waitForCalloutScreen(config.win.document, screen.id);
  testStyles(config.win);
  return { featureCallout, sandbox };
}

function testStyles(win) {
  const calloutEl = win.document.querySelector(calloutSelector);
  const calloutStyle = win.getComputedStyle(calloutEl);
  for (const type of ["light", "dark", "hcm"]) {
    for (const name of FeatureCallout.themePropNames) {
      ok(
        calloutStyle.getPropertyValue(`--fc-${name}-${type}`),
        `Theme property --fc-${name}-${type} is set`
      );
    }
  }
}

add_task(async function feature_callout_chrome_theme() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const { sandbox } = await testCallout({
    win,
    browser: win.gBrowser.selectedBrowser,
    prefName: "fakepref",
    page: "chrome",
    theme: { preset: "chrome" },
  });
  await BrowserTestUtils.closeWindow(win);
  sandbox.restore();
});

add_task(async function feature_callout_pdfjs_theme() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const { sandbox } = await testCallout({
    win,
    browser: win.gBrowser.selectedBrowser,
    prefName: "fakepref",
    page: "chrome",
    theme: { preset: "pdfjs", simulateContent: true },
  });
  await BrowserTestUtils.closeWindow(win);
  sandbox.restore();
});

add_task(async function feature_callout_content_theme() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { sandbox } = await testCallout({
        win: browser.contentWindow,
        prefName: "fakepref",
        page: "about:firefoxview",
        theme: { preset: "themed-content" },
      });
      sandbox.restore();
    }
  );
});
