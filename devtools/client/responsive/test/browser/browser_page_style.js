/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the Page Style browser menu actions make it to the viewport, instead of
// applying to the RDM UI.

const TEST_URL = `${URL_ROOT}page_style.html`;

addRDMTask(TEST_URL, async function({ ui, manager }) {
  // Store the RDM body text color for later.
  const rdmWindow = ui.toolWindow;
  const rdmTextColor = rdmWindow.getComputedStyle(rdmWindow.document.body)
    .color;

  info(
    "Trigger the no page style action and wait for the text color to change"
  );
  let onPageColorChanged = waitForContentPageTextColor(ui, "rgb(0, 0, 0)");
  let menuItem = document.querySelector("#menu_pageStyleNoStyle");
  menuItem.click();
  let color = await onPageColorChanged;

  is(
    color,
    "rgb(0, 0, 0)",
    "The text color is black, so the style was disabled"
  );

  info("Check that the RDM page style wasn't disabled");
  is(
    rdmWindow.getComputedStyle(rdmWindow.document.body).color,
    rdmTextColor,
    "The color of the text in the RDM window is correct, so that style still applies"
  );

  info(
    "Trigger the page style back and wait for the text color to change again"
  );
  onPageColorChanged = waitForContentPageTextColor(ui, "rgb(255, 0, 0)");
  menuItem = document.querySelector("#menu_pageStylePersistentOnly");
  menuItem.click();
  color = await onPageColorChanged;

  is(
    color,
    "rgb(255, 0, 0)",
    "The text color is red, so the style was enabled"
  );
});

function waitForContentPageTextColor(ui, expectedColor) {
  return ContentTask.spawn(ui.getViewportBrowser(), { expectedColor }, function(
    args
  ) {
    return new Promise(resolve => {
      const interval = content.setInterval(() => {
        const color = content.getComputedStyle(content.document.body).color;
        if (color === args.expectedColor) {
          content.clearInterval(interval);
          resolve(color);
        }
      }, 200);
    });
  });
}
