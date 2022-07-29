/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the TargetCommand API with changes made to devtools.browsertoolbox.scope

const TEST_URL =
  "data:text/html;charset=utf-8," + encodeURIComponent(`<div id="test"></div>`);

add_task(async function() {
  // Do not run this test when both fission and EFT is disabled as it changes
  // the number of targets
  if (!isFissionEnabled() && !isEveryFrameTargetEnabled()) {
    return;
  }

  // Enabled fission's pref as the scope pref only works when fission pref is enabled
  await pushPref("devtools.browsertoolbox.fission", true);
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
  ok(targets.size > 1, "We get many targets");

  info("Open a tab in a new content process");
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_URL,
    forceNewProcess: true,
  });

  const newTabProcessID =
    gBrowser.selectedTab.linkedBrowser.browsingContext.currentWindowGlobal
      .osPid;
  const newTabInnerWindowId =
    gBrowser.selectedTab.linkedBrowser.browsingContext.currentWindowGlobal
      .innerWindowId;

  info("Wait for the tab content process target");
  const processTarget = await waitFor(() =>
    [...targets].find(
      target =>
        target.targetType == TYPES.PROCESS &&
        target.processID == newTabProcessID
    )
  );

  info("Wait for the tab window global target");
  const windowGlobalTarget = await waitFor(() =>
    [...targets].find(
      target =>
        target.targetType == TYPES.FRAME &&
        target.innerWindowId == newTabInnerWindowId
    )
  );

  let multiprocessTargetCount = targets.size;

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

  await wait(1000);
  is(
    parentProcessTargetCount,
    targets.size,
    "The new tab process should be ignored and no target be created"
  );

  info("Re-enable multiprocess debugging");
  await pushPref("devtools.browsertoolbox.scope", "everything");

  // The second tab relates to one content process target and one window global target
  multiprocessTargetCount += 2;

  await waitFor(
    () => targets.size == multiprocessTargetCount,
    "Wait for all targets we used to have before disable multiprocess debugging"
  );

  info("Wait for the tab content process target to be available again");
  ok(
    [...targets].some(
      target =>
        target.targetType == TYPES.PROCESS &&
        target.processID == newTabProcessID
    ),
    "We have the tab content process target"
  );

  info("Wait for the tab window global target to be available again");
  ok(
    [...targets].some(
      target =>
        target.targetType == TYPES.FRAME &&
        target.innerWindowId == newTabInnerWindowId
    ),
    "We have the tab window global target"
  );

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
