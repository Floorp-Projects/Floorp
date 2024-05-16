/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the TargetCommand API with changes made to devtools.browsertoolbox.scope

const TEST_URL =
  "data:text/html;charset=utf-8," + encodeURIComponent(`<div id="test"></div>`);

add_task(async function () {
  // Do not run this test when both fission and EFT is disabled as it changes
  // the number of targets
  if (!isFissionEnabled() && !isEveryFrameTargetEnabled()) {
    return;
  }

  // Disable the preloaded process as it gets created lazily and may interfere
  // with process count assertions
  await pushPref("dom.ipc.processPrelaunch.enabled", false);
  // This preference helps destroying the content process when we close the tab
  await pushPref("dom.ipc.keepProcessesAlive.web", 1);

  // First test with multiprocess debugging enabled
  await pushPref("devtools.browsertoolbox.scope", "everything");

  const commands = await CommandsFactory.forMainProcess();
  const targetCommand = commands.targetCommand;
  await targetCommand.startListening();

  // Pass any configuration, in order to ensure attaching all the thread actors
  await commands.threadConfigurationCommand.updateConfiguration({
    skipBreakpoints: false,
  });

  const { TYPES } = targetCommand;

  const targets = new Set();
  const destroyedTargetIsModeSwitchingMap = new Map();
  const onAvailable = async ({ targetFront }) => {
    targets.add(targetFront);
  };
  const onDestroyed = ({ targetFront, isModeSwitching }) => {
    destroyedTargetIsModeSwitchingMap.set(targetFront, isModeSwitching);
    targets.delete(targetFront);
  };
  await targetCommand.watchTargets({
    types: [TYPES.PROCESS, TYPES.FRAME],
    onAvailable,
    onDestroyed,
  });
  Assert.greater(targets.size, 1, "We get many targets");

  info("Open a tab in a new content process");
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_URL,
    forceNewProcess: true,
  });

  // Verify that only PROCESS and top target have their thread actor attached.
  // We especially care about having the FRAME targets to not be attached,
  // otherwise we would have two thread actor debugging the same thread
  // with the PROCESS target already debugging all FRAME documents.
  for (const target of targets) {
    const threadFront = await target.getFront("thread");
    const isAttached = await threadFront.isAttached();
    if (target.targetType == TYPES.PROCESS) {
      ok(isAttached, "All process targets are attached");
    } else if (target.isTopLevel) {
      ok(isAttached, "The top level target is attached");
    } else {
      ok(
        !isAttached,
        "The frame targets should not be attached in multiprocess mode"
      );
    }
  }

  const firstTabProcessID =
    gBrowser.selectedTab.linkedBrowser.browsingContext.currentWindowGlobal
      .osPid;
  const firstTabInnerWindowId =
    gBrowser.selectedTab.linkedBrowser.browsingContext.currentWindowGlobal
      .innerWindowId;

  info("Wait for the tab content process target");
  const processTarget = await waitFor(() =>
    [...targets].find(
      target =>
        target.targetType == TYPES.PROCESS &&
        target.processID == firstTabProcessID
    )
  );

  info("Wait for the tab window global target");
  const windowGlobalTarget = await waitFor(() =>
    [...targets].find(
      target =>
        target.targetType == TYPES.FRAME &&
        target.innerWindowId == firstTabInnerWindowId
    )
  );

  info("Disable multiprocess debugging");
  await pushPref("devtools.browsertoolbox.scope", "parent-process");

  info("Wait for all targets but top level and workers to be destroyed");
  await waitFor(() =>
    [...targets].every(
      target =>
        target == targetCommand.targetFront || target.targetType == TYPES.WORKER
    )
  );

  ok(processTarget.isDestroyed(), "The process target is destroyed");
  ok(
    destroyedTargetIsModeSwitchingMap.get(processTarget),
    "isModeSwitching was passed to onTargetDestroyed and is true for the process target"
  );
  ok(windowGlobalTarget.isDestroyed(), "The window global target is destroyed");
  ok(
    destroyedTargetIsModeSwitchingMap.get(windowGlobalTarget),
    "isModeSwitching was passed to onTargetDestroyed and is true for the window global target"
  );

  info("Open a second tab in a new content process");
  const parentProcessTargetCount = targets.size;
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_URL,
    forceNewProcess: true,
  });
  const secondTabProcessID =
    gBrowser.selectedTab.linkedBrowser.browsingContext.currentWindowGlobal
      .osPid;
  const secondTabInnerWindowId =
    gBrowser.selectedTab.linkedBrowser.browsingContext.currentWindowGlobal
      .innerWindowId;

  await wait(1000);
  is(
    parentProcessTargetCount,
    targets.size,
    "The new tab process should be ignored and no target be created"
  );

  info("Re-enable multiprocess debugging");
  await pushPref("devtools.browsertoolbox.scope", "everything");

  await waitFor(() => {
    return [...targets].some(
      target =>
        target.targetType == TYPES.PROCESS &&
        target.processID == firstTabProcessID
    );
  }, "Wait for the first tab content process target to be available again");

  await waitFor(() => {
    return [...targets].some(
      target =>
        target.targetType == TYPES.FRAME &&
        target.innerWindowId == firstTabInnerWindowId
    );
  }, "Wait for the first tab frame target to be available again");

  await waitFor(() => {
    return [...targets].some(
      target =>
        target.targetType == TYPES.PROCESS &&
        target.processID == secondTabProcessID
    );
  }, "Wait for the second tab content process target to be available again");

  await waitFor(() => {
    return [...targets].some(
      target =>
        target.targetType == TYPES.FRAME &&
        target.innerWindowId == secondTabInnerWindowId
    );
  }, "Wait for the second tab frame target to be available again");

  info("Open a third tab in a new content process");
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_URL,
    forceNewProcess: true,
  });

  const thirdTabProcessID =
    gBrowser.selectedTab.linkedBrowser.browsingContext.currentWindowGlobal
      .osPid;
  const thirdTabInnerWindowId =
    gBrowser.selectedTab.linkedBrowser.browsingContext.currentWindowGlobal
      .innerWindowId;

  info("Wait for the third tab content process target");
  await waitFor(() =>
    [...targets].find(
      target =>
        target.targetType == TYPES.PROCESS &&
        target.processID == thirdTabProcessID
    )
  );

  info("Wait for the third tab window global target");
  await waitFor(() =>
    [...targets].find(
      target =>
        target.targetType == TYPES.FRAME &&
        target.innerWindowId == thirdTabInnerWindowId
    )
  );

  targetCommand.destroy();

  // Wait for all the targets to be fully attached so we don't have pending requests.
  await Promise.all(
    targetCommand.getAllTargets(targetCommand.ALL_TYPES).map(t => t.initialized)
  );

  await commands.destroy();
});
