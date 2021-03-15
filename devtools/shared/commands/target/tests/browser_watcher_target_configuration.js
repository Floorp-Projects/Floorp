/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the watcher's target-configuration actor API.

add_task(async function() {
  info("Setup the test page with workers of all types");
  const client = await createLocalClient();
  const mainRoot = client.mainRoot;
  const tab = await addTab("data:text/html;charset=utf-8,Configuration actor");

  info("Create a target list for a tab target");
  const descriptor = await mainRoot.getTab({ tab });
  const commands = await descriptor.getCommands();
  const targetList = commands.targetCommand;
  await targetList.startListening();

  const { watcherFront } = targetList;
  ok(watcherFront, "A watcherFront is available on targetList");

  const targetConfigurationFront = await watcherFront.getTargetConfigurationActor();
  compareOptions(
    targetConfigurationFront.configuration,
    {},
    "Initial configuration is empty"
  );

  await targetConfigurationFront.updateConfiguration({
    cacheDisabled: true,
  });
  compareOptions(
    targetConfigurationFront.configuration,
    { cacheDisabled: true },
    "Option cacheDisabled was set"
  );

  await targetConfigurationFront.updateConfiguration({
    javascriptEnabled: false,
  });
  compareOptions(
    targetConfigurationFront.configuration,
    { cacheDisabled: true, javascriptEnabled: false },
    "Option javascriptEnabled was set"
  );

  await targetConfigurationFront.updateConfiguration({
    cacheDisabled: false,
  });
  compareOptions(
    targetConfigurationFront.configuration,
    { cacheDisabled: false, javascriptEnabled: false },
    "Option cacheDisabled was updated"
  );

  await targetConfigurationFront.updateConfiguration({
    colorSchemeSimulation: "dark",
  });
  compareOptions(
    targetConfigurationFront.configuration,
    {
      cacheDisabled: false,
      colorSchemeSimulation: "dark",
      javascriptEnabled: false,
    },
    "Option colorSchemeSimulation was set, with a string value"
  );

  targetList.destroy();
  await client.close();
});

function compareOptions(options, expected, message) {
  is(
    Object.keys(options).length,
    Object.keys(expected).length,
    message + " (wrong number of options)"
  );

  for (const key of Object.keys(expected)) {
    is(options[key], expected[key], message + ` (wrong value for ${key})`);
  }
}
