/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the TargetCommand API around processes

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
  const targetDescriptor = await client.mainRoot.getMainProcess();

  const commands = await targetDescriptor.getCommands();
  const targetList = commands.targetCommand;
  await targetList.startListening();

  await testProcesses(targetList, targetList.targetFront);

  targetList.destroy();
  // Wait for all the targets to be fully attached so we don't have pending requests.
  await Promise.all(
    targetList
      .getAllTargets(targetList.ALL_TYPES)
      .map(t => t.attachAndInitThread(targetList))
  );

  await client.close();
});

add_task(async function() {
  const client = await createLocalClient();
  const targetDescriptor = await client.mainRoot.getMainProcess();

  const commands = await targetDescriptor.getCommands();
  const targetList = commands.targetCommand;
  await targetList.startListening();

  const created = [];
  const destroyed = [];
  const onAvailable = ({ targetFront }) => {
    created.push(targetFront);
  };
  const onDestroyed = ({ targetFront }) => {
    destroyed.push(targetFront);
  };
  await targetList.watchTargets(
    [targetList.TYPES.PROCESS],
    onAvailable,
    onDestroyed
  );
  ok(created.length > 1, "We get many content process targets");

  targetList.stopListening();

  await waitFor(
    () => created.length == destroyed.length,
    "Wait for the destruction of all content process targets when calling stopListening"
  );
  is(
    created.length,
    destroyed.length,
    "Got notification of destruction for all previously reported targets"
  );

  targetList.destroy();
  // Wait for all the targets to be fully attached so we don't have pending requests.
  await Promise.all(
    targetList
      .getAllTargets(targetList.ALL_TYPES)
      .map(t => t.attachAndInitThread(targetList))
  );

  await client.close();
});

async function testProcesses(targetList, target) {
  info("Test TargetCommand against processes");
  const { TYPES } = targetList;

  // Note that ppmm also includes the parent process, which is considered as a frame rather than a process
  const originalProcessesCount = Services.ppmm.childCount - 1;
  const processes = await targetList.getAllTargets([TYPES.PROCESS]);
  is(
    processes.length,
    originalProcessesCount,
    "Get a target for all content processes"
  );

  const processes2 = await targetList.getAllTargets([TYPES.PROCESS]);
  is(
    processes2.length,
    originalProcessesCount,
    "retrieved the same number of processes"
  );
  function sortFronts(f1, f2) {
    return f1.actorID < f2.actorID;
  }
  processes.sort(sortFronts);
  processes2.sort(sortFronts);
  for (let i = 0; i < processes.length; i++) {
    is(processes[i], processes2[i], `process ${i} targets are the same`);
  }

  // Assert that watchTargets will call the create callback for all existing frames
  const targets = new Set();
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
      targetFront == target ? targetFront.isTopLevel : !targetFront.isTopLevel,
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
      "We are never notified about the top level target destruction"
    );
    targets.delete(targetFront);
  };
  await targetList.watchTargets([TYPES.PROCESS], onAvailable, onDestroyed);
  is(
    targets.size,
    originalProcessesCount,
    "retrieved the same number of processes via watchTargets"
  );
  for (let i = 0; i < processes.length; i++) {
    ok(
      targets.has(processes[i]),
      `process ${i} targets are the same via watchTargets`
    );
  }

  const previousTargets = new Set(targets);
  // Assert that onAvailable is called for processes created *after* the call to watchTargets
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
  // so it is not reasonable to assert the size of `targets` as it may be lower than expected.
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

  // Ensure that getAllTargets still works after the call to unwatchTargets
  const processes3 = await targetList.getAllTargets([TYPES.PROCESS]);
  is(
    processes3.length,
    processCountAfterTabOpen - 1,
    "getAllTargets reports a new target"
  );
}
