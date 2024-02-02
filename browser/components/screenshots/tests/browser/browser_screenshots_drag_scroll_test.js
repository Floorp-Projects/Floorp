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
      info(JSON.stringify(contentInfo, null, 2));
      ok(contentInfo, "Got dimensions back from the content");

      helper.triggerUIFromToolbar();

      await helper.waitForOverlay();
      await helper.dragOverlay(10, 10, 500, 500);

      let { scrollWidth, scrollHeight } =
        await helper.getScreenshotsOverlayDimensions();

      is(
        scrollWidth,
        contentInfo.scrollWidth,
        "The overlay spans the entire width of the page"
      );

      is(
        scrollHeight,
        contentInfo.scrollHeight,
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

      await helper.assertStateChange("resizing");

      mouse.move(10, 10);

      // We moved the box to the edge of the screen so we need to wait until the box size is updated
      await helper.waitForSelectionRegionSizeChange(490);

      let dimensions = await helper.getSelectionRegionDimensions();

      is(dimensions.left, 0, "The box x1 position is now 0");
      is(dimensions.top, 0, "The box y1 position is now 0");
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
      await helper.waitForSelectionRegionSizeChange(255);

      dimensions = await helper.getSelectionRegionDimensions();

      is(dimensions.left, 10, "The box x1 position is now 10 again");
      is(dimensions.top, 10, "The box y1 position is now 10 again");
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

      await helper.scrollContentWindow(
        contentInfo.scrollWidth - contentInfo.clientWidth,
        contentInfo.scrollHeight - contentInfo.clientHeight
      );

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

      await helper.assertStateChange("resizing");

      mouse.move(endX, endY);

      // We moved the box to the edge of the screen so we need to wait until the box size is updated
      await helper.waitForSelectionRegionSizeChange(480);

      let dimensions = await helper.getSelectionRegionDimensions();

      is(dimensions.left, startX + 240, "The box x1 position is now 3748");
      is(dimensions.top, startY + 240, "The box y1 position is now 3756");
      is(dimensions.width, 260, "The box width is now 260");
      is(dimensions.height, 260, "The box height is now 260");

      mouse.move(
        startX + Math.floor((endX - startX) / 2),
        startY + Math.floor((endY - startY) / 2)
      );

      mouse.up(
        startX + Math.floor((endX - startX) / 2),
        startY + Math.floor((endY - startY) / 2)
      );

      // We moved the box off the edge of the screen so we need to wait until the box size is updated
      await helper.waitForSelectionRegionSizeChange(252);

      dimensions = await helper.getSelectionRegionDimensions();

      is(dimensions.left, startX, "The box x1 position is now 3508 again");
      is(dimensions.top, startY, "The box y1 position is now 3516 again");
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

      let dimensions = await helper.getSelectionRegionDimensions();

      is(dimensions.left, startX, "The box x1 position is 10");
      is(dimensions.top, startY, "The box y1 position is 10");
      is(dimensions.width, endX - startX, "The box width is now 90");
      is(dimensions.height, endY - startY, "The box height is now 90");

      // reset screenshots box
      mouse.click(scrollX + startX, scrollY + endY);
      await helper.assertStateChange("crosshairs");

      await helper.dragOverlay(
        scrollX + startX,
        scrollY + startY,
        scrollX + endX,
        scrollY + endY
      );

      await helper.scrollContentWindow(0, 0);

      dimensions = await helper.getSelectionRegionDimensions();

      is(dimensions.left, scrollX + startX, "The box x1 position is 1010");
      is(dimensions.top, scrollY + startY, "The box y1 position is 1010");
      is(dimensions.width, endX - startX, "The box width is now 90");
      is(dimensions.height, endY - startY, "The box height is now 90");

      // reset screenshots box
      mouse.click(10, 10);
      await helper.assertStateChange("crosshairs");

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

      await helper.assertStateChange("resizing");

      mouse.move(
        contentInfo.clientWidth * 2 - 30,
        contentInfo.clientHeight * 2 - 30
      );

      mouse.up(
        contentInfo.clientWidth * 2 - 30,
        contentInfo.clientHeight * 2 - 30
      );

      await helper.assertStateChange("selected");

      let { left, top, right, bottom, width, height } =
        await helper.getSelectionRegionDimensions();
      let { scrollWidth, scrollHeight } =
        await helper.getScreenshotsOverlayDimensions();

      is(left, startX, "The box left is 10");
      is(top, startY, "The box top is 10");
      is(
        right,
        contentInfo.clientWidth * 2 - 30,
        "The box right is 2 x clientWidth - 30"
      );
      is(
        bottom,
        contentInfo.clientHeight * 2 - 30,
        "The box right is 2 x clientHeight - 30"
      );
      is(
        width,
        contentInfo.clientWidth * 2 - 40,
        "The box right is 2 x clientWidth - 40"
      );
      is(
        height,
        contentInfo.clientHeight * 2 - 40,
        "The box right is 2 x clientHeight - 40"
      );
      is(
        scrollWidth,
        contentInfo.scrollWidth,
        "The overlay spans the entire width of the page"
      );
      is(
        scrollHeight,
        contentInfo.scrollHeight,
        "The overlay spans the entire height of the page"
      );
    }
  );
});

/**
 * test scroll if by edge
 */
add_task(async function test_scrollIfByEdge() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);

      let windowX = 1000;
      let windowY = 1000;

      await helper.scrollContentWindow(windowX, windowY);

      await TestUtils.waitForTick();

      helper.triggerUIFromToolbar();
      await helper.waitForOverlay();

      let { scrollX, scrollY } = await helper.getContentDimensions();

      is(scrollX, windowX, "Window x position is 1000");
      is(scrollY, windowY, "Window y position is 1000");

      let startX = 1100;
      let startY = 1100;
      let endX = 1010;
      let endY = 1010;

      // The window won't scroll if the state is draggingReady so we move to
      // get into the dragging state and then move again to scroll the window
      mouse.down(startX, startY);
      await helper.assertStateChange("draggingReady");
      mouse.move(1050, 1050);
      await helper.assertStateChange("dragging");
      mouse.move(endX, endY);
      mouse.up(endX, endY);
      await helper.assertStateChange("selected");

      windowX = 990;
      windowY = 990;
      await helper.waitForScrollTo(windowX, windowY);

      ({ scrollX, scrollY } = await helper.getContentDimensions());

      is(scrollX, windowX, "Window x position is 990");
      is(scrollY, windowY, "Window y position is 990");

      let contentInfo = await helper.getContentDimensions();

      endX = windowX + contentInfo.clientWidth - 10;
      endY = windowY + contentInfo.clientHeight - 10;

      info(
        `starting to drag overlay to ${endX}, ${endY} in test\nclientInfo: ${JSON.stringify(
          contentInfo
        )}\n`
      );
      mouse.down(startX, startY);
      await helper.assertStateChange("resizing");
      mouse.move(endX, endY);
      mouse.up(endX, endY);
      await helper.assertStateChange("selected");

      windowX = 1000;
      windowY = 1000;
      await helper.waitForScrollTo(windowX, windowY);

      ({ scrollX, scrollY } = await helper.getContentDimensions());

      is(scrollX, windowX, "Window x position is 1000");
      is(scrollY, windowY, "Window y position is 1000");
    }
  );
});

add_task(async function test_scrollIfByEdgeWithKeyboard() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);

      let windowX = 1000;
      let windowY = 1000;

      await helper.scrollContentWindow(windowX, windowY);

      helper.triggerUIFromToolbar();
      await helper.waitForOverlay();

      let { scrollX, scrollY, clientWidth, clientHeight } =
        await helper.getContentDimensions();

      is(scrollX, windowX, "Window x position is 1000");
      is(scrollY, windowY, "Window y position is 1000");

      await helper.dragOverlay(1020, 1020, 1120, 1120);

      await SpecialPowers.spawn(browser, [], async () => {
        let screenshotsChild = content.windowGlobalChild.getActor(
          "ScreenshotsComponent"
        );

        // Test moving each corner of the region
        screenshotsChild.overlay.highlightEl.focus();

        EventUtils.synthesizeKey("ArrowLeft", { shiftKey: true }, content);
        EventUtils.synthesizeKey("ArrowLeft", {}, content);

        EventUtils.synthesizeKey("ArrowUp", { shiftKey: true }, content);
        EventUtils.synthesizeKey("ArrowUp", {}, content);
      });

      windowX = 989;
      windowY = 989;
      await helper.waitForScrollTo(windowX, windowY);

      ({ scrollX, scrollY, clientWidth, clientHeight } =
        await helper.getContentDimensions());

      is(scrollX, windowX, "Window x position is 989");
      is(scrollY, windowY, "Window y position is 989");

      mouse.click(1200, 1200);
      await helper.assertStateChange("crosshairs");
      await helper.dragOverlay(
        scrollX + clientWidth - 100 - 20,
        scrollY + clientHeight - 100 - 20,
        scrollX + clientWidth - 20,
        scrollY + clientHeight - 20
      );

      await SpecialPowers.spawn(browser, [], async () => {
        let screenshotsChild = content.windowGlobalChild.getActor(
          "ScreenshotsComponent"
        );

        // Test moving each corner of the region
        screenshotsChild.overlay.highlightEl.focus();

        EventUtils.synthesizeKey("ArrowRight", { shiftKey: true }, content);
        EventUtils.synthesizeKey("ArrowRight", {}, content);

        EventUtils.synthesizeKey("ArrowDown", { shiftKey: true }, content);
        EventUtils.synthesizeKey("ArrowDown", {}, content);
      });

      windowX = 1000;
      windowY = 1000;
      await helper.waitForScrollTo(windowX, windowY);

      ({ scrollX, scrollY } = await helper.getContentDimensions());

      is(scrollX, windowX, "Window x position is 1000");
      is(scrollY, windowY, "Window y position is 1000");
    }
  );
});
