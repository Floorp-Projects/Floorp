/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test LocalTabCommandsFactory

const {
  LocalTabCommandsFactory,
} = require("resource://devtools/client/framework/local-tab-commands-factory.js");

add_task(async function () {
  await testTabDescriptorWithURL("data:text/html;charset=utf-8,foo");

  // Bug 1699497: Also test against a page in the parent process
  // which can hit some race with frame-connector's frame scripts.
  await testTabDescriptorWithURL("about:robots");
});

async function testTabDescriptorWithURL(url) {
  info(`Test TabDescriptor against url ${url}\n`);
  const tab = await addTab(url);

  const commands = await LocalTabCommandsFactory.createCommandsForTab(tab);
  is(
    commands.descriptorFront.localTab,
    tab,
    "TabDescriptor's localTab is set correctly"
  );

  info(
    "Calling a second time createCommandsForTab with the same tab, will return the same commands"
  );
  const secondCommands = await LocalTabCommandsFactory.createCommandsForTab(
    tab
  );
  is(commands, secondCommands, "second commands is the same");

  // We have to involve TargetCommand in order to have a function TabDescriptor.getTarget.
  await commands.targetCommand.startListening();

  info("Wait for descriptor's target");
  const target = await commands.descriptorFront.getTarget();

  info("Call any method to ensure that each target works");
  await target.logInPage("foo");

  info("Destroy the command");
  await commands.destroy();

  gBrowser.removeCurrentTab();
}
