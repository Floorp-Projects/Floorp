/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceWatcher API around CSS_CHANGE.

add_task(async function() {
  // Open a test tab
  const tab = await addTab(
    "data:text/html,<body style='color: lime;'>CSS Changes</body>"
  );

  const { client, resourceWatcher, targetCommand } = await initResourceWatcher(
    tab
  );

  // CSS_CHANGE watcher doesn't record modification made before watching,
  // so we have to start watching before doing any DOM mutation.
  await resourceWatcher.watchResources([resourceWatcher.TYPES.CSS_CHANGE], {
    onAvailable: () => {},
  });

  const { walker } = await targetCommand.targetFront.getFront("inspector");
  const nodeList = await walker.querySelectorAll(walker.rootNode, "body");
  const body = (await nodeList.items())[0];
  const style = (
    await body.inspectorFront.pageStyle.getApplied(body, {
      skipPseudo: false,
    })
  )[0];

  info(
    "Check whether ResourceWatcher catches CSS change that fired before starting to watch"
  );
  await setProperty(style.rule, 0, "color", "black");

  const availableResources = [];
  await resourceWatcher.watchResources([resourceWatcher.TYPES.CSS_CHANGE], {
    onAvailable: resources => availableResources.push(...resources),
  });
  assertResource(
    availableResources[0],
    { index: 0, property: "color", value: "black" },
    { index: 0, property: "color", value: "lime" }
  );

  info(
    "Check whether ResourceWatcher catches CSS change after the property changed"
  );
  await setProperty(style.rule, 0, "background-color", "pink");
  await waitUntil(() => availableResources.length === 2);
  assertResource(
    availableResources[1],
    { index: 0, property: "background-color", value: "pink" },
    { index: 0, property: "color", value: "black" }
  );

  info("Check whether ResourceWatcher catches CSS change of disabling");
  await setPropertyEnabled(style.rule, 0, "background-color", false);
  await waitUntil(() => availableResources.length === 3);
  assertResource(availableResources[2], null, {
    index: 0,
    property: "background-color",
    value: "pink",
  });

  info("Check whether ResourceWatcher catches CSS change of new property");
  await createProperty(style.rule, 1, "font-size", "100px");
  await waitUntil(() => availableResources.length === 4);
  assertResource(
    availableResources[3],
    { index: 1, property: "font-size", value: "100px" },
    null
  );

  info("Check whether ResourceWatcher sends all resources added in this test");
  const existingResources = [];
  await resourceWatcher.watchResources([resourceWatcher.TYPES.CSS_CHANGE], {
    onAvailable: resources => existingResources.push(...resources),
  });
  await waitUntil(() => existingResources.length === 4);
  is(availableResources[0], existingResources[0], "1st resource is correct");
  is(availableResources[1], existingResources[1], "2nd resource is correct");
  is(availableResources[2], existingResources[2], "3rd resource is correct");
  is(availableResources[3], existingResources[3], "4th resource is correct");

  targetCommand.destroy();
  await client.close();
});

function assertResource(resource, expectedAddedChange, expectedRemovedChange) {
  if (expectedAddedChange) {
    is(resource.add.length, 1, "The number of added changes is correct");
    assertChange(resource.add[0], expectedAddedChange);
  } else {
    is(resource.add, null, "There is no added changes");
  }

  if (expectedRemovedChange) {
    is(resource.remove.length, 1, "The number of removed changes is correct");
    assertChange(resource.remove[0], expectedRemovedChange);
  } else {
    is(resource.remove, null, "There is no removed changes");
  }
}

function assertChange(change, expected) {
  is(change.index, expected.index, "The index of change is correct");
  is(change.property, expected.property, "The property of change is correct");
  is(change.value, expected.value, "The value of change is correct");
}

async function setProperty(rule, index, property, value) {
  const modifications = rule.startModifyingProperties({ isKnown: true });
  modifications.setProperty(index, property, value, "");
  await modifications.apply();
}

async function createProperty(rule, index, property, value) {
  const modifications = rule.startModifyingProperties({ isKnown: true });
  modifications.createProperty(index, property, value, "", true);
  await modifications.apply();
}

async function setPropertyEnabled(rule, index, property, isEnabled) {
  const modifications = rule.startModifyingProperties({ isKnown: true });
  modifications.setPropertyEnabled(index, property, isEnabled);
  await modifications.apply();
}
