/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This test page should follow the overall color scheme.
// Default colors / background colors should change depending on the scheme.
const TEST_URI = `https://example.com/document-builder.sjs?html=
  <!DOCTYPE html>
    <html lang=en>
      <meta charset=utf-8>
      <meta name=color-scheme content="dark light">
      Hello!
`;
add_task(async function () {
  await addTab(TEST_URI);
  const { inspector } = await openRuleView();

  // Retrieve light and dark scheme buttons.
  const lightButton = inspector.panelDoc.querySelector(
    "#color-scheme-simulation-light-toggle"
  );
  const darkButton = inspector.panelDoc.querySelector(
    "#color-scheme-simulation-dark-toggle"
  );

  // Read the color scheme to know if we should click on the light or dark button
  // to trigger a change.
  info("Retrieve the default color scheme");
  let isDarkScheme = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => {
      return content.matchMedia("(prefers-color-scheme: dark)").matches;
    }
  );

  // Clicks on the simulation button which triggers a color-scheme change.
  // If current scheme is light, click on dark and vice-versa.
  function toggleScheme() {
    info(`Switch color scheme to ${isDarkScheme ? "light" : "dark"} mode`);
    isDarkScheme ? lightButton.click() : darkButton.click();
    isDarkScheme = !isDarkScheme;
  }

  info("Retrieve the initial (text) color of the page");
  const initialColor = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => {
      return content.getComputedStyle(content.document.body).color;
    }
  );

  toggleScheme();

  info("Wait until the color of the page changes");
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [initialColor],
    _initialColor => {
      return ContentTaskUtils.waitForCondition(() => {
        const newColor = content.getComputedStyle(content.document.body).color;
        return newColor !== _initialColor;
      });
    }
  );

  toggleScheme();

  info("Wait until the color of the page goes back to the initial value");
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [initialColor],
    _initialColor => {
      return ContentTaskUtils.waitForCondition(() => {
        const newColor = content.getComputedStyle(content.document.body).color;
        return newColor === _initialColor;
      });
    }
  );
});
