/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceCommand API around CONSOLE_MESSAGE for the whole browser

const TEST_URL = URL_ROOT_SSL + "early_console_document.html";

add_task(async function() {
  // Enable Multiprocess Browser Toolbox (it's still disabled for non-Nightly builds).
  await pushPref("devtools.browsertoolbox.fission", true);

  const {
    client,
    resourceCommand,
    targetCommand,
  } = await initMultiProcessResourceCommand();

  info(
    "Log some messages *before* calling ResourceCommand.watchResources in order to " +
      "assert the behavior of already existing messages."
  );
  console.log("foobar");

  info("Wait for existing browser mochitest log");
  const existingMsg = await waitForNextResource(
    resourceCommand,
    resourceCommand.TYPES.CONSOLE_MESSAGE,
    {
      ignoreExistingResources: false,
      predicate({ message }) {
        return message.arguments[0] === "foobar";
      },
    }
  );
  ok(existingMsg, "The existing log was retrieved");
  is(
    existingMsg.isAlreadyExistingResource,
    true,
    "isAlreadyExistingResource is true for the existing message"
  );

  // We can't use waitForNextResource here as we have to ensure
  // waiting for watchResource resolution before doing the console log.
  let resolveMochitestRuntimeLog;
  const onMochitestRuntimeLog = new Promise(resolve => {
    resolveMochitestRuntimeLog = resolve;
  });
  const onAvailable = resources => {
    const runtimeLogResource = resources.find(
      resource => resource.message.arguments[0] == "foobar2"
    );
    if (runtimeLogResource) {
      resourceCommand.unwatchResources(
        [resourceCommand.TYPES.CONSOLE_MESSAGE],
        { onAvailable }
      );
      resolveMochitestRuntimeLog(runtimeLogResource);
    }
  };
  await resourceCommand.watchResources(
    [resourceCommand.TYPES.CONSOLE_MESSAGE],
    {
      ignoreExistingResources: true,
      onAvailable,
    }
  );
  console.log("foobar2");

  info("Wait for runtime browser mochitest log");
  const runtimeLogResource = await onMochitestRuntimeLog;
  ok(runtimeLogResource, "The runtime log was retrieved");
  is(
    runtimeLogResource.isAlreadyExistingResource,
    false,
    "isAlreadyExistingResource is false for the runtime message"
  );

  const onEarlyLog = waitForNextResource(
    resourceCommand,
    resourceCommand.TYPES.CONSOLE_MESSAGE,
    {
      ignoreExistingResources: true,
      predicate({ message }) {
        return message.arguments[0] === "early-page-log";
      },
    }
  );
  await addTab(TEST_URL);
  info("Wait for early page log");
  const earlyResource = await onEarlyLog;
  ok(earlyResource, "The early page log was retrieved");
  is(
    earlyResource.isAlreadyExistingResource,
    false,
    "isAlreadyExistingResource is false for the early message"
  );

  targetCommand.destroy();
  await client.close();
});
