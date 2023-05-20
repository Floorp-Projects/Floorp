/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test watch/unwatchTargets throw when provided with invalid types.

const TEST_URL = "data:text/html;charset=utf-8,invalid api usage test";

add_task(async function () {
  info("Setup the test page with workers of all types");
  const tab = await addTab(TEST_URL);

  info("Create a target list for a tab target");
  const commands = await CommandsFactory.forTab(tab);
  const targetCommand = commands.targetCommand;

  const onAvailable = function () {};

  await Assert.rejects(
    targetCommand.watchTargets({ types: [null], onAvailable }),
    /TargetCommand.watchTargets invoked with an unknown type/,
    "watchTargets should throw for null type"
  );

  await Assert.rejects(
    targetCommand.watchTargets({ types: [undefined], onAvailable }),
    /TargetCommand.watchTargets invoked with an unknown type/,
    "watchTargets should throw for undefined type"
  );

  await Assert.rejects(
    targetCommand.watchTargets({ types: ["NOT_A_TARGET"], onAvailable }),
    /TargetCommand.watchTargets invoked with an unknown type/,
    "watchTargets should throw for unknown type"
  );

  await Assert.rejects(
    targetCommand.watchTargets({
      types: [targetCommand.TYPES.FRAME, "NOT_A_TARGET"],
      onAvailable,
    }),
    /TargetCommand.watchTargets invoked with an unknown type/,
    "watchTargets should throw for unknown type mixed with a correct type"
  );

  Assert.throws(
    () => targetCommand.unwatchTargets({ types: [null], onAvailable }),
    /TargetCommand.unwatchTargets invoked with an unknown type/,
    "unwatchTargets should throw for null type"
  );

  Assert.throws(
    () => targetCommand.unwatchTargets({ types: [undefined], onAvailable }),
    /TargetCommand.unwatchTargets invoked with an unknown type/,
    "unwatchTargets should throw for undefined type"
  );

  Assert.throws(
    () =>
      targetCommand.unwatchTargets({ types: ["NOT_A_TARGET"], onAvailable }),
    /TargetCommand.unwatchTargets invoked with an unknown type/,
    "unwatchTargets should throw for unknown type"
  );

  Assert.throws(
    () =>
      targetCommand.unwatchTargets({
        types: [targetCommand.TYPES.CONSOLE_MESSAGE, "NOT_A_TARGET"],
        onAvailable,
      }),
    /TargetCommand.unwatchTargets invoked with an unknown type/,
    "unwatchTargets should throw for unknown type mixed with a correct type"
  );

  BrowserTestUtils.removeTab(tab);
  await commands.destroy();
});
