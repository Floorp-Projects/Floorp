/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the TargetCommand's `watchTargets` function

const TEST_URL =
  "data:text/html;charset=utf-8," + encodeURIComponent(`<div id="test"></div>`);

add_task(async function() {
  // Enabled fission's pref as the TargetCommand is almost disabled without it
  await pushPref("devtools.browsertoolbox.fission", true);
  // Disable the preloaded process as it gets created lazily and may interfere
  // with process count assertions
  await pushPref("dom.ipc.processPrelaunch.enabled", false);
  // This preference helps destroying the content process when we close the tab
  await pushPref("dom.ipc.keepProcessesAlive.web", 1);

  const client = await createLocalClient();
  const mainRoot = client.mainRoot;

  await testWatchTargets(mainRoot);
  await testContentProcessTarget(mainRoot);
  await testThrowingInOnAvailable(mainRoot);

  await client.close();
});

async function testWatchTargets(mainRoot) {
  info("Test TargetCommand watchTargets function");

  const targetDescriptor = await mainRoot.getMainProcess();
  const commands = await targetDescriptor.getCommands();
  const targetList = commands.targetCommand;
  const { TYPES } = targetList;

  await targetList.startListening();

  // Note that ppmm also includes the parent process, which is considered as a frame rather than a process
  const originalProcessesCount = Services.ppmm.childCount - 1;

  info(
    "Check that onAvailable is called for processes already created *before* the call to watchTargets"
  );
  const targets = new Set();
  const topLevelTarget = await targetDescriptor.getTarget();
  const onAvailable = ({ targetFront }) => {
    if (targets.has(targetFront)) {
      ok(false, "The same target is notified multiple times via onAvailable");
    }
    is(
      targetFront.targetType,
      TYPES.PROCESS,
      "We are only notified about process targets"
    );
    ok(
      targetFront == topLevelTarget
        ? targetFront.isTopLevel
        : !targetFront.isTopLevel,
      "isTopLevel property is correct"
    );
    targets.add(targetFront);
  };
  const onDestroyed = ({ targetFront }) => {
    if (!targets.has(targetFront)) {
      ok(
        false,
        "A target is declared destroyed via onDestroyed without being notified via onAvailable"
      );
    }
    is(
      targetFront.targetType,
      TYPES.PROCESS,
      "We are only notified about process targets"
    );
    ok(
      !targetFront.isTopLevel,
      "We are not notified about the top level target destruction"
    );
    targets.delete(targetFront);
  };
  await targetList.watchTargets([TYPES.PROCESS], onAvailable, onDestroyed);
  is(
    targets.size,
    originalProcessesCount,
    "retrieved the expected number of processes via watchTargets"
  );
  // Start from 1 in order to ignore the parent process target, which is considered as a frame rather than a process
  for (let i = 1; i < Services.ppmm.childCount; i++) {
    const process = Services.ppmm.getChildAt(i);
    const hasTargetWithSamePID = [...targets].find(
      processTarget => processTarget.targetForm.processID == process.osPid
    );
    ok(
      hasTargetWithSamePID,
      `Process with PID ${process.osPid} has been reported via onAvailable`
    );
  }

  info(
    "Check that onAvailable is called for processes created *after* the call to watchTargets"
  );
  const previousTargets = new Set(targets);
  const onProcessCreated = new Promise(resolve => {
    const onAvailable2 = ({ targetFront }) => {
      if (previousTargets.has(targetFront)) {
        return;
      }
      targetList.unwatchTargets([TYPES.PROCESS], onAvailable2);
      resolve(targetFront);
    };
    targetList.watchTargets([TYPES.PROCESS], onAvailable2);
  });
  const tab1 = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_URL,
    forceNewProcess: true,
  });
  const createdTarget = await onProcessCreated;

  // For some reason, creating a new tab purges processes created from previous tests
  // so it is not reasonable to assert the side of `targets` as it may be lower than expected.
  ok(targets.has(createdTarget), "The new tab process is in the list");

  const processCountAfterTabOpen = targets.size;

  // Assert that onDestroyed is called for destroyed processes
  const onProcessDestroyed = new Promise(resolve => {
    const onAvailable3 = () => {};
    const onDestroyed3 = ({ targetFront }) => {
      resolve(targetFront);
      targetList.unwatchTargets([TYPES.PROCESS], onAvailable3, onDestroyed3);
    };
    targetList.watchTargets([TYPES.PROCESS], onAvailable3, onDestroyed3);
  });

  BrowserTestUtils.removeTab(tab1);

  const destroyedTarget = await onProcessDestroyed;
  is(
    targets.size,
    processCountAfterTabOpen - 1,
    "The closed tab's process has been reported as destroyed"
  );
  ok(
    !targets.has(destroyedTarget),
    "The destroyed target is no longer in the list"
  );
  is(
    destroyedTarget,
    createdTarget,
    "The destroyed target is the one that has been reported as created"
  );

  targetList.unwatchTargets([TYPES.PROCESS], onAvailable, onDestroyed);

  targetList.destroy();

  // Also destroy the descriptor so that testThrowingInOnAvailable can get a fresh
  // commands object and also a fresh TargetList instance
  targetDescriptor.destroy();
}

async function testContentProcessTarget(mainRoot) {
  info("Test TargetCommand watchTargets with a content process target");

  const processes = await mainRoot.listProcesses();
  const commands = await processes[1].getCommands();
  const targetList = commands.targetCommand;
  const { TYPES } = targetList;

  await targetList.startListening();

  // Assert that watchTargets is only called for the top level content process target
  // as listening for additional target is only enable for the parent process target.
  // See bug 1593928.
  const targets = new Set();
  const topLevelTarget = await processes[1].getTarget();
  const onAvailable = ({ targetFront }) => {
    if (targets.has(targetFront)) {
      // This may fail if the top level target is reported by LegacyImplementation
      // to TargetCommand and emits an available event for it.
      ok(false, "The same target is notified multiple times via onAvailable");
    }
    is(
      targetFront.targetType,
      TYPES.PROCESS,
      "We are only notified about process targets"
    );
    is(targetFront, topLevelTarget, "This is the existing top level target");
    ok(
      targetFront.isTopLevel,
      "We are only notified about the top level target"
    );
    targets.add(targetFront);
  };
  const onDestroyed = _ => {
    ok(false, "onDestroyed should never be called in this test");
  };
  await targetList.watchTargets([TYPES.PROCESS], onAvailable, onDestroyed);

  // This may fail if the top level target is reported by LegacyImplementation
  // to TargetCommand and registers a duplicated entry
  is(targets.size, 1, "We were only notified about the top level target");

  targetList.unwatchTargets([TYPES.PROCESS], onAvailable, onDestroyed);
  targetList.destroy();
}

async function testThrowingInOnAvailable(mainRoot) {
  info(
    "Test TargetCommand watchTargets function when an exception is thrown in onAvailable callback"
  );

  const targetDescriptor = await mainRoot.getMainProcess();
  const commands = await targetDescriptor.getCommands();
  const targetList = commands.targetCommand;
  const { TYPES } = targetList;

  await targetList.startListening();

  // Note that ppmm also includes the parent process, which is considered as a frame rather than a process
  const originalProcessesCount = Services.ppmm.childCount - 1;

  info(
    "Check that onAvailable is called for processes already created *before* the call to watchTargets"
  );
  const targets = new Set();
  let thrown = false;
  const onAvailable = ({ targetFront }) => {
    if (!thrown) {
      thrown = true;
      throw new Error("Force an exception when processing the first target");
    }
    targets.add(targetFront);
  };
  await targetList.watchTargets([TYPES.PROCESS], onAvailable);
  is(
    targets.size,
    originalProcessesCount - 1,
    "retrieved the expected number of processes via onAvailable. All but the first one where we have thrown."
  );

  targetList.destroy();
}
