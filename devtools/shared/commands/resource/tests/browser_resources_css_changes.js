/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceCommand API around CSS_CHANGE.

add_task(async function () {
  // Open a test tab
  const tab = await addTab(
    "data:text/html,<body style='color: lime;'>CSS Changes</body>"
  );

  const { client, resourceCommand, targetCommand } = await initResourceCommand(
    tab
  );

  // CSS_CHANGE watcher doesn't record modification made before watching,
  // so we have to start watching before doing any DOM mutation.
  await resourceCommand.watchResources([resourceCommand.TYPES.CSS_CHANGE], {
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
    "Check whether ResourceCommand catches CSS change that fired before starting to watch"
  );
  await setProperty(style.rule, 0, "color", "black");

  const availableResources = [];
  await resourceCommand.watchResources([resourceCommand.TYPES.CSS_CHANGE], {
    onAvailable: resources => availableResources.push(...resources),
  });

  // There is no guarantee that the CSS change will be already available when calling watchResources,
  // so wait for it to be received.
  info("Wait for CSS change to be received");
  await waitFor(() => availableResources.length == 1);

  assertResource(
    availableResources[0],
    { index: 0, property: "color", value: "black" },
    { index: 0, property: "color", value: "lime" }
  );

  info(
    "Check whether ResourceCommand catches CSS changes after the property was renamed and updated"
  );

  // RuleRewriter:apply will not support a simultaneous rename + setProperty.
  // Doing so would send inconsistent arguments to StyleRuleActor:setRuleText,
  // the CSS text for the rule will not match the list of modifications, which
  // would desynchronize the Changes view. Thankfully this scenario should not
  // happen when using the UI to update the rules.
  await renameProperty(style.rule, 0, "color", "background-color");
  await waitUntil(() => availableResources.length === 2);
  assertResource(
    availableResources[1],
    { index: 0, property: "background-color", value: "black" },
    { index: 0, property: "color", value: "black" }
  );

  await setProperty(style.rule, 0, "background-color", "pink");
  await waitUntil(() => availableResources.length === 3);
  assertResource(
    availableResources[2],
    { index: 0, property: "background-color", value: "pink" },
    { index: 0, property: "background-color", value: "black" }
  );

  info("Check whether ResourceCommand catches CSS change of disabling");
  await setPropertyEnabled(style.rule, 0, "background-color", false);
  await waitUntil(() => availableResources.length === 4);
  assertResource(availableResources[3], null, {
    index: 0,
    property: "background-color",
    value: "pink",
  });

  info("Check whether ResourceCommand catches CSS change of new property");
  await createProperty(style.rule, 1, "font-size", "100px");
  await waitUntil(() => availableResources.length === 5);
  assertResource(
    availableResources[4],
    { index: 1, property: "font-size", value: "100px" },
    null
  );

  info("Check whether ResourceCommand sends all resources added in this test");
  const existingResources = [];
  await resourceCommand.watchResources([resourceCommand.TYPES.CSS_CHANGE], {
    onAvailable: resources => existingResources.push(...resources),
  });
  await waitUntil(() => existingResources.length === 5);
  is(availableResources[0], existingResources[0], "1st resource is correct");
  is(availableResources[1], existingResources[1], "2nd resource is correct");
  is(availableResources[2], existingResources[2], "3rd resource is correct");
  is(availableResources[3], existingResources[3], "4th resource is correct");
  is(availableResources[4], existingResources[4], "4th resource is correct");

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

async function renameProperty(rule, index, oldName, newName) {
  const modifications = rule.startModifyingProperties({ isKnown: true });
  modifications.renameProperty(index, oldName, newName);
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
