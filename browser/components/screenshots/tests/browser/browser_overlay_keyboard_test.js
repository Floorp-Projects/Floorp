/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
const KEY_TO_EXPECTED_POSITION_ARRAY = [
  [
    "ArrowRight",
    {
      top: 10,
      left: 20,
      bottom: 20,
      right: 30,
    },
  ],
  [
    "ArrowDown",
    {
      top: 20,
      left: 20,
      bottom: 30,
      right: 30,
    },
  ],
  [
    "ArrowLeft",
    {
      top: 20,
      left: 10,
      bottom: 30,
      right: 20,
    },
  ],
  [
    "ArrowUp",
    {
      top: 10,
      left: 10,
      bottom: 20,
      right: 20,
    },
  ],
  ["ArrowDown", { top: 20, left: 10, bottom: 30, right: 20 }],
  [
    "ArrowRight",
    {
      top: 20,
      left: 20,
      bottom: 30,
      right: 30,
    },
  ],
  [
    "ArrowUp",
    {
      top: 10,
      left: 20,
      bottom: 20,
      right: 30,
    },
  ],
  [
    "ArrowLeft",
    {
      top: 10,
      left: 10,
      bottom: 20,
      right: 20,
    },
  ],
];

const SHIFT_PLUS_KEY_TO_EXPECTED_POSITION_ARRAY = [
  [
    "ArrowRight",
    {
      top: 100,
      left: 200,
      bottom: 200,
      right: 300,
    },
  ],
  [
    "ArrowDown",
    {
      top: 200,
      left: 200,
      bottom: 300,
      right: 300,
    },
  ],
  [
    "ArrowLeft",
    {
      top: 200,
      left: 100,
      bottom: 300,
      right: 200,
    },
  ],
  [
    "ArrowUp",
    {
      top: 100,
      left: 100,
      bottom: 200,
      right: 200,
    },
  ],
  ["ArrowDown", { top: 200, left: 100, bottom: 300, right: 200 }],
  [
    "ArrowRight",
    {
      top: 200,
      left: 200,
      bottom: 300,
      right: 300,
    },
  ],
  [
    "ArrowUp",
    {
      top: 100,
      left: 200,
      bottom: 200,
      right: 300,
    },
  ],
  [
    "ArrowLeft",
    {
      top: 100,
      left: 100,
      bottom: 200,
      right: 200,
    },
  ],
];

/**
 *
 */
add_task(async function test_moveRegionWithKeyboard() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);

      let contentInfo = await helper.getContentDimensions();
      ok(contentInfo, "Got dimensions back from the content");

      helper.triggerUIFromToolbar();

      await helper.waitForOverlay();

      // Because the screenshots state won't go from draggingReady to
      // dragging until the diagonal distance is 40px, we have to resize
      // it to get the region to 10px x 10px
      await helper.dragOverlay(10, 10, 100, 100);
      mouse.down(100, 100);
      await helper.assertStateChange("resizing");
      mouse.move(20, 20);
      mouse.up(20, 20);
      await helper.assertStateChange("selected");

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

          let screenshotsChild = content.windowGlobalChild.getActor(
            "ScreenshotsComponent"
          );

          // Test moving each corner of the region
          screenshotsChild.overlay.topLeftMover.focus();

          // Check that initial position is correct
          assertSelectionRegionDimensions(
            screenshotsChild.overlay.selectionRegion.dimensions,
            {
              top: 10,
              left: 10,
              bottom: 20,
              right: 20,
            }
          );

          for (let [key, expectedDimensions] of keyToExpectedPositionArray) {
            EventUtils.synthesizeKey(key, { repeat: 20 }, content);
            assertSelectionRegionDimensions(
              screenshotsChild.overlay.selectionRegion.dimensions,
              expectedDimensions
            );
          }

          // Test moving the highlight element
          screenshotsChild.overlay.highlightEl.focus();

          // Check that initial position is correct
          assertSelectionRegionDimensions(
            screenshotsChild.overlay.selectionRegion.dimensions,
            {
              top: 10,
              left: 10,
              bottom: 20,
              right: 20,
            }
          );

          for (let [key, expectedDimensions] of keyToExpectedPositionArray) {
            EventUtils.synthesizeKey(key, { repeat: 10 }, content);
            assertSelectionRegionDimensions(
              screenshotsChild.overlay.selectionRegion.dimensions,
              expectedDimensions
            );
          }
        }
      );

      helper.triggerUIFromToolbar();
      await helper.waitForOverlayClosed();
    }
  );
});

add_task(async function test_moveRegionWithKeyboardWithShift() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);

      let contentInfo = await helper.getContentDimensions();
      ok(contentInfo, "Got dimensions back from the content");

      helper.triggerUIFromToolbar();

      await helper.waitForOverlay();

      await helper.dragOverlay(100, 100, 200, 200);

      await SpecialPowers.spawn(
        browser,
        [SHIFT_PLUS_KEY_TO_EXPECTED_POSITION_ARRAY],
        async shiftPlusKeyToExpectedPositionArray => {
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

          let screenshotsChild = content.windowGlobalChild.getActor(
            "ScreenshotsComponent"
          );

          // Test moving each corner of the region
          screenshotsChild.overlay.topLeftMover.focus();

          // Check that initial position is correct
          assertSelectionRegionDimensions(
            screenshotsChild.overlay.selectionRegion.dimensions,
            {
              top: 100,
              left: 100,
              bottom: 200,
              right: 200,
            }
          );

          for (let [
            key,
            expectedDimensions,
          ] of shiftPlusKeyToExpectedPositionArray) {
            EventUtils.synthesizeKey(
              key,
              { repeat: 20, shiftKey: true },
              content
            );
            assertSelectionRegionDimensions(
              screenshotsChild.overlay.selectionRegion.dimensions,
              expectedDimensions
            );
          }

          // Test moving the highlight element
          screenshotsChild.overlay.highlightEl.focus();

          // Check that initial position is correct
          assertSelectionRegionDimensions(
            screenshotsChild.overlay.selectionRegion.dimensions,
            {
              top: 100,
              left: 100,
              bottom: 200,
              right: 200,
            }
          );

          for (let [
            key,
            expectedDimensions,
          ] of shiftPlusKeyToExpectedPositionArray) {
            EventUtils.synthesizeKey(
              key,
              { repeat: 10, shiftKey: true },
              content
            );
            assertSelectionRegionDimensions(
              screenshotsChild.overlay.selectionRegion.dimensions,
              expectedDimensions
            );
          }
        }
      );

      helper.triggerUIFromToolbar();
      await helper.waitForOverlayClosed();
    }
  );
});

add_task(async function test_moveRegionWithKeyboardWithAccelKey() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);

      await helper.scrollContentWindow(100, 100);

      let contentWindowDimensions = await helper.getContentDimensions();
      ok(contentWindowDimensions, "Got dimensions back from the content");

      helper.triggerUIFromToolbar();

      await helper.waitForOverlay();

      await helper.dragOverlay(100, 100, 200, 200);

      info("Test moving the the highlight element");
      await SpecialPowers.spawn(
        browser,
        [contentWindowDimensions],
        async contentDimensions => {
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

          let { scrollX, scrollY, clientHeight, clientWidth } =
            contentDimensions;

          const HIGHLIGHT_CONTROL_PLUS_KEY_TO_EXPECTED_POSITION_ARRAY = [
            [
              "ArrowRight",
              {
                top: scrollY,
                left: scrollX + clientWidth - 100,
                bottom: scrollY + 100,
                right: scrollX + clientWidth,
              },
            ],
            [
              "ArrowDown",
              {
                top: scrollY + clientHeight - 100,
                left: scrollX + clientWidth - 100,
                bottom: scrollY + clientHeight,
                right: scrollX + clientWidth,
              },
            ],
            [
              "ArrowLeft",
              {
                top: scrollY + clientHeight - 100,
                left: scrollX,
                bottom: scrollY + clientHeight,
                right: scrollX + 100,
              },
            ],
            [
              "ArrowUp",
              {
                top: scrollY,
                left: scrollX,
                bottom: scrollY + 100,
                right: scrollX + 100,
              },
            ],
            [
              "ArrowDown",
              {
                top: scrollY + clientHeight - 100,
                left: scrollX,
                bottom: scrollY + clientHeight,
                right: scrollX + 100,
              },
            ],
            [
              "ArrowRight",
              {
                top: scrollY + clientHeight - 100,
                left: scrollX + clientWidth - 100,
                bottom: scrollY + clientHeight,
                right: scrollX + clientWidth,
              },
            ],
            [
              "ArrowUp",
              {
                top: scrollY,
                left: scrollX + clientWidth - 100,
                bottom: scrollY + 100,
                right: scrollX + clientWidth,
              },
            ],
            [
              "ArrowLeft",
              {
                top: scrollY,
                left: scrollX,
                bottom: scrollY + 100,
                right: scrollX + 100,
              },
            ],
          ];

          let screenshotsChild = content.windowGlobalChild.getActor(
            "ScreenshotsComponent"
          );
          screenshotsChild.overlay.highlightEl.focus();

          // Move the region around in a clockwise direction
          // Check that original position is correct
          assertSelectionRegionDimensions(
            screenshotsChild.overlay.selectionRegion.dimensions,
            {
              top: 100,
              left: 100,
              bottom: 200,
              right: 200,
            }
          );

          for (let [
            key,
            expectedDimensions,
          ] of HIGHLIGHT_CONTROL_PLUS_KEY_TO_EXPECTED_POSITION_ARRAY) {
            EventUtils.synthesizeKey(key, { accelKey: true }, content);
            assertSelectionRegionDimensions(
              screenshotsChild.overlay.selectionRegion.dimensions,
              expectedDimensions
            );
          }
        }
      );

      mouse.click(300, 300);
      await helper.assertStateChange("crosshairs");

      await helper.dragOverlay(200, 200, 300, 300);

      info("Test moving the corners clockwise");
      await SpecialPowers.spawn(
        browser,
        [contentWindowDimensions],
        async contentDimensions => {
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

          let { scrollX, scrollY, clientHeight, clientWidth } =
            contentDimensions;

          const CONTROL_PLUS_KEY_TO_EXPECTED_POSITION_ARRAY = [
            [
              "ArrowRight",
              {
                top: scrollY + 100,
                left: scrollX + 100 + 100,
                bottom: scrollY + 100 + 100,
                right: scrollX + clientWidth,
              },
            ],
            [
              "ArrowDown",
              {
                top: scrollY + 100 + 100,
                left: scrollX + 100 + 100,
                bottom: scrollY + clientHeight,
                right: scrollX + clientWidth,
              },
            ],
            [
              "ArrowLeft",
              {
                top: scrollY + 100 + 100,
                left: scrollX,
                bottom: scrollY + clientHeight,
                right: scrollX + 100 + 100,
              },
            ],
            [
              "ArrowUp",
              {
                top: scrollY,
                left: scrollX,
                bottom: scrollY + 100 + 100,
                right: scrollX + 100 + 100,
              },
            ],
          ];

          let screenshotsChild = content.windowGlobalChild.getActor(
            "ScreenshotsComponent"
          );
          screenshotsChild.overlay.topLeftMover.focus();

          // Move the region around in a clockwise direction
          // Check that original position is correct
          assertSelectionRegionDimensions(
            screenshotsChild.overlay.selectionRegion.dimensions,
            {
              top: 200,
              left: 200,
              bottom: 300,
              right: 300,
            }
          );

          for (let [
            key,
            expectedDimensions,
          ] of CONTROL_PLUS_KEY_TO_EXPECTED_POSITION_ARRAY) {
            EventUtils.synthesizeKey(key, { accelKey: true }, content);
            assertSelectionRegionDimensions(
              screenshotsChild.overlay.selectionRegion.dimensions,
              expectedDimensions
            );
          }
        }
      );

      mouse.click(400, 400);
      await helper.assertStateChange("crosshairs");

      await helper.dragOverlay(200, 200, 300, 300);

      info("Test moving the corners counter clockwise");
      await SpecialPowers.spawn(
        browser,
        [contentWindowDimensions],
        async contentDimensions => {
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

          let { scrollX, scrollY, clientHeight, clientWidth } =
            contentDimensions;

          const CONTROL_PLUS_KEY_TO_EXPECTED_POSITION_ARRAY = [
            [
              "ArrowDown",
              {
                top: scrollY + 100 + 100,
                left: scrollX + 100,
                bottom: scrollY + clientHeight,
                right: scrollX + 100 + 100,
              },
            ],
            [
              "ArrowRight",
              {
                top: scrollY + 100 + 100,
                left: scrollX + 100 + 100,
                bottom: scrollY + clientHeight,
                right: scrollX + clientWidth,
              },
            ],
            [
              "ArrowUp",
              {
                top: scrollY,
                left: scrollX + 100 + 100,
                bottom: scrollY + 100 + 100,
                right: scrollX + clientWidth,
              },
            ],
            [
              "ArrowLeft",
              {
                top: scrollY,
                left: scrollX,
                bottom: scrollY + 100 + 100,
                right: scrollX + 100 + 100,
              },
            ],
          ];

          let screenshotsChild = content.windowGlobalChild.getActor(
            "ScreenshotsComponent"
          );
          screenshotsChild.overlay.topLeftMover.focus();

          // Move the region around in a clockwise direction
          // Check that original position is correct
          assertSelectionRegionDimensions(
            screenshotsChild.overlay.selectionRegion.dimensions,
            {
              top: 200,
              left: 200,
              bottom: 300,
              right: 300,
            }
          );

          for (let [
            key,
            expectedDimensions,
          ] of CONTROL_PLUS_KEY_TO_EXPECTED_POSITION_ARRAY) {
            EventUtils.synthesizeKey(key, { accelKey: true }, content);
            assertSelectionRegionDimensions(
              screenshotsChild.overlay.selectionRegion.dimensions,
              expectedDimensions
            );
          }
        }
      );
    }
  );
});
