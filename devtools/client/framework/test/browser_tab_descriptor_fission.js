/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that tab descriptor survives after the page navigates and changes
 * process.
 */

const EXAMPLE_COM_URI =
  "http://example.com/document-builder.sjs?html=<div id=com>com";
const EXAMPLE_NET_URI =
  "http://example.net/document-builder.sjs?html=<div id=net>net";

add_task(async function() {
  const tab = await addTab(EXAMPLE_COM_URI);
  const toolbox = await gDevTools.showToolboxForTab(tab);
  const target = toolbox.target;
  const client = target.client;

  info("Retrieve the initial list of tab descriptors");
  const tabDescriptors = await client.mainRoot.listTabs();
  const tabDescriptor = tabDescriptors.find(
    d => decodeURIComponent(d.url) === EXAMPLE_COM_URI
  );
  ok(tabDescriptor, "Should have a descriptor actor for the tab");

  const firstCommands = await tabDescriptor.getCommands();
  ok(firstCommands, "Got commands");
  const secondCommands = await tabDescriptor.getCommands();
  is(
    firstCommands,
    secondCommands,
    "Multiple calls to getCommands return the same commands object"
  );

  is(
    target.descriptorFront,
    tabDescriptor,
    "The toolbox target descriptor is the same as the descriptor returned by list tab"
  );

  info("Retrieve the target corresponding to the TabDescriptor");
  const comTabTarget = await tabDescriptor.getTarget();
  is(
    target,
    comTabTarget,
    "The toolbox target is also the target associated with the tab descriptor"
  );

  await navigateTo(EXAMPLE_NET_URI);

  info("Call list tabs again to update the tab descriptor forms");
  await client.mainRoot.listTabs();

  is(
    decodeURIComponent(tabDescriptor.url),
    EXAMPLE_NET_URI,
    "The existing descriptor now points to the new URI"
  );

  const newTarget = toolbox.target;
  const newTabDescriptor = newTarget.descriptorFront;
  is(
    newTabDescriptor,
    tabDescriptor,
    "The same tab descriptor instance is reused after navigating"
  );

  if (isFissionEnabled()) {
    is(
      comTabTarget.actorID,
      null,
      "With Fission, example.com target front is destroyed"
    );
    ok(
      comTabTarget != newTarget,
      "With Fission, a new target was created for example.net"
    );
  } else {
    is(
      comTabTarget,
      newTarget,
      "Without Fission, the example.com target is reused"
    );
  }
});
