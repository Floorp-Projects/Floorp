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
  const firstTarget = await mainRoot.getMainProcess();
  const targetList = new TargetList(mainRoot, firstTarget);

  await targetList.startListening([TargetList.TYPES.FRAME]);

  is(
    targetList.targetFront,
    firstTarget,
    "The target list top level target is the main process one"
  );

  // Create the new target to switch to, a new tab with an iframe
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  const tab = await addTab(
    `data:text/html,<iframe src="data:text/html,foo"></iframe>`
  );
  const secondTarget = await mainRoot.getTab({ tab: gBrowser.selectedTab });

  const frameTargets = [];
  let currentTarget = firstTarget;
  const onFrameAvailable = ({
    type,
    targetFront,
    isTopLevel,
    isTargetSwitching,
  }) => {
    is(
      type,
      TargetList.TYPES.FRAME,
      "We are only notified about frame targets"
    );
    ok(
      targetFront == currentTarget ? isTopLevel : !isTopLevel,
      "isTopLevel argument is correct"
    );
    if (isTopLevel) {
      // When calling watchTargets, this will be false, but it will be true when calling switchToTarget
      is(
        isTargetSwitching,
        currentTarget == secondTarget,
        "target switching boolean is correct"
      );
    } else {
      ok(!isTargetSwitching, "for now, only top level target can be switched");
    }
    frameTargets.push(targetFront);
  };
  const destroyedTargets = [];
  const onFrameDestroyed = ({
    type,
    targetFront,
    isTopLevel,
    isTargetSwitching,
  }) => {
    is(
      type,
      TargetList.TYPES.FRAME,
      "target-destroyed: We are only notified about frame targets"
    );
    ok(
      targetFront == firstTarget ? isTopLevel : !isTopLevel,
      "target-destroyed: isTopLevel argument is correct"
    );
    if (isTopLevel) {
      is(
        isTargetSwitching,
        true,
        "target-destroyed: target switching boolean is correct"
      );
    } else {
      ok(
        !isTargetSwitching,
        "target-destroyed: for now, only top level target can be switched"
      );
    }
    destroyedTargets.push(targetFront);
  };
  await targetList.watchTargets(
    [TargetList.TYPES.FRAME],
    onFrameAvailable,
    onFrameDestroyed
  );

  // Save the original list of targets
  const createdTargets = [...frameTargets];
  // Clear the recorded target list of all existing targets
  frameTargets.length = 0;

  currentTarget = secondTarget;
  await targetList.switchToTarget(secondTarget);

  is(
    targetList.targetFront,
    currentTarget,
    "After the switch, the top level target has been updated"
  );
  // Because JS Window Actor API isn't used yet, FrameDescriptor.getTarget returns null
  // And there is no target being created for the iframe, yet.
  // As soon as bug 1565200 is resolved, this should return two frames, including the iframe.
  is(
    frameTargets.length,
    1,
    "We get the report of the top level iframe when switching to the new target"
  );
  is(frameTargets[0], currentTarget);
  //is(frameTargets[1].url, "data:text/html,foo");

  // Ensure that all the targets reported before the call to switchToTarget
  // are reported as destroyed while calling switchToTarget.
  is(
    destroyedTargets.length,
    createdTargets.length,
    "All targets original reported are destroyed"
  );
  for (const newTarget of createdTargets) {
    ok(
      destroyedTargets.includes(newTarget),
      "Each originally target is reported as destroyed"
    );
  }

  targetList.stopListening([TargetList.TYPES.FRAME]);

  BrowserTestUtils.removeTab(tab);
}
