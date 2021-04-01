/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test TabDescriptorFactory

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

  info("Wait for descriptor's target");
  const target = await descriptor.getTarget();

  info("Call any method to ensure that each target works");
  await target.logInPage("foo");

  info("Destroy the descriptor");
  await descriptor.destroy();

  gBrowser.removeCurrentTab();
}
