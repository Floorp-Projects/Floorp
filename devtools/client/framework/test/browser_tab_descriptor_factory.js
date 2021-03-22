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

  const descriptorPromises = [];
  const sharedDescriptor = await TabDescriptorFactory.createDescriptorForTab(
    tab
  );
  descriptorPromises.push(sharedDescriptor);
  is(
    sharedDescriptor.localTab,
    tab,
    "TabDescriptor's localTab is set correctly"
  );

  info(
    "Calling a second time createDescriptorForTab with the same tab, will return the same descriptor"
  );
  const secondDescriptor = await TabDescriptorFactory.createDescriptorForTab(
    tab
  );
  is(sharedDescriptor, secondDescriptor, "second descriptor is the same");

  info(
    "forceCreationForWebextension allows to spawn new descriptor for the same tab"
  );
  const webExtDescriptor = await TabDescriptorFactory.createDescriptorForTab(
    tab,
    { forceCreationForWebextension: true }
  );
  isnot(
    sharedDescriptor,
    webExtDescriptor,
    "web extension descriptor is a new one"
  );
  is(
    webExtDescriptor.localTab,
    tab,
    "web ext descriptor still refers to the same tab"
  );
  descriptorPromises.push(webExtDescriptor);

  info("Instantiate many descriptor in parallel");
  for (let i = 0; i < 10; i++) {
    const descriptor = TabDescriptorFactory.createDescriptorForTab(tab, {
      forceCreationForWebextension: true,
    });
    descriptorPromises.push(descriptor);
  }

  info("Wait for all descriptor to be resolved");
  const descriptors = await Promise.all(descriptorPromises);

  info("Wait for all targets to be created");
  const targets = await Promise.all(
    descriptors.map(async descriptor => {
      return descriptor.getTarget();
    })
  );

  info("Call any method to ensure that each target works");
  await Promise.all(
    targets.map(async target => {
      await target.logInPage("foo");
    })
  );

  info("Destroy all the descriptors");
  await Promise.all(
    descriptors.map(async descriptor => {
      await descriptor.destroy();
    })
  );

  gBrowser.removeCurrentTab();
}
