/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const kToolbarName = "test-specials-toolbar";

let gTests = [
  {
    desc: "Add a toolbar with two springs and the downloads button.",
    run: function() {
      // Create the toolbar with a single spring:
      createToolbarWithPlacements(kToolbarName, ["spring"]);
      ok(document.getElementById(kToolbarName), "Toolbar should be created.");

      // Check it's there with a generated ID:
      assertAreaPlacements(kToolbarName, [/customizableui-special-spring\d+/]);
      let [springId] = getAreaWidgetIds(kToolbarName);

      // Add a second spring, check if that's there and doesn't share IDs
      CustomizableUI.addWidgetToArea("spring", kToolbarName);
      assertAreaPlacements(kToolbarName, [springId,
                                          /customizableui-special-spring\d+/]);
      let [, spring2Id] = getAreaWidgetIds(kToolbarName);

      isnot(springId, spring2Id, "Springs shouldn't have identical IDs.");

      // Try moving the downloads button to this new toolbar, between the two springs:
      CustomizableUI.addWidgetToArea("downloads-button", kToolbarName, 1);
      assertAreaPlacements(kToolbarName, [springId, "downloads-button", spring2Id]);
    }
  },
  {
    desc: "Add separators around the downloads button.",
    run: function() {
      CustomizableUI.addWidgetToArea("separator", kToolbarName, 1);
      CustomizableUI.addWidgetToArea("separator", kToolbarName, 3);
      assertAreaPlacements(kToolbarName,
                           [/customizableui-special-spring\d+/,
                            /customizableui-special-separator\d+/,
                            "downloads-button",
                            /customizableui-special-separator\d+/,
                            /customizableui-special-spring\d+/]);
      let [,separator1Id,,separator2Id,] = getAreaWidgetIds(kToolbarName);
      isnot(separator1Id, separator2Id, "Separator ids shouldn't be equal.");
    }
  },
  {
    desc: "Add spacers around the downloads button.",
    run: function() {
      CustomizableUI.addWidgetToArea("spacer", kToolbarName, 2);
      CustomizableUI.addWidgetToArea("spacer", kToolbarName, 4);
      assertAreaPlacements(kToolbarName,
                           [/customizableui-special-spring\d+/,
                            /customizableui-special-separator\d+/,
                            /customizableui-special-spacer\d+/,
                            "downloads-button",
                            /customizableui-special-spacer\d+/,
                            /customizableui-special-separator\d+/,
                            /customizableui-special-spring\d+/]);
      let [,,spacer1Id,,spacer2Id,,] = getAreaWidgetIds(kToolbarName);
      isnot(spacer1Id, spacer2Id, "Spacer ids shouldn't be equal.");
    }
  }
];

function cleanup() {
  removeCustomToolbars();
  resetCustomization();
}

function test() {
  waitForExplicitFinish();
  registerCleanupFunction(cleanup);
  runTests(gTests);
}
