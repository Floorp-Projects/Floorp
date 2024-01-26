/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that tab descriptor survives after the page navigates and changes
 * process.
 */

const EXAMPLE_COM_URI =
  "https://example.com/document-builder.sjs?html=<div id=com>com";
const EXAMPLE_ORG_URI =
  "https://example.org/document-builder.sjs?html=<div id=org>org";

add_task(async function () {
  const tab = await addTab(EXAMPLE_COM_URI);
  const toolbox = await gDevTools.showToolboxForTab(tab);
  const target = toolbox.target;
  const client = toolbox.commands.client;

  info("Retrieve the initial list of tab descriptors");
  const tabDescriptors = await client.mainRoot.listTabs();
  const tabDescriptor = tabDescriptors.find(
    d => decodeURIComponent(d.url) === EXAMPLE_COM_URI
  );
  ok(tabDescriptor, "Should have a descriptor actor for the tab");

  info("Retrieve the target corresponding to the TabDescriptor");
  const comTabTarget = await tabDescriptor.getTarget();
  is(
    target,
    comTabTarget,
    "The toolbox target is also the target associated with the tab descriptor"
  );

  await navigateTo(EXAMPLE_ORG_URI);

  info("Call list tabs again to update the tab descriptor forms");
  await client.mainRoot.listTabs();

  is(
    decodeURIComponent(tabDescriptor.url),
    EXAMPLE_ORG_URI,
    "The existing descriptor now points to the new URI"
  );

  const newTarget = toolbox.target;

  is(
    comTabTarget.actorID,
    null,
    "With Fission or server side target switching, example.com target front is destroyed"
  );
  Assert.notEqual(
    comTabTarget,
    newTarget,
    "With Fission or server side target switching, a new target was created for example.org"
  );

  const onDescriptorDestroyed = tabDescriptor.once("descriptor-destroyed");

  await removeTab(tab);

  info("Wait for descriptor destroyed event");
  await onDescriptorDestroyed;
  ok(tabDescriptor.isDestroyed(), "the descriptor front is really destroyed");
});
