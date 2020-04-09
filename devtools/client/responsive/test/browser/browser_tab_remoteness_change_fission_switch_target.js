/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for target switching.

const PAGE_ON_CHILD = "http://example.com/";
const PAGE_ON_MAIN = "about:robots";

const TEST_DPPX = 2;

add_task(async function() {
  await pushPref("devtools.target-switching.enabled", true);
  // The target-switching only works with the new browserUI RDM.
  await pushPref("devtools.responsive.browserUI.enabled", true);

  // Set a pref for DPPX in order to assert whether the RDM is working correctly or not.
  await pushPref("devtools.responsive.viewport.pixelRatio", TEST_DPPX);

  info("Open a page which runs on the child process");
  const tab = await addTab(PAGE_ON_CHILD);
  await assertDocshell(tab, false, 0);

  info("Open RDM");
  const { ui } = await openRDM(tab);
  const targetForChild = ui.currentTarget;
  await assertDocshell(tab, true, TEST_DPPX);

  info("Load a page which runs on the main process");
  await load(tab.linkedBrowser, PAGE_ON_MAIN);
  await waitUntil(() => ui.currentTarget !== targetForChild);
  ok(true, "The target front is switched correctly");
  await assertDocshell(tab, true, TEST_DPPX);

  info("Close RDM");
  await closeRDM(tab);
  await assertDocshell(tab, false, 0);

  await removeTab(tab);
});

async function assertDocshell(tab, expectedRDMMode, expectedDPPX) {
  await asyncWaitUntil(async () => {
    const { overrideDPPX, inRDMPane } = await SpecialPowers.spawn(
      tab.linkedBrowser,
      [],
      () => {
        return {
          overrideDPPX: content.docShell.contentViewer.overrideDPPX,
          inRDMPane: content.docShell.browsingContext.inRDMPane,
        };
      }
    );
    return inRDMPane === expectedRDMMode && overrideDPPX === expectedDPPX;
  });
  ok(true, "The state of the docshell is correct");
}
