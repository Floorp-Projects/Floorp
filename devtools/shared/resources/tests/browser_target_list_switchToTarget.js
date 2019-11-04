/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the TargetList API switchToTarget function

const { TargetList } = require("devtools/shared/resources/target-list");

add_task(async function() {
  // Enabled fission's pref as the TargetList is almost disabled without it
  await pushPref("devtools.browsertoolbox.fission", true);
  // Disable the preloaded process as it gets created lazily and may interfere
  // with process count assertions
  await pushPref("dom.ipc.processPrelaunch.enabled", false);
  // This preference helps destroying the content process when we close the tab
  await pushPref("dom.ipc.keepProcessesAlive.web", 1);

  const client = await createLocalClient();

  await testSwitchToTarget(client);

  await client.close();
});

async function testSwitchToTarget(client) {
  info("Test TargetList.switchToTarget method");

  const { mainRoot } = client;
  let target = await mainRoot.getMainProcess();
  const targetList = new TargetList(mainRoot, target);

  await targetList.startListening([TargetList.TYPES.FRAME]);

  is(
    targetList.targetFront,
    target,
    "The target list top level target is the main process one"
  );

  // Create the new target to switch to, a new tab with an iframe
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  const tab = await addTab(
    `data:text/html,<iframe src="data:text/html,foo"></iframe>`
  );
  const secondTarget = await mainRoot.getTab({ tab: gBrowser.selectedTab });

  const frameTargets = [];
  const onFrameAvailable = (type, newTarget, isTopLevel) => {
    is(
      type,
      TargetList.TYPES.FRAME,
      "We are only notified about frame targets"
    );
    ok(
      newTarget == target ? isTopLevel : !isTopLevel,
      "isTopLevel argument is correct"
    );
    frameTargets.push(newTarget);
  };
  await targetList.watchTargets([TargetList.TYPES.FRAME], onFrameAvailable);

  // Clear the recorded target list of all existing targets
  frameTargets.length = 0;

  target = secondTarget;
  await targetList.switchToTarget(secondTarget);

  is(
    targetList.targetFront,
    target,
    "After the switch, the top level target has been updated"
  );
  // Because JS Window Actor API isn't used yet, FrameDescriptor.getTarget returns null
  // And there is no target being created for the iframe, yet.
  // As soon as bug 1565200 is resolved, this should return two frames, including the iframe.
  is(
    frameTargets.length,
    1,
    "We get the report of two iframe when switching to the new target"
  );
  is(frameTargets[0], target);
  //is(frameTargets[1].url, "data:text/html,foo");

  targetList.stopListening([TargetList.TYPES.FRAME]);

  BrowserTestUtils.removeTab(tab);
}
