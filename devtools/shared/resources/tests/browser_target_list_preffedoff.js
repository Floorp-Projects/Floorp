/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the TargetCommand API when DevTools Fission preference is false

add_task(async function() {
  // Disable the preloaded process as it gets created lazily and may interfere
  // with process count assertions
  await pushPref("dom.ipc.processPrelaunch.enabled", false);
  // This preference helps destroying the content process when we close the tab
  await pushPref("dom.ipc.keepProcessesAlive.web", 1);

  const client = await createLocalClient();
  const mainRoot = client.mainRoot;

  // Assert the limited behavior of this API with fission preffed off
  await pushPref("devtools.browsertoolbox.fission", false);

  // Test with Main process targets as top level target
  await testPreffedOffMainProcess(mainRoot);

  await client.close();
});

async function testPreffedOffMainProcess(mainRoot) {
  info(
    "Test TargetCommand when devtools's fission pref is false, via the parent process target"
  );

  const targetDescriptor = await mainRoot.getMainProcess();
  const mainProcess = await targetDescriptor.getTarget();
  const commands = await targetDescriptor.getCommands();
  const targetList = commands.targetCommand;
  const { TYPES } = targetList;
  await targetList.startListening();

  // The API should only report the top level target,
  // i.e. the Main process target, which is considered as frame
  // and not as process.
  const processes = await targetList.getAllTargets([TYPES.PROCESS]);
  is(
    processes.length,
    0,
    "We only get a frame target for the top level target"
  );
  const frames = await targetList.getAllTargets([TYPES.FRAME]);
  is(frames.length, 1, "We get only one frame when preffed-off");
  is(
    frames[0],
    mainProcess,
    "The target is the top level one via getAllTargets"
  );

  const processTargets = [];
  const onProcessAvailable = ({ targetFront }) => {
    processTargets.push(targetFront);
  };
  await targetList.watchTargets([TYPES.PROCESS], onProcessAvailable);
  is(processTargets.length, 0, "We get no process when preffed-off");
  targetList.unwatchTargets([TYPES.PROCESS], onProcessAvailable);

  const frameTargets = [];
  const onFrameAvailable = ({ targetFront }) => {
    is(
      targetFront.targetType,
      TYPES.FRAME,
      "We are only notified about frame targets"
    );
    ok(
      targetFront.isTopLevel,
      "We are only notified about the top level target"
    );
    frameTargets.push(targetFront);
  };
  await targetList.watchTargets([TYPES.FRAME], onFrameAvailable);
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
  targetList.unwatchTargets([TYPES.FRAME], onFrameAvailable);

  targetList.destroy();
}
