/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceWatcher API around CONSOLE_MESSAGE for the whole browser

const {
  ResourceWatcher,
} = require("devtools/shared/resources/resource-watcher");

const TEST_URL = URL_ROOT_SSL + "early_console_document.html";

add_task(async function() {
  // Enable Multiprocess Browser Toolbox (it's still disabled for non-Nightly builds).
  await pushPref("devtools.browsertoolbox.fission", true);

  const {
    client,
    resourceWatcher,
    targetList,
  } = await initMultiProcessResourceWatcher();

  info(
    "Log some messages *before* calling ResourceWatcher.watchResources in order to " +
      "assert the behavior of already existing messages."
  );
  console.log("foobar");

  info("Wait for existing browser mochitest log");
  await waitForNextResource(
    resourceWatcher,
    ResourceWatcher.TYPES.CONSOLE_MESSAGE,
    {
      ignoreExistingResources: false,
      predicate({ message }) {
        return message.arguments[0] === "foobar";
      },
    }
  );
  ok(true, "The existing log was retrieved");

  // We can't use waitForNextResource here as we have to ensure
  // waiting for watchResource resolution before doing the console log.
  let resolveMochitestRuntimeLog;
  const onMochitestRuntimeLog = new Promise(resolve => {
    resolveMochitestRuntimeLog = resolve;
  });
  const onAvailable = resources => {
    if (
      resources.some(resource => resource.message.arguments[0] == "foobar2")
    ) {
      resourceWatcher.unwatchResources(
        [ResourceWatcher.TYPES.CONSOLE_MESSAGE],
        { onAvailable }
      );
      resolveMochitestRuntimeLog();
    }
  };
  await resourceWatcher.watchResources(
    [ResourceWatcher.TYPES.CONSOLE_MESSAGE],
    {
      ignoreExistingResources: true,
      onAvailable,
    }
  );
  console.log("foobar2");

  info("Wait for runtime browser mochitest log");
  await onMochitestRuntimeLog;
  ok(true, "The runtime log was retrieved");

  const onEarlyLog = waitForNextResource(
    resourceWatcher,
    ResourceWatcher.TYPES.CONSOLE_MESSAGE,
    {
      ignoreExistingResources: true,
      predicate({ message }) {
        return message.arguments[0] === "early-page-log";
      },
    }
  );
  await addTab(TEST_URL);
  info("Wait for early page log");
  await onEarlyLog;
  ok(true, "The early page log was retrieved");

  targetList.destroy();
  await client.close();
});
