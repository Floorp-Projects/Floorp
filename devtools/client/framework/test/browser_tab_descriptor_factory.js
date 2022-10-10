/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test TabDescriptorFactory

const {
  createCommandsDictionary,
} = require("resource://devtools/shared/commands/index.js");

add_task(async function() {
  await testTabDescriptorWithURL("data:text/html;charset=utf-8,foo");

  // Bug 1699497: Also test against a page in the parent process
  // which can hit some race with frame-connector's frame scripts.
  await testTabDescriptorWithURL("about:robots");
});

async function testTabDescriptorWithURL(url) {
  info(`Test TabDescriptor against url ${url}\n`);
  const tab = await addTab(url);

  const descriptor = await TabDescriptorFactory.createDescriptorForTab(tab);
  is(descriptor.localTab, tab, "TabDescriptor's localTab is set correctly");

  info(
    "Calling a second time createDescriptorForTab with the same tab, will return the same descriptor"
  );
  const secondDescriptor = await TabDescriptorFactory.createDescriptorForTab(
    tab
  );
  is(descriptor, secondDescriptor, "second descriptor is the same");

  // We have to involve TargetCommand in order to have a function TabDescriptor.getTarget.
  // With bug 1700909, all code should soon be migrated to use CommandsFactory instead of TabDescriptorFactory.
  // Making this test irrelevant and this cryptic workaround useless.
  const commands = await createCommandsDictionary(descriptor);
  await commands.targetCommand.startListening();

  info("Wait for descriptor's target");
  const target = await descriptor.getTarget();

  info("Call any method to ensure that each target works");
  await target.logInPage("foo");

  info("Destroy the descriptor");
  await descriptor.destroy();

  info("Destroy the command");
  await commands.destroy();

  gBrowser.removeCurrentTab();
}
