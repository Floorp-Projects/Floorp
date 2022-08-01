/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the ResourceCommand clears its pending events for resources emitted from
// target destroyed when devtools.browsertoolbox.scope is updated.

const TEST_URL =
  "data:text/html;charset=utf-8," + encodeURIComponent(`<div id="test"></div>`);

add_task(async function() {
  // Do not run this test when both fission and EFT is disabled as it changes
  // the number of targets
  if (!isFissionEnabled() && !isEveryFrameTargetEnabled()) {
    ok(true, "Don't go further is both Fission and EFT are disabled");
    return;
  }

  // Enabled fission's pref as the scope pref only works when fission pref is enabled
  await pushPref("devtools.browsertoolbox.fission", true);
  // Disable the preloaded process as it gets created lazily and may interfere
  // with process count assertions
  await pushPref("dom.ipc.processPrelaunch.enabled", false);
  // This preference helps destroying the content process when we close the tab
  await pushPref("dom.ipc.keepProcessesAlive.web", 1);

  // Start with multiprocess debugging enabled
  await pushPref("devtools.browsertoolbox.scope", "everything");

  const commands = await CommandsFactory.forMainProcess();
  const targetCommand = commands.targetCommand;
  await targetCommand.startListening();
  const { TYPES } = targetCommand;

  const targets = new Set();
  const onAvailable = async ({ targetFront }) => {
    targets.add(targetFront);
  };
  const onDestroyed = () => {};
  await targetCommand.watchTargets({
    types: [TYPES.PROCESS, TYPES.FRAME],
    onAvailable,
    onDestroyed,
  });

  info("Open a tab in a new content process");
  const firstTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_URL,
    forceNewProcess: true,
  });

  const newTabInnerWindowId =
    firstTab.linkedBrowser.browsingContext.currentWindowGlobal.innerWindowId;

  info("Wait for the tab window global target");
  const windowGlobalTarget = await waitFor(() =>
    [...targets].find(
      target =>
        target.targetType == TYPES.FRAME &&
        target.innerWindowId == newTabInnerWindowId
    )
  );

  let gotTabResource = false;
  const onResourceAvailable = resources => {
    for (const resource of resources) {
      if (resource.targetFront == windowGlobalTarget) {
        gotTabResource = true;

        if (resource.targetFront.isDestroyed()) {
          ok(
            false,
            "we shouldn't get resources for the target that was destroyed when switching mode"
          );
        }
      }
    }
  };

  info("Start listening for resources");
  await commands.resourceCommand.watchResources(
    [commands.resourceCommand.TYPES.CONSOLE_MESSAGE],
    {
      onAvailable: onResourceAvailable,
      ignoreExistingResources: true,
    }
  );

  // Emit logs every ms to fill up the resourceCommand resource queue (pendingEvents)
  const intervalId = await SpecialPowers.spawn(
    firstTab.linkedBrowser,
    [],
    () => {
      let counter = 0;
      return content.wrappedJSObject.setInterval(() => {
        counter++;
        content.wrappedJSObject.console.log("STREAM_" + counter);
      }, 1);
    }
  );

  info("Wait until we get the first resource");
  await waitFor(() => gotTabResource);

  info("Disable multiprocess debugging");
  await pushPref("devtools.browsertoolbox.scope", "parent-process");

  info("Wait for the tab target to be destroyed");
  await waitFor(() => windowGlobalTarget.isDestroyed());

  info("Wait for a bit so any throttled action would have the time to occur");
  await wait(1000);

  // Stop listening for resources
  await commands.resourceCommand.unwatchResources(
    [commands.resourceCommand.TYPES.CONSOLE_MESSAGE],
    {
      onAvailable: onResourceAvailable,
    }
  );
  // And stop the interval
  await SpecialPowers.spawn(firstTab.linkedBrowser, [intervalId], id => {
    content.wrappedJSObject.clearInterval(id);
  });

  targetCommand.destroy();
  await commands.destroy();
});
