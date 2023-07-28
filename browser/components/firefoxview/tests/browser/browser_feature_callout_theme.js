/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { FeatureCallout } = ChromeUtils.importESModule(
  "resource:///modules/FeatureCallout.sys.mjs"
);

async function testCallout(config) {
  const featureCallout = new FeatureCallout(config);
  const testMessage = getCalloutMessageById("FIREFOX_VIEW_FEATURE_TOUR");
  const screen = testMessage.message.content.screens[1];
  screen.anchors[0].selector = "body";
  testMessage.message.content.screens = [screen];
  featureCallout.showFeatureCallout(testMessage.message);
  await waitForCalloutScreen(config.win.document, screen.id);
  testStyles(config);
  return { featureCallout };
}

function testStyles({ win, theme }) {
  const calloutEl = win.document.querySelector(calloutSelector);
  const calloutStyle = win.getComputedStyle(calloutEl);
  for (const type of ["light", "dark", "hcm"]) {
    const appliedTheme = Object.assign(
      {},
      FeatureCallout.themePresets[theme.preset],
      theme
    );
    const scheme = appliedTheme[type];
    for (const name of FeatureCallout.themePropNames) {
      ok(
        !!calloutStyle.getPropertyValue(`--fc-${name}-${type}`) ==
          !!(scheme?.[name] || appliedTheme.all?.[name]),
        `Theme property --fc-${name}-${type} is set`
      );
    }
  }
}

add_task(async function feature_callout_chrome_theme() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  await testCallout({
    win,
    location: "chrome",
    context: "chrome",
    browser: win.gBrowser.selectedBrowser,
    theme: { preset: "chrome" },
  });
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function feature_callout_pdfjs_theme() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  await testCallout({
    win,
    location: "pdfjs",
    context: "chrome",
    browser: win.gBrowser.selectedBrowser,
    theme: { preset: "pdfjs", simulateContent: true },
  });
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function feature_callout_content_theme() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    browser =>
      testCallout({
        win: browser.contentWindow,
        location: "about:firefoxview",
        context: "content",
        theme: { preset: "themed-content" },
      })
  );
});
