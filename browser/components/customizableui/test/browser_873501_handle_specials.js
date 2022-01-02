/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const kToolbarName = "test-specials-toolbar";

registerCleanupFunction(removeCustomToolbars);

// Add a toolbar with two springs and the downloads button.
add_task(async function addToolbarWith2SpringsAndDownloadsButton() {
  // Create the toolbar with a single spring:
  createToolbarWithPlacements(kToolbarName, ["spring"]);
  ok(document.getElementById(kToolbarName), "Toolbar should be created.");

  // Check it's there with a generated ID:
  assertAreaPlacements(kToolbarName, [/customizableui-special-spring\d+/]);
  let [springId] = getAreaWidgetIds(kToolbarName);

  // Add a second spring, check if that's there and doesn't share IDs
  CustomizableUI.addWidgetToArea("spring", kToolbarName);
  assertAreaPlacements(kToolbarName, [
    springId,
    /customizableui-special-spring\d+/,
  ]);
  let [, spring2Id] = getAreaWidgetIds(kToolbarName);

  isnot(springId, spring2Id, "Springs shouldn't have identical IDs.");

  // Try moving the downloads button to this new toolbar, between the two springs:
  CustomizableUI.addWidgetToArea("downloads-button", kToolbarName, 1);
  assertAreaPlacements(kToolbarName, [springId, "downloads-button", spring2Id]);
  await removeCustomToolbars();
});

// Add separators around the downloads button.
add_task(async function addSeparatorsAroundDownloadsButton() {
  createToolbarWithPlacements(kToolbarName, ["separator"]);
  ok(document.getElementById(kToolbarName), "Toolbar should be created.");

  // Check it's there with a generated ID:
  assertAreaPlacements(kToolbarName, [/customizableui-special-separator\d+/]);
  let [separatorId] = getAreaWidgetIds(kToolbarName);

  CustomizableUI.addWidgetToArea("separator", kToolbarName);
  assertAreaPlacements(kToolbarName, [
    separatorId,
    /customizableui-special-separator\d+/,
  ]);
  let [, separator2Id] = getAreaWidgetIds(kToolbarName);

  isnot(separatorId, separator2Id, "Separator ids shouldn't be equal.");

  CustomizableUI.addWidgetToArea("downloads-button", kToolbarName, 1);
  assertAreaPlacements(kToolbarName, [
    separatorId,
    "downloads-button",
    separator2Id,
  ]);
  await removeCustomToolbars();
});

// Add spacers around the downloads button.
add_task(async function addSpacersAroundDownloadsButton() {
  createToolbarWithPlacements(kToolbarName, ["spacer"]);
  ok(document.getElementById(kToolbarName), "Toolbar should be created.");

  // Check it's there with a generated ID:
  assertAreaPlacements(kToolbarName, [/customizableui-special-spacer\d+/]);
  let [spacerId] = getAreaWidgetIds(kToolbarName);

  CustomizableUI.addWidgetToArea("spacer", kToolbarName);
  assertAreaPlacements(kToolbarName, [
    spacerId,
    /customizableui-special-spacer\d+/,
  ]);
  let [, spacer2Id] = getAreaWidgetIds(kToolbarName);

  isnot(spacerId, spacer2Id, "Spacer ids shouldn't be equal.");

  CustomizableUI.addWidgetToArea("downloads-button", kToolbarName, 1);
  assertAreaPlacements(kToolbarName, [spacerId, "downloads-button", spacer2Id]);
  await removeCustomToolbars();
});

add_task(async function asyncCleanup() {
  await resetCustomization();
});
