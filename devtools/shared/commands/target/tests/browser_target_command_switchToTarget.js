/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the TargetCommand API switchToTarget function

add_task(async function testSwitchToTarget() {
  info("Test TargetCommand.switchToTarget method");

  // Create a first target to switch from, a new tab with an iframe
  const firstTab = await addTab(
    `data:text/html,<iframe src="data:text/html,foo"></iframe>`
  );
  const commands = await CommandsFactory.forTab(firstTab);
  const targetCommand = commands.targetCommand;
  const { TYPES } = targetCommand;
  await targetCommand.startListening();

  // Create a second target to switch to, a new tab with an iframe
  const secondTab = await addTab(
    `data:text/html,<iframe src="data:text/html,bar"></iframe>`
  );
  // We have to spawn a new distinct `commands` object for this new tab,
  // but we will otherwise consider the first one as the main one.
  // From this second one, we will only retrieve a new target.
  const secondCommands = await CommandsFactory.forTab(secondTab, {
    client: commands.client,
  });
  await secondCommands.targetCommand.startListening();
  const secondTarget = secondCommands.targetCommand.targetFront;

  const frameTargets = [];
  const firstTarget = targetCommand.targetFront;
  let currentTarget = targetCommand.targetFront;
  const onFrameAvailable = ({ targetFront, isTargetSwitching }) => {
    is(
      targetFront.targetType,
      TYPES.FRAME,
      "We are only notified about frame targets"
    );
    ok(
      targetFront == currentTarget
        ? targetFront.isTopLevel
        : !targetFront.isTopLevel,
      "isTopLevel property is correct"
    );
    if (targetFront.isTopLevel) {
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
  const onFrameDestroyed = ({ targetFront, isTargetSwitching }) => {
    is(
      targetFront.targetType,
      TYPES.FRAME,
      "target-destroyed: We are only notified about frame targets"
    );
    ok(
      targetFront == firstTarget
        ? targetFront.isTopLevel
        : !targetFront.isTopLevel,
      "target-destroyed: isTopLevel property is correct"
    );
    if (targetFront.isTopLevel) {
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
  await targetCommand.watchTargets({
    types: [TYPES.FRAME],
    onAvailable: onFrameAvailable,
    onDestroyed: onFrameDestroyed,
  });

  // Save the original list of targets
  const createdTargets = [...frameTargets];
  // Clear the recorded target list of all existing targets
  frameTargets.length = 0;

  currentTarget = secondTarget;
  await targetCommand.switchToTarget(secondTarget);

  is(
    targetCommand.targetFront,
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

  targetCommand.destroy();

  await commands.destroy();
  await secondCommands.destroy();

  BrowserTestUtils.removeTab(firstTab);
  BrowserTestUtils.removeTab(secondTab);
});
