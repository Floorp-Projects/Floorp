/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test watch/unwatchResources throw when provided with invalid types.

const TEST_URI = "data:text/html;charset=utf-8,invalid api usage test";

add_task(async function() {
  const tab = await addTab(TEST_URI);

  const { client, resourceCommand, targetCommand } = await initResourceCommand(
    tab
  );

  const onAvailable = function() {};

  await Assert.rejects(
    resourceCommand.watchResources([null], { onAvailable }),
    /ResourceCommand\.watchResources invoked with an unknown type/,
    "watchResources should throw for null type"
  );

  await Assert.rejects(
    resourceCommand.watchResources([undefined], { onAvailable }),
    /ResourceCommand\.watchResources invoked with an unknown type/,
    "watchResources should throw for undefined type"
  );

  await Assert.rejects(
    resourceCommand.watchResources(["NOT_A_RESOURCE"], { onAvailable }),
    /ResourceCommand\.watchResources invoked with an unknown type/,
    "watchResources should throw for unknown type"
  );

  await Assert.rejects(
    resourceCommand.watchResources(
      [resourceCommand.TYPES.CONSOLE_MESSAGE, "NOT_A_RESOURCE"],
      { onAvailable }
    ),
    /ResourceCommand\.watchResources invoked with an unknown type/,
    "watchResources should throw for unknown type mixed with a correct type"
  );

  await Assert.throws(
    () => resourceCommand.unwatchResources([null], { onAvailable }),
    /ResourceCommand\.unwatchResources invoked with an unknown type/,
    "unwatchResources should throw for null type"
  );

  await Assert.throws(
    () => resourceCommand.unwatchResources([undefined], { onAvailable }),
    /ResourceCommand\.unwatchResources invoked with an unknown type/,
    "unwatchResources should throw for undefined type"
  );

  await Assert.throws(
    () => resourceCommand.unwatchResources(["NOT_A_RESOURCE"], { onAvailable }),
    /ResourceCommand\.unwatchResources invoked with an unknown type/,
    "unwatchResources should throw for unknown type"
  );

  await Assert.throws(
    () =>
      resourceCommand.unwatchResources(
        [resourceCommand.TYPES.CONSOLE_MESSAGE, "NOT_A_RESOURCE"],
        { onAvailable }
      ),
    /ResourceCommand\.unwatchResources invoked with an unknown type/,
    "unwatchResources should throw for unknown type mixed with a correct type"
  );

  targetCommand.destroy();
  await client.close();
});
