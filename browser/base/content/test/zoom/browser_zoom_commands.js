/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the zoom commands have the expected disabled state.
 *
 * @param {Object} expectedState
 *   An object where each key represents one of the zoom commands,
 *   and the value is a boolean that is true if the command should
 *   be enabled, and false if it should be disabled.
 *
 *   The keys are "enlarge", "reduce" and "reset" for readability,
 *   and internally this function maps those keys to the appropriate
 *   commands.
 */
function assertCommandEnabledState(expectedState) {
  const COMMAND_MAP = {
    enlarge: "cmd_fullZoomEnlarge",
    reduce: "cmd_fullZoomReduce",
    reset: "cmd_fullZoomReset",
  };

  for (let commandKey in expectedState) {
    let commandID = COMMAND_MAP[commandKey];
    let command = document.getElementById(commandID);
    let expectedEnabled = expectedState[commandKey];

    Assert.equal(
      command.hasAttribute("disabled"),
      !expectedEnabled,
      `${commandID} command should have the expected enabled state.`
    );
  }
}

/**
 * Tests that the "Zoom Text Only" command is in the right checked
 * state.
 *
 * @param {boolean} isChecked
 *   True if the command should have its "checked" attribute set to
 *   "true". Otherwise, ensures that the attribute is set to "false".
 */
function assertTextZoomCommandCheckedState(isChecked) {
  let command = document.getElementById("cmd_fullZoomToggle");
  Assert.equal(
    command.getAttribute("checked"),
    "" + isChecked,
    "Text zoom command has expected checked attribute"
  );
}

/**
 * Tests that zoom commands are properly updated when changing
 * zoom levels and/or preferences.
 */
add_task(async () => {
  const TEST_PAGE_URL =
    "data:text/html;charset=utf-8,<body>test_zoom_levels</body>";

  const winUtils = Services.wm.getMostRecentWindow("").windowUtils;
  const layerManager = winUtils.layerManagerType;
  const softwareWebRenderEnabled = layerManager == "WebRender (Software)";

  await BrowserTestUtils.withNewTab(TEST_PAGE_URL, async browser => {
    let currentZoom = await FullZoomHelper.getGlobalValue();
    Assert.equal(
      currentZoom,
      1,
      "We expect to start at the default zoom level."
    );

    assertCommandEnabledState({
      enlarge: true,
      reduce: true,
      reset: false,
    });
    assertTextZoomCommandCheckedState(false);

    // We'll run two variations of this test - one with text zoom enabled,
    // and the other without.
    for (let textZoom of [true, false]) {
      info(`Running variation with textZoom set to ${textZoom}`);

      if (!textZoom && softwareWebRenderEnabled) {
        todo(
          false,
          "Skipping full zoom when using software WebRender (bug 1775498)"
        );
        continue;
      }

      await SpecialPowers.pushPrefEnv({
        set: [["browser.zoom.full", !textZoom]],
      });

      // 120% global zoom
      info("Changing default zoom by a single level");
      await FullZoomHelper.changeDefaultZoom(120);

      assertCommandEnabledState({
        enlarge: true,
        reduce: true,
        reset: true,
      });
      assertTextZoomCommandCheckedState(textZoom);

      // Now max out the zoom level.
      await FullZoomHelper.changeDefaultZoom(500);

      assertCommandEnabledState({
        enlarge: false,
        reduce: true,
        reset: true,
      });
      assertTextZoomCommandCheckedState(textZoom);

      // Now min out the zoom level.
      await FullZoomHelper.changeDefaultZoom(30);
      assertCommandEnabledState({
        enlarge: true,
        reduce: false,
        reset: true,
      });
      assertTextZoomCommandCheckedState(textZoom);

      // Now reset back to the default zoom level
      await FullZoomHelper.changeDefaultZoom(100);
      assertCommandEnabledState({
        enlarge: true,
        reduce: true,
        reset: false,
      });
      assertTextZoomCommandCheckedState(textZoom);
    }
  });
});
