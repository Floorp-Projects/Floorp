/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the TargetCommand's when detaching the top target
//
// Do this with the "remote tab" codepath, which will avoid
// destroying the DevToolsClient when the target is destroyed.
// Otherwise, with "local tab", the client is closed and everything is destroy
// on both client and server side.

const TEST_URL = "data:text/html,test-page";

add_task(async function () {
  info(" ### Test detaching the top target");

  // Create a TargetCommand for a given test tab
  const tab = await addTab(TEST_URL);

  info("Create a first commands, which will destroy its top target");
  const commands = await CommandsFactory.forRemoteTab(
    tab.linkedBrowser.browserId
  );
  const targetCommand = commands.targetCommand;

  // We have to start listening in order to ensure having a targetFront available
  await targetCommand.startListening();

  info("Call any target front method, to ensure it works fine");
  await targetCommand.targetFront.focus();

  // Simulate a toolbox closing by the cleanup of TargetCommand.
  // This will stop watching for all target types and destroy all target actors/fronts.
  await targetCommand.stopListening();

  info(
    "Now create a second commands after destroy, to see if we can spawn a new, functional target"
  );
  const secondCommands = await CommandsFactory.forRemoteTab(
    tab.linkedBrowser.browserId,
    {
      client: commands.client,
    }
  );
  const secondTargetCommand = secondCommands.targetCommand;

  // We have to start listening in order to ensure having a targetFront available
  await secondTargetCommand.startListening();

  info("Call any target front method, to ensure it works fine");
  await secondTargetCommand.targetFront.focus();

  BrowserTestUtils.removeTab(tab);

  info("Close the two commands");
  await commands.destroy();
  await secondCommands.destroy();
});
