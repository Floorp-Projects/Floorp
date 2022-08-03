/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the TargetCommand's `watchTargets` function

const TEST_URL =
  "data:text/html;charset=utf-8," + encodeURIComponent(`<div id="test"></div>`);

add_task(async function() {
  // Enabled fission's pref as the TargetCommand is almost disabled without it
  await pushPref("devtools.browsertoolbox.fission", true);
  await pushPref("devtools.browsertoolbox.scope", "everything");
  // Disable the preloaded process as it gets created lazily and may interfere
  // with process count assertions
  await pushPref("dom.ipc.processPrelaunch.enabled", false);
  // This preference helps destroying the content process when we close the tab
  await pushPref("dom.ipc.keepProcessesAlive.web", 1);

  await testWatchTargets();
  await testContentProcessTarget();
  await testThrowingInOnAvailable();
});

async function testWatchTargets() {
  info("Test TargetCommand watchTargets function");

  const commands = await CommandsFactory.forMainProcess();
  const targetCommand = commands.targetCommand;
  const { TYPES } = targetCommand;

  await targetCommand.startListening();

  // Note that ppmm also includes the parent process, which is considered as a frame rather than a process
  const originalProcessesCount = Services.ppmm.childCount - 1;

  info(
    "Check that onAvailable is called for processes already created *before* the call to watchTargets"
  );
  const targets = new Set();
  const topLevelTarget = targetCommand.targetFront;
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
  await targetCommand.watchTargets({
    types: [TYPES.PROCESS],
    onAvailable,
    onDestroyed,
  });
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
      targetCommand.unwatchTargets({
        types: [TYPES.PROCESS],
        onAvailable: onAvailable2,
      });
      resolve(targetFront);
    };
    targetCommand.watchTargets({
      types: [TYPES.PROCESS],
      onAvailable: onAvailable2,
    });
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
      targetCommand.unwatchTargets({
        types: [TYPES.PROCESS],
        onAvailable: onAvailable3,
        onDestroyed: onDestroyed3,
      });
    };
    targetCommand.watchTargets({
      types: [TYPES.PROCESS],
      onAvailable: onAvailable3,
      onDestroyed: onDestroyed3,
    });
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

  targetCommand.unwatchTargets({
    types: [TYPES.PROCESS],
    onAvailable,
    onDestroyed,
  });

  targetCommand.destroy();

  await commands.destroy();
}

async function testContentProcessTarget() {
  info("Test TargetCommand watchTargets with a content process target");

  const {
    osPid,
  } = gBrowser.selectedBrowser.browsingContext.currentWindowGlobal;
  const commands = await CommandsFactory.forProcess(osPid);
  const targetCommand = commands.targetCommand;
  const { TYPES } = targetCommand;

  await targetCommand.startListening();

  // Assert that watchTargets is only called for the top level content process target
  // as listening for additional target is only enable for the parent process target.
  // See bug 1593928.
  const targets = new Set();
  const topLevelTarget = targetCommand.targetFront;
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
    ok(false, "onDestroy should never be called in this test");
  };
  await targetCommand.watchTargets({
    types: [TYPES.PROCESS],
    onAvailable,
    onDestroyed,
  });

  // This may fail if the top level target is reported by LegacyImplementation
  // to TargetCommand and registers a duplicated entry
  is(targets.size, 1, "We were only notified about the top level target");

  targetCommand.unwatchTargets({
    types: [TYPES.PROCESS],
    onAvailable,
    onDestroyed,
  });
  targetCommand.destroy();

  await commands.destroy();
}

async function testThrowingInOnAvailable() {
  info(
    "Test TargetCommand watchTargets function when an exception is thrown in onAvailable callback"
  );

  const commands = await CommandsFactory.forMainProcess();
  const targetCommand = commands.targetCommand;
  const { TYPES } = targetCommand;

  await targetCommand.startListening();

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
  await targetCommand.watchTargets({ types: [TYPES.PROCESS], onAvailable });
  is(
    targets.size,
    originalProcessesCount - 1,
    "retrieved the expected number of processes via onAvailable. All but the first one where we have thrown."
  );

  targetCommand.destroy();

  await commands.destroy();
}
