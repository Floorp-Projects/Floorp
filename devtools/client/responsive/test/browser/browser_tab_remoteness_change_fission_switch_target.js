/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for target switching.

const PAGE_ON_CHILD = "http://example.com/";
const PAGE_ON_MAIN = "about:robots";

const TEST_DPPX = 2;

add_task(async function () {
  // Set a pref for DPPX in order to assert whether the RDM is working correctly or not.
  await pushPref("devtools.responsive.viewport.pixelRatio", TEST_DPPX);

  info("Open a page which runs on the child process");
  const tab = await addTab(PAGE_ON_CHILD);
  await assertDocshell(tab, false, 0);

  info("Open RDM");
  await openRDM(tab);
  await assertDocshell(tab, true, TEST_DPPX);

  info("Load a page which runs on the main process");
  await navigateTo(PAGE_ON_MAIN);
  await assertDocshell(tab, true, TEST_DPPX);

  info("Close RDM");
  await closeRDM(tab);
  await assertDocshell(tab, false, 0);

  await removeTab(tab);
});

async function assertDocshell(tab, expectedRDMMode, expectedDPPX) {
  await asyncWaitUntil(async () => {
    const { overrideDPPX, inRDMPane } = tab.linkedBrowser.browsingContext;
    return inRDMPane === expectedRDMMode && overrideDPPX === expectedDPPX;
  });
  ok(true, "The state of the docshell is correct");
}
