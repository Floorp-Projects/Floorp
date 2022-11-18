/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const WIDE_WINDOW_WIDTH = 1100;
const NARROW_WINDOW_WIDTH = 900;

function getTestElements(doc) {
  return {
    recentlyClosedTabs: doc.getElementById("recently-closed-tabs-container"),
    colorways: doc.getElementById("colorways"),
  };
}

function iscolorwaysBeforeRecentlyClosedTabs(document) {
  const recentlyClosedTabs = document.getElementById(
    "recently-closed-tabs-container"
  );
  const colorways = document.getElementById("colorways");
  return recentlyClosedTabs.previousElementSibling === colorways;
}

async function resizeWindow(win, width) {
  const resizePromise = BrowserTestUtils.waitForEvent(win, "resize");
  win.windowUtils.ensureDirtyRootFrame();
  info("Resizing window...");
  win.resizeTo(width, win.outerHeight);
  await resizePromise;
}

add_task(async function media_query_less_than_65em() {
  await withFirefoxView({}, async browser => {
    let win = browser.contentWindow;
    const { recentlyClosedTabs, colorways } = getTestElements(win.document);
    await resizeWindow(win, NARROW_WINDOW_WIDTH);
    is(
      recentlyClosedTabs.previousSibling,
      colorways,
      "colorway card has been positioned before recently closed tabs"
    );
  });
});

add_task(async function media_query_more_than_65em() {
  await withFirefoxView({}, async browser => {
    let win = browser.contentWindow;
    const { recentlyClosedTabs, colorways } = getTestElements(win.document);
    await resizeWindow(win, WIDE_WINDOW_WIDTH);
    is(
      recentlyClosedTabs.nextSibling,
      colorways,
      "colorway card has been positioned after recently closed tabs"
    );
  });
});
