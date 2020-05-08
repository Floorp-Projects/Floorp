/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the TargetList API when DevTools Fission preference is false

const { TargetList } = require("devtools/shared/resources/target-list");

const FISSION_TEST_URL = URL_ROOT_SSL + "/fission_document.html";

add_task(async function() {
  // Disable the preloaded process as it gets created lazily and may interfere
  // with process count assertions
  await pushPref("dom.ipc.processPrelaunch.enabled", false);
  // This preference helps destroying the content process when we close the tab
  await pushPref("dom.ipc.keepProcessesAlive.web", 1);

  const client = await createLocalClient();
  const mainRoot = client.mainRoot;
  const targetDescriptor = await mainRoot.getMainProcess();
  const mainProcess = await targetDescriptor.getTarget();

  // Assert the limited behavior of this API with fission preffed off
  await pushPref("devtools.browsertoolbox.fission", false);
  await pushPref("devtools.contenttoolbox.fission", false);

  // Test with Main process targets as top level target
  await testPreffedOffMainProcess(mainRoot, mainProcess);

  // Test with Tab target as top level target
  await testPreffedOffTab(mainRoot);

  await client.close();
});

async function testPreffedOffMainProcess(mainRoot, mainProcess) {
  info(
    "Test TargetList when devtools's fission pref is false, via the parent process target"
  );

  const targetList = new TargetList(mainRoot, mainProcess);
  await targetList.startListening();

  // The API should only report the top level target,
  // i.e. the Main process target, which is considered as frame
  // and not as process.
  const processes = await targetList.getAllTargets(TargetList.TYPES.PROCESS);
  is(
    processes.length,
    0,
    "We only get a frame target for the top level target"
  );
  const frames = await targetList.getAllTargets(TargetList.TYPES.FRAME);
  is(frames.length, 1, "We get only one frame when preffed-off");
  is(
    frames[0],
    mainProcess,
    "The target is the top level one via getAllTargets"
  );

  const processTargets = [];
  const onProcessAvailable = ({ type, targetFront, isTopLevel }) => {
    processTargets.push(targetFront);
  };
  await targetList.watchTargets([TargetList.TYPES.PROCESS], onProcessAvailable);
  is(processTargets.length, 0, "We get no process when preffed-off");
  targetList.unwatchTargets([TargetList.TYPES.PROCESS], onProcessAvailable);

  const frameTargets = [];
  const onFrameAvailable = ({ type, targetFront, isTopLevel }) => {
    is(
      type,
      TargetList.TYPES.FRAME,
      "We are only notified about frame targets"
    );
    ok(isTopLevel, "We are only notified about the top level target");
    frameTargets.push(targetFront);
  };
  await targetList.watchTargets([TargetList.TYPES.FRAME], onFrameAvailable);
  is(
    frameTargets.length,
    1,
    "We get one frame via watchTargets when preffed-off"
  );
  is(
    frameTargets[0],
    mainProcess,
    "The target is the top level one via watchTargets"
  );
  targetList.unwatchTargets([TargetList.TYPES.FRAME], onFrameAvailable);

  targetList.stopListening();
}

async function testPreffedOffTab(mainRoot) {
  info(
    "Test TargetList when devtools's fission pref is false, via the tab target"
  );

  // Create a TargetList for a given test tab
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  const tab = await addTab(FISSION_TEST_URL);
  const descriptor = await mainRoot.getTab({ tab });
  const target = await descriptor.getTarget();
  const targetList = new TargetList(mainRoot, target);

  await targetList.startListening();

  const processes = await targetList.getAllTargets(TargetList.TYPES.PROCESS);
  is(processes.length, 0, "Tabs don't expose any process");
  // This only reports the top level target when devtools fission preference is off
  const frames = await targetList.getAllTargets(TargetList.TYPES.FRAME);
  is(frames.length, 1, "We get only one frame when preffed-off");
  is(frames[0], target, "The target is the top level one via getAllTargets");

  const processTargets = [];
  const onProcessAvailable = ({ type, targetFront }) => {
    processTargets.push(targetFront);
  };
  await targetList.watchTargets([TargetList.TYPES.PROCESS], onProcessAvailable);
  is(processTargets.length, 0, "We get no process when preffed-off");
  targetList.unwatchTargets([TargetList.TYPES.PROCESS], onProcessAvailable);

  const frameTargets = [];
  const onFrameAvailable = ({ type, targetFront, isTopLevel }) => {
    is(
      type,
      TargetList.TYPES.FRAME,
      "We are only notified about frame targets"
    );
    ok(isTopLevel, "We are only notified about the top level target");
    frameTargets.push(targetFront);
  };
  await targetList.watchTargets([TargetList.TYPES.FRAME], onFrameAvailable);
  is(
    frameTargets.length,
    1,
    "We get one frame via watchTargets when preffed-off"
  );
  is(
    frameTargets[0],
    target,
    "The target is the top level one via watchTargets"
  );
  targetList.unwatchTargets([TargetList.TYPES.FRAME], onFrameAvailable);

  targetList.stopListening();

  BrowserTestUtils.removeTab(tab);
}
