/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const KEY_TO_EXPECTED_POSITION_ARRAY = [
  [
    "ArrowRight",
    {
      top: 100,
      left: 100,
      bottom: 100,
      right: 110,
    },
  ],
  [
    "ArrowDown",
    {
      top: 100,
      left: 100,
      bottom: 110,
      right: 110,
    },
  ],
  [
    "ArrowLeft",
    {
      top: 100,
      left: 100,
      bottom: 110,
      right: 100,
    },
  ],
  [
    "ArrowUp",
    {
      top: 100,
      left: 100,
      bottom: 100,
      right: 100,
    },
  ],
  ["ArrowDown", { top: 100, left: 100, bottom: 110, right: 100 }],
  [
    "ArrowRight",
    {
      top: 100,
      left: 100,
      bottom: 110,
      right: 110,
    },
  ],
  [
    "ArrowUp",
    {
      top: 100,
      left: 100,
      bottom: 100,
      right: 110,
    },
  ],
  [
    "ArrowLeft",
    {
      top: 100,
      left: 100,
      bottom: 100,
      right: 100,
    },
  ],
];

const SHIFT_PLUS_KEY_TO_EXPECTED_POSITION_ARRAY = [
  [
    "ArrowRight",
    {
      top: 100,
      left: 100,
      bottom: 100,
      right: 200,
    },
  ],
  [
    "ArrowDown",
    {
      top: 100,
      left: 100,
      bottom: 200,
      right: 200,
    },
  ],
  [
    "ArrowLeft",
    {
      top: 100,
      left: 100,
      bottom: 200,
      right: 100,
    },
  ],
  [
    "ArrowUp",
    {
      top: 100,
      left: 100,
      bottom: 100,
      right: 100,
    },
  ],
  ["ArrowDown", { top: 100, left: 100, bottom: 200, right: 100 }],
  [
    "ArrowRight",
    {
      top: 100,
      left: 100,
      bottom: 200,
      right: 200,
    },
  ],
  [
    "ArrowUp",
    {
      top: 100,
      left: 100,
      bottom: 100,
      right: 200,
    },
  ],
  [
    "ArrowLeft",
    {
      top: 100,
      left: 100,
      bottom: 100,
      right: 100,
    },
  ],
];

add_task(async function test_focusMovesToContentOnArrowKeydown() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);
      helper.triggerUIFromToolbar();
      await helper.waitForOverlay();

      let visibleButton = await helper.getPanelButton("#visible-page");
      visibleButton.focus();

      await BrowserTestUtils.waitForCondition(() => {
        return visibleButton.getRootNode().activeElement === visibleButton;
      }, "The visible button in the panel should have focus");
      info(
        `Actual focused id: ${Services.focus.focusedElement.id}. Expected focused id: ${visibleButton.id}`
      );
      is(
        Services.focus.focusedElement,
        visibleButton,
        "The visible button in the panel should have focus"
      );

      EventUtils.synthesizeKey("ArrowLeft");

      // Focus should move to the content document
      let fullpageButton = await helper.getPanelButton("#full-page");
      await BrowserTestUtils.waitForCondition(() => {
        return (
          fullpageButton.getRootNode().activeElement !== fullpageButton &&
          visibleButton.getRootNode().activeElement !== visibleButton
        );
      }, "The visible and full page buttons do not have focus");
      Assert.notEqual(
        Services.focus.focusedElement,
        visibleButton,
        "The visible button does not have focus"
      );
      Assert.notEqual(
        Services.focus.focusedElement,
        fullpageButton,
        "The full page button does not have focus"
      );

      await ContentTask.spawn(browser, null, async () => {
        let screenshotsChild = content.windowGlobalChild.getActor(
          "ScreenshotsComponent"
        );

        let { left: currentCursorX, top: currentCursorY } =
          screenshotsChild.overlay.cursorRegion;

        let rect = content.document
          .getElementById("testPageElement")
          .getBoundingClientRect();

        let repeatShiftLeft = Math.round((currentCursorX - rect.right) / 10);
        EventUtils.synthesizeKey(
          "ArrowLeft",
          { repeat: repeatShiftLeft, shiftKey: true },
          content
        );

        let repeatLeft = (currentCursorX - rect.right) % 10;
        EventUtils.synthesizeKey("ArrowLeft", { repeat: repeatLeft }, content);

        let repeatShiftRight = Math.round((currentCursorY - rect.bottom) / 10);
        EventUtils.synthesizeKey(
          "ArrowUp",
          { repeat: repeatShiftRight, shiftKey: true },
          content
        );

        let repeatRight = (currentCursorY - rect.bottom) % 10;
        EventUtils.synthesizeKey("ArrowUp", { repeat: repeatRight }, content);

        await ContentTaskUtils.waitForCondition(() => {
          return screenshotsChild.overlay.hoverElementRegion.isRegionValid;
        }, "Wait for hover element region to be valid");

        EventUtils.synthesizeKey("Enter", {}, content);
      });

      let rect = await helper.getTestPageElementRect();
      let region = await helper.getSelectionRegionDimensions();

      is(
        region.left,
        rect.left,
        "The selected region left is the same as the element left"
      );
      is(
        region.right,
        rect.right,
        "The selected region right is the same as the element right"
      );
      is(
        region.top,
        rect.top,
        "The selected region top is the same as the element top"
      );
      is(
        region.bottom,
        rect.bottom,
        "The selected region bottom is the same as the element bottom"
      );
    }
  );
});

add_task(async function test_createRegionWithKeyboard() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);

      helper.triggerUIFromToolbar();
      await helper.waitForOverlay();

      mouse.move(100, 100);

      await SpecialPowers.spawn(
        browser,
        [KEY_TO_EXPECTED_POSITION_ARRAY],
        async keyToExpectedPositionArray => {
          function assertSelectionRegionDimensions(
            actualDimensions,
            expectedDimensions
          ) {
            is(
              actualDimensions.top,
              expectedDimensions.top,
              "Top dimension is correct"
            );
            is(
              actualDimensions.left,
              expectedDimensions.left,
              "Left dimension is correct"
            );
            is(
              actualDimensions.bottom,
              expectedDimensions.bottom,
              "Bottom dimension is correct"
            );
            is(
              actualDimensions.right,
              expectedDimensions.right,
              "Right dimension is correct"
            );
          }

          let stateChangePromise = expectedState =>
            ContentTaskUtils.waitForCondition(() => {
              info(
                `got ${screenshotsChild.overlay.state}. expected ${expectedState}`
              );
              return screenshotsChild.overlay.state === expectedState;
            }, `Wait for overlay state to be ${expectedState}`);

          let screenshotsChild = content.windowGlobalChild.getActor(
            "ScreenshotsComponent"
          );

          await ContentTaskUtils.waitForCondition(() => {
            return (
              screenshotsChild.overlay.cursorRegion.left === 100 &&
              screenshotsChild.overlay.cursorRegion.top === 100
            );
          }, "Wait for cursor region to be at 10, 10");

          EventUtils.synthesizeKey(" ", {}, content);

          let expectedState = "dragging";
          await stateChangePromise(expectedState);

          for (let [key, expectedDimensions] of keyToExpectedPositionArray) {
            EventUtils.synthesizeKey(key, { repeat: 10 }, content);
            info(`Key: ${key}`);
            info(
              `Actual dimensions: ${JSON.stringify(
                screenshotsChild.overlay.selectionRegion.dimensions,
                null,
                2
              )}`
            );
            info(
              `Expected dimensions: ${JSON.stringify(
                expectedDimensions,
                null,
                2
              )}`
            );
            assertSelectionRegionDimensions(
              screenshotsChild.overlay.selectionRegion.dimensions,
              expectedDimensions
            );
          }

          EventUtils.synthesizeKey("ArrowRight", { repeat: 10 }, content);
          EventUtils.synthesizeKey("ArrowDown", { repeat: 10 }, content);

          EventUtils.synthesizeKey(" ", {}, content);
          expectedState = "selected";
          await stateChangePromise(expectedState);
        }
      );

      let region = await helper.getSelectionRegionDimensions();

      is(region.left, 100, "The selected region left is 100");
      is(region.right, 110, "The selected region right is 110");
      is(region.top, 100, "The selected region top is 100");
      is(region.bottom, 110, "The selected region bottom is 110");

      helper.triggerUIFromToolbar();
      await helper.waitForOverlayClosed();
    }
  );
});

add_task(async function test_createRegionWithKeyboardWithShift() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);

      helper.triggerUIFromToolbar();
      await helper.waitForOverlay();

      mouse.move(100, 100);

      await SpecialPowers.spawn(
        browser,
        [SHIFT_PLUS_KEY_TO_EXPECTED_POSITION_ARRAY],
        async keyToExpectedPositionArray => {
          function assertSelectionRegionDimensions(
            actualDimensions,
            expectedDimensions
          ) {
            is(
              actualDimensions.top,
              expectedDimensions.top,
              "Top dimension is correct"
            );
            is(
              actualDimensions.left,
              expectedDimensions.left,
              "Left dimension is correct"
            );
            is(
              actualDimensions.bottom,
              expectedDimensions.bottom,
              "Bottom dimension is correct"
            );
            is(
              actualDimensions.right,
              expectedDimensions.right,
              "Right dimension is correct"
            );
          }

          let stateChangePromise = expectedState =>
            ContentTaskUtils.waitForCondition(() => {
              info(
                `got ${screenshotsChild.overlay.state}. expected ${expectedState}`
              );
              return screenshotsChild.overlay.state === expectedState;
            }, `Wait for overlay state to be ${expectedState}`);

          let screenshotsChild = content.windowGlobalChild.getActor(
            "ScreenshotsComponent"
          );

          await ContentTaskUtils.waitForCondition(() => {
            return (
              screenshotsChild.overlay.cursorRegion.left === 100 &&
              screenshotsChild.overlay.cursorRegion.top === 100
            );
          }, "Wait for cursor region to be at 10, 10");

          EventUtils.synthesizeKey(" ", {}, content);

          let expectedState = "dragging";
          await stateChangePromise(expectedState);

          for (let [key, expectedDimensions] of keyToExpectedPositionArray) {
            EventUtils.synthesizeKey(
              key,
              { repeat: 10, shiftKey: true },
              content
            );
            info(`Key: ${key}`);
            info(
              `Actual dimensions: ${JSON.stringify(
                screenshotsChild.overlay.selectionRegion.dimensions,
                null,
                2
              )}`
            );
            info(
              `Expected dimensions: ${JSON.stringify(
                expectedDimensions,
                null,
                2
              )}`
            );
            assertSelectionRegionDimensions(
              screenshotsChild.overlay.selectionRegion.dimensions,
              expectedDimensions
            );
          }

          EventUtils.synthesizeKey(
            "ArrowRight",
            { repeat: 10, shiftKey: true },
            content
          );
          EventUtils.synthesizeKey(
            "ArrowDown",
            { repeat: 10, shiftKey: true },
            content
          );

          EventUtils.synthesizeKey(" ", {}, content);
          expectedState = "selected";
          await stateChangePromise(expectedState);
        }
      );

      let region = await helper.getSelectionRegionDimensions();

      is(region.left, 100, "The selected region left is 100");
      is(region.right, 200, "The selected region right is 200");
      is(region.top, 100, "The selected region top is 100");
      is(region.bottom, 200, "The selected region bottom is 200");

      helper.triggerUIFromToolbar();
      await helper.waitForOverlayClosed();
    }
  );
});
