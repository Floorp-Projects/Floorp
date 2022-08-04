/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceCommand API around CONSOLE_MESSAGE for the whole browser

const TEST_URL = URL_ROOT_SSL + "early_console_document.html";

add_task(async function() {
  // Enable Multiprocess Browser Toolbox (it's still disabled for non-Nightly builds).
  await pushPref("devtools.browsertoolbox.fission", true);
  await pushPref("devtools.browsertoolbox.scope", "everything");

  const {
    client,
    resourceCommand,
    targetCommand,
  } = await initMultiProcessResourceCommand();

  const d = Date.now();
  const CACHED_MESSAGE_TEXT = `cached-${d}`;
  const LIVE_MESSAGE_TEXT = `live-${d}`;

  info(
    "Log some messages *before* calling ResourceCommand.watchResources in order to " +
      "assert the behavior of already existing messages."
  );
  console.log(CACHED_MESSAGE_TEXT);

  info("Wait for existing browser mochitest log");
  const { onResource } = await resourceCommand.waitForNextResource(
    resourceCommand.TYPES.CONSOLE_MESSAGE,
    {
      ignoreExistingResources: false,
      predicate({ message }) {
        return message.arguments[0] === CACHED_MESSAGE_TEXT;
      },
    }
  );
  const existingMsg = await onResource;
  ok(existingMsg, "The existing log was retrieved");
  is(
    existingMsg.isAlreadyExistingResource,
    true,
    "isAlreadyExistingResource is true for the existing message"
  );

  const {
    onResource: onMochitestRuntimeLog,
  } = await resourceCommand.waitForNextResource(
    resourceCommand.TYPES.CONSOLE_MESSAGE,
    {
      ignoreExistingResources: false,
      predicate({ message }) {
        return message.arguments[0] === LIVE_MESSAGE_TEXT;
      },
    }
  );
  console.log(LIVE_MESSAGE_TEXT);

  info("Wait for runtime browser mochitest log");
  const runtimeLogResource = await onMochitestRuntimeLog;
  ok(runtimeLogResource, "The runtime log was retrieved");
  is(
    runtimeLogResource.isAlreadyExistingResource,
    false,
    "isAlreadyExistingResource is false for the runtime message"
  );

  const { onResource: onEarlyLog } = await resourceCommand.waitForNextResource(
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
