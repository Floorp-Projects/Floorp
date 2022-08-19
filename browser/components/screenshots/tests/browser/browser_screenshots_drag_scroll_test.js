/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that screenshots overlay covers the entire page
 */
add_task(async function test_overlayCoversEntirePage() {
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

      await helper.dragOverlay(10, 10, 500, 500);

      let dimensions = await helper.getSelectionLayerDimensions();
      info(JSON.stringify(dimensions));
      is(
        dimensions.scrollWidth,
        4000,
        "The overlay spans the entire width of the page"
      );

      is(
        dimensions.scrollHeight,
        4000,
        "The overlay spans the entire height of the page"
      );
    }
  );
});

/**
 * Test dragging screenshots box off top left of screen
 */
add_task(async function test_draggingBoxOffTopLeft() {
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

      let startX = 10;
      let startY = 10;
      let endX = 500;
      let endY = 500;
      await helper.dragOverlay(startX, startY, endX, endY);

      mouse.down(
        startX + Math.floor((endX - startX) / 2),
        startY + Math.floor((endY - startY) / 2)
      );

      await helper.waitForStateChange("resizing");
      let state = await helper.getOverlayState();
      is(state, "resizing", "The overlay is in the resizing state");

      mouse.move(10, 10);

      // We moved the box to the edge of the screen so we need to wait until the box size is updated
      await helper.waitForSelectionBoxSizeChange(490);

      let dimensions = await helper.getSelectionBoxDimensions();

      is(dimensions.x1, 0, "The box x1 position is now 0");
      is(dimensions.y1, 0, "The box y1 position is now 0");
      is(dimensions.width, 255, "The box width is now 255");
      is(dimensions.height, 255, "The box height is now 255");

      mouse.move(
        startX + Math.floor((endX - startX) / 2),
        startY + Math.floor((endY - startY) / 2)
      );

      mouse.up(
        startX + Math.floor((endX - startX) / 2),
        startY + Math.floor((endY - startY) / 2)
      );

      // We moved the box off the edge of the screen so we need to wait until the box size is updated
      await helper.waitForSelectionBoxSizeChange(255);

      dimensions = await helper.getSelectionBoxDimensions();

      is(dimensions.x1, 10, "The box x1 position is now 10 again");
      is(dimensions.y1, 10, "The box y1 position is now 10 again");
      is(dimensions.width, 490, "The box width is now 490 again");
      is(dimensions.height, 490, "The box height is now 490 again");
    }
  );
});

/**
 * Test dragging screenshots box off bottom right of screen
 */
add_task(async function test_draggingBoxOffBottomRight() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);

      let contentInfo = await helper.getContentDimensions();
      info(JSON.stringify(contentInfo));
      ok(contentInfo, "Got dimensions back from the content");

      await helper.scrollContentWindow(4000, 4000);

      helper.triggerUIFromToolbar();

      await helper.waitForOverlay();

      let startX = contentInfo.scrollWidth - 500;
      let startY = contentInfo.scrollHeight - 500;
      let endX = contentInfo.scrollWidth - 20;
      let endY = contentInfo.scrollHeight - 20;

      await helper.dragOverlay(startX, startY, endX, endY);

      // move box off the bottom right of the screen
      mouse.down(
        startX + Math.floor((endX - startX) / 2),
        startY + Math.floor((endY - startY) / 2)
      );
      mouse.move(
        startX + 50 + Math.floor((endX - startX) / 2),
        startY + 50 + Math.floor((endY - startY) / 2)
      );

      await helper.waitForStateChange("resizing");
      let state = await helper.getOverlayState();
      is(state, "resizing", "The overlay is in the resizing state");

      mouse.move(endX, endY);

      // We moved the box to the edge of the screen so we need to wait until the box size is updated
      await helper.waitForSelectionBoxSizeChange(480);

      let dimensions = await helper.getSelectionBoxDimensions();

      is(dimensions.x1, startX + 240, "The box x1 position is now 3748");
      is(dimensions.y1, startY + 240, "The box y1 position is now 3756");
      is(dimensions.width, 252, "The box width is now 252");
      is(dimensions.height, 244, "The box height is now 244");

      mouse.move(
        startX + Math.floor((endX - startX) / 2),
        startY + Math.floor((endY - startY) / 2)
      );

      mouse.up(
        startX + Math.floor((endX - startX) / 2),
        startY + Math.floor((endY - startY) / 2)
      );

      // We moved the box off the edge of the screen so we need to wait until the box size is updated
      await helper.waitForSelectionBoxSizeChange(252);

      dimensions = await helper.getSelectionBoxDimensions();

      is(dimensions.x1, startX, "The box x1 position is now 3508 again");
      is(dimensions.y1, startY, "The box y1 position is now 3516 again");
      is(dimensions.width, 480, "The box width is now 480 again");
      is(dimensions.height, 480, "The box height is now 480 again");
    }
  );
});

/**
 * test scrolling while screenshots is open
 */
add_task(async function test_scrollingScreenshotsOpen() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);

      let contentInfo = await helper.getContentDimensions();
      info(JSON.stringify(contentInfo));
      ok(contentInfo, "Got dimensions back from the content");

      helper.triggerUIFromToolbar();

      await helper.waitForOverlay();

      let startX = 10;
      let startY = 10;
      let endX = 100;
      let endY = 100;

      await helper.dragOverlay(startX, startY, endX, endY);

      let scrollX = 1000;
      let scrollY = 1000;

      await helper.scrollContentWindow(scrollX, scrollY);

      let dimensions = await helper.getSelectionBoxDimensions();

      is(dimensions.x1, startX, "The box x1 position is 10");
      is(dimensions.y1, startY, "The box y1 position is 10");
      is(dimensions.width, endX - startX, "The box width is now 90");
      is(dimensions.height, endY - startY, "The box height is now 90");

      // reset screenshots box
      mouse.click(scrollX + startX, scrollY + endY);
      await helper.waitForStateChange("crosshairs");

      await helper.dragOverlay(
        scrollX + startX,
        scrollY + startY,
        scrollX + endX,
        scrollY + endY
      );

      await helper.scrollContentWindow(0, 0);

      dimensions = await helper.getSelectionBoxDimensions();

      is(dimensions.x1, scrollX + startX, "The box x1 position is 1010");
      is(dimensions.y1, scrollY + startY, "The box y1 position is 1010");
      is(dimensions.width, endX - startX, "The box width is now 90");
      is(dimensions.height, endY - startY, "The box height is now 90");

      // reset screenshots box
      mouse.click(10, 10);
      await helper.waitForStateChange("crosshairs");

      await helper.dragOverlay(
        startX,
        startY,
        contentInfo.clientWidth - 10,
        contentInfo.clientHeight - 10
      );

      await helper.scrollContentWindow(
        contentInfo.clientWidth - 20,
        contentInfo.clientHeight - 20
      );

      mouse.down(contentInfo.clientWidth - 10, contentInfo.clientHeight - 10);

      await helper.waitForStateChange("resizing");

      mouse.move(
        contentInfo.clientWidth * 2 - 30,
        contentInfo.clientHeight * 2 - 30
      );

      mouse.up(
        contentInfo.clientWidth * 2 - 30,
        contentInfo.clientHeight * 2 - 30
      );

      await helper.waitForStateChange("selected");

      dimensions = await helper.getSelectionLayerDimensions();
      info(JSON.stringify(dimensions));
      is(dimensions.boxLeft, startX, "The box left is 10");
      is(dimensions.boxTop, startY, "The box top is 10");
      is(
        dimensions.boxRight,
        contentInfo.clientWidth * 2 - 30,
        "The box right is 2 x clientWidth - 30"
      );
      is(
        dimensions.boxBottom,
        contentInfo.clientHeight * 2 - 30,
        "The box right is 2 x clientHeight - 30"
      );
      is(
        dimensions.boxWidth,
        contentInfo.clientWidth * 2 - 40,
        "The box right is 2 x clientWidth - 40"
      );
      is(
        dimensions.boxHeight,
        contentInfo.clientHeight * 2 - 40,
        "The box right is 2 x clientHeight - 40"
      );
      is(
        dimensions.scrollWidth,
        4000,
        "The overlay spans the entire width of the page"
      );
      is(
        dimensions.scrollHeight,
        4000,
        "The overlay spans the entire height of the page"
      );
    }
  );
});
