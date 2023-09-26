/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the watcher's target-configuration actor API.

add_task(async function () {
  info("Setup the test page with workers of all types");
  const tab = await addTab("data:text/html;charset=utf-8,Configuration actor");

  info("Create a target list for a tab target");
  const commands = await CommandsFactory.forTab(tab);

  const targetConfigurationCommand = commands.targetConfigurationCommand;
  const targetCommand = commands.targetCommand;
  await targetCommand.startListening();

  compareOptions(
    targetConfigurationCommand.configuration,
    {},
    "Initial configuration is empty"
  );

  await targetConfigurationCommand.updateConfiguration({
    cacheDisabled: true,
  });
  compareOptions(
    targetConfigurationCommand.configuration,
    { cacheDisabled: true },
    "Option cacheDisabled was set"
  );

  await targetConfigurationCommand.updateConfiguration({
    javascriptEnabled: false,
  });
  compareOptions(
    targetConfigurationCommand.configuration,
    { cacheDisabled: true, javascriptEnabled: false },
    "Option javascriptEnabled was set"
  );

  await targetConfigurationCommand.updateConfiguration({
    cacheDisabled: false,
  });
  compareOptions(
    targetConfigurationCommand.configuration,
    { cacheDisabled: false, javascriptEnabled: false },
    "Option cacheDisabled was updated"
  );

  await targetConfigurationCommand.updateConfiguration({
    colorSchemeSimulation: "dark",
  });
  compareOptions(
    targetConfigurationCommand.configuration,
    {
      cacheDisabled: false,
      colorSchemeSimulation: "dark",
      javascriptEnabled: false,
    },
    "Option colorSchemeSimulation was set, with a string value"
  );

  await targetConfigurationCommand.updateConfiguration({
    setTabOffline: true,
  });
  compareOptions(
    targetConfigurationCommand.configuration,
    {
      cacheDisabled: false,
      colorSchemeSimulation: "dark",
      javascriptEnabled: false,
      setTabOffline: true,
    },
    "Option setTabOffline was set on"
  );

  await targetConfigurationCommand.updateConfiguration({
    setTabOffline: false,
  });
  compareOptions(
    targetConfigurationCommand.configuration,
    {
      setTabOffline: false,
      cacheDisabled: false,
      colorSchemeSimulation: "dark",
      javascriptEnabled: false,
    },
    "Option setTabOffline was set off"
  );

  targetCommand.destroy();
  await commands.destroy();
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
