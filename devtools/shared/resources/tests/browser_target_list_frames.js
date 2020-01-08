/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the TargetList API around frames

const { TargetList } = require("devtools/shared/resources/target-list");

const FISSION_TEST_URL = URL_ROOT + "/fission_document.html";

add_task(async function() {
  // Enabled fission's pref as the TargetList is almost disabled without it
  await pushPref("devtools.browsertoolbox.fission", true);
  // Disable the preloaded process as it gets created lazily and may interfere
  // with process count assertions
  await pushPref("dom.ipc.processPrelaunch.enabled", false);
  // This preference helps destroying the content process when we close the tab
  await pushPref("dom.ipc.keepProcessesAlive.web", 1);

  const client = await createLocalClient();
  const mainRoot = client.mainRoot;

  await testBrowserFrames(mainRoot);
  await testTabFrames(mainRoot);

  await client.close();
});

async function testBrowserFrames(mainRoot) {
  info("Test TargetList against frames via the parent process target");

  const target = await mainRoot.getMainProcess();
  const targetList = new TargetList(mainRoot, target);
  await targetList.startListening();

  // Very naive sanity check against getAllTargets(frame)
  const frames = await targetList.getAllTargets(TargetList.TYPES.FRAME);
  const hasBrowserDocument = frames.find(
    frameTarget => frameTarget.url == window.location.href
  );
  ok(hasBrowserDocument, "retrieve the target for the browser document");

  // Check that calling getAllTargets(frame) return the same target instances
  const frames2 = await targetList.getAllTargets(TargetList.TYPES.FRAME);
  is(frames2.length, frames.length, "retrieved the same number of frames");

  function sortFronts(f1, f2) {
    return f1.actorID < f2.actorID;
  }
  frames.sort(sortFronts);
  frames2.sort(sortFronts);
  for (let i = 0; i < frames.length; i++) {
    is(frames[i], frames2[i], `frame ${i} targets are the same`);
  }

  // Assert that watchTargets will call the create callback for all existing frames
  const targets = [];
  const onAvailable = ({ type, targetFront, isTopLevel }) => {
    is(
      type,
      TargetList.TYPES.FRAME,
      "We are only notified about frame targets"
    );
    ok(
      targetFront == target ? isTopLevel : !isTopLevel,
      "isTopLevel argument is correct"
    );
    targets.push(targetFront);
  };
  await targetList.watchTargets([TargetList.TYPES.FRAME], onAvailable);
  is(
    targets.length,
    frames.length,
    "retrieved the same number of frames via watchTargets"
  );

  frames.sort(sortFronts);
  targets.sort(sortFronts);
  for (let i = 0; i < frames.length; i++) {
    is(
      frames[i],
      targets[i],
      `frame ${i} targets are the same via watchTargets`
    );
  }
  targetList.unwatchTargets([TargetList.TYPES.FRAME], onAvailable);

  /* NOT READY YET, need to implement frame listening
  // Open a new tab and see if the frame target is reported by watchTargets and getAllTargets
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  const tab = await addTab(TEST_URL);

  is(targets.length, frames.length + 1, "Opening a tab reported a new frame");
  is(targets[targets.length - 1].url, TEST_URL, "This frame target is about the new tab");

  const frames3 = await targetList.getAllTargets(TargetList.TYPES.FRAME);
  const hasTabDocument = frames3.find(target => target.url == TEST_URL);
  ok(hasTabDocument, "retrieve the target for tab via getAllTargets");
  */

  targetList.stopListening();
}

// For now as we do not support "real fission", for tabs, this behaves as if devtools fission pref was false
async function testTabFrames(mainRoot) {
  info("Test TargetList against frames via a tab target");

  // Create a TargetList for a given test tab
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  const tab = await addTab(FISSION_TEST_URL);
  const target = await mainRoot.getTab({ tab });
  const targetList = new TargetList(mainRoot, target);

  await targetList.startListening();

  // Check that calling getAllTargets(frame) return the same target instances
  const frames = await targetList.getAllTargets(TargetList.TYPES.FRAME);
  is(
    frames.length,
    1,
    "retrieved the top level document and the remoted frame"
  );
  is(
    frames[0].url,
    FISSION_TEST_URL,
    "The first frame is the top level document"
  );

  // Assert that watchTargets will call the create callback for all existing frames
  const targets = [];
  const onAvailable = ({ type, targetFront, isTopLevel }) => {
    is(
      type,
      TargetList.TYPES.FRAME,
      "We are only notified about frame targets"
    );
    ok(
      targetFront == target ? isTopLevel : !isTopLevel,
      "isTopLevel argument is correct"
    );
    targets.push(targetFront);
  };
  await targetList.watchTargets([TargetList.TYPES.FRAME], onAvailable);
  is(
    targets.length,
    frames.length,
    "retrieved the same number of frames via watchTargets"
  );
  for (let i = 0; i < frames.length; i++) {
    is(
      frames[i],
      targets[i],
      `frame ${i} targets are the same via watchTargets`
    );
  }
  targetList.unwatchTargets([TargetList.TYPES.FRAME], onAvailable);

  targetList.stopListening();

  BrowserTestUtils.removeTab(tab);
}
