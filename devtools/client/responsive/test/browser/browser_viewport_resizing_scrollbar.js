/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test scrollbar appearance after viewport resizing, with and without
// meta viewport support.

Services.scriptloader.loadSubScript(
  "chrome://mochikit/content/tests/SimpleTest/WindowSnapshot.js",
  this
);

// The quest for a TEST_ROOT: we have to choose a way of addressing the RDM document
// such that two things can happen:
// 1) addRDMTask can load it.
// 2) WindowSnapshot can take a picture of it.

// The WindowSnapshot does not work cross-origin. We can't use a data URI, because those
// are considered cross-origin.

// let TEST_ROOT = "";

// We can't use a relative URL, because addRDMTask can't load local files.
// TEST_ROOT = "";

// We can't use a mochi.test URL, because it's cross-origin.
// TEST_ROOT =
//   "http://mochi.test:8888/browser/devtools/client/responsive/test/browser/";

// We can't use a chrome URL, because it triggers an assertion: RDM only available for
// remote tabs.
// TEST_ROOT =
//   "chrome://mochitests/content/browser/devtools/client/responsive/test/browser/";

// So if we had an effective TEST_ROOT, we'd use it here and run our test. But we don't.
// The proposed "file_meta_1000_div.html" would just contain the html specified below as
// a data URI.
// const TEST_URL = TEST_ROOT + "file_meta_1000_div.html";

// Instead we're going to mess with a security preference to allow a data URI to be
// treated as same-origin. This doesn't work either for reasons that I don't understand.

const TEST_URL =
  "data:text/html;charset=utf-8," +
  "<head>" +
  '<meta name="viewport" content="width=device-width, initial-scale=1"/>' +
  "</head>" +
  '<body><div style="background:orange; width:1000px; height:1000px"></div></body>';

addRDMTask(TEST_URL, async function({ ui, manager }) {
  // Turn on the prefs that force overlay scrollbars to always be visible.
  await SpecialPowers.pushPrefEnv({
    set: [["layout.testing.overlay-scrollbars.always-visible", true]],
  });

  info("--- Starting viewport test output ---");

  const browser = ui.getViewportBrowser();

  const expected = [false, true];
  for (const e of expected) {
    const message = "Meta Viewport " + (e ? "ON" : "OFF");

    // Ensure meta viewport is set.
    info(message + " setting meta viewport support.");
    await setTouchAndMetaViewportSupport(ui, e.metaSupport);

    // Get to the initial size and snapshot the window.
    await setViewportSizeAndAwaitReflow(ui, manager, 300, 600);
    const initialSnapshot = await snapshotWindow(browser);

    // Move to the rotated size.
    await setViewportSizeAndAwaitReflow(ui, manager, 600, 300);

    // Reload the window.
    await reloadBrowser();

    // Go back to the initial size and take another snapshot.
    await setViewportSizeAndAwaitReflow(ui, manager, 300, 600);
    const finalSnapshot = await snapshotWindow(browser);

    const result = compareSnapshots(initialSnapshot, finalSnapshot, true);
    is(result[2], result[1], "Window snapshots should match.");
  }
});
