/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This function drags to a 490x490 area and copies to the clipboard
 */
add_task(async function dragTest() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);
      let contentInfo = await helper.getContentDimensions();
      ok(contentInfo, "Got dimensions back from the content");
      let expected = Math.floor(
        490 * (await getContentDevicePixelRatio(browser))
      );

      helper.triggerUIFromToolbar();

      await helper.waitForOverlay();

      await helper.dragOverlay(10, 10, 500, 500);

      let clipboardChanged = helper.waitForRawClipboardChange(
        expected,
        expected
      );

      await helper.clickCopyButton();

      info("Waiting for clipboard change");
      let result = await clipboardChanged;

      info("result: " + JSON.stringify(result, null, 2));

      Assert.equal(
        result.width,
        expected,
        `The copied image from the overlay is ${expected}px in width`
      );
      Assert.equal(
        result.height,
        expected,
        `The copied image from the overlay is ${expected}px in height`
      );
    }
  );
});

/**
 * This function drags a 1.5 zoomed browser to a 490x490 area and copies to the clipboard
 */
add_task(async function dragTest1Point5Zoom() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      const zoom = 1.5;
      let helper = new ScreenshotsHelper(browser);
      helper.zoomBrowser(zoom);

      let contentInfo = await helper.getContentDimensions();
      ok(contentInfo, "Got dimensions back from the content");
      let expected = Math.floor(
        50 * (await getContentDevicePixelRatio(browser))
      );

      helper.triggerUIFromToolbar();

      await helper.waitForOverlay();

      await helper.dragOverlay(300, 100, 350, 150);

      let clipboardChanged = helper.waitForRawClipboardChange(
        expected,
        expected
      );

      await helper.clickCopyButton();

      info("Waiting for clipboard change");
      let result = await clipboardChanged;

      result.zoom = zoom;
      result.devicePixelRatio = window.devicePixelRatio;
      result.contentDevicePixelRatio = await getContentDevicePixelRatio(
        browser
      );

      info("result: " + JSON.stringify(result, null, 2));

      Assert.equal(
        result.width,
        expected,
        `The copied image from the overlay is ${expected}px in width`
      );
      Assert.equal(
        result.height,
        expected,
        `The copied image from the overlay is ${expected}px in height`
      );
    }
  );
});

/**
 * This function drags an area and clicks elsewhere
 * on the overlay to go back to the crosshairs state
 */
add_task(async function clickOverlayResetState() {
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

      await helper.dragOverlay(10, 10, 100, 100);

      // click outside overlay
      mouse.click(200, 200);

      await helper.waitForStateChange("crosshairs");
      let state = await helper.getOverlayState();
      Assert.equal(state, "crosshairs", "The state is back to crosshairs");
    }
  );
});

/**
 * This function drags an area and clicks the
 * cancel button to cancel the overlay
 */
add_task(async function overlayCancelButton() {
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

      await helper.dragOverlay(10, 10, 300, 300);

      await helper.clickCancelButton();

      await helper.waitForOverlayClosed();

      ok(!(await helper.isOverlayInitialized()), "Overlay is not initialized");
    }
  );
});

/**
 * This function drags a 490x490 area and moves it along the edges
 * and back to the center to confirm that the original size is preserved
 */
add_task(async function preserveBoxSizeWhenMovingOutOfWindowBounds() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: SHORT_TEST_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);

      let contentInfo = await helper.getContentDimensions();
      ok(contentInfo, "Got dimensions back from the content");
      let expected = Math.floor(
        490 * (await getContentDevicePixelRatio(browser))
      );

      helper.triggerUIFromToolbar();

      await helper.waitForOverlay();

      await helper.dragOverlay(10, 10, 500, 500);

      let startX = 10;
      let startY = 10;
      let endX = 500;
      let endY = 500;

      mouse.down(
        Math.floor((endX - startX) / 2),
        Math.floor((endY - startY) / 2)
      );

      await helper.waitForStateChange("resizing");
      let state = await helper.getOverlayState();
      Assert.equal(state, "resizing", "The overlay is in the resizing state");

      mouse.move(10, 10);

      mouse.move(contentInfo.clientWidth - 10, contentInfo.clientHeight - 10);

      mouse.up(
        Math.floor((endX - startX) / 2),
        Math.floor((endY - startY) / 2)
      );

      await helper.waitForStateChange("selected");
      state = await helper.getOverlayState();
      Assert.equal(state, "selected", "The overlay is in the selected state");

      let clipboardChanged = helper.waitForRawClipboardChange(
        expected,
        expected
      );

      await helper.clickCopyButton();

      info("Waiting for clipboard change");
      let result = await clipboardChanged;

      info("result: " + JSON.stringify(result, null, 2));

      Assert.equal(
        result.width,
        expected,
        `The copied image from the overlay is ${expected}px in width`
      );
      Assert.equal(
        result.height,
        expected,
        `The copied image from the overlay is ${expected}px in height`
      );
    }
  );
});

/**
 * This function drags a 490x490 area and resizes it to a 300x300 area
 *  with the 4 sides of the box
 */
add_task(async function resizeAllEdges() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);

      let contentInfo = await helper.getContentDimensions();
      ok(contentInfo, "Got dimensions back from the content");
      let expected = Math.floor(
        300 * (await getContentDevicePixelRatio(browser))
      );

      helper.triggerUIFromToolbar();

      await helper.waitForOverlay();

      await helper.dragOverlay(10, 10, 500, 500);

      let startX = 10;
      let startY = 10;
      let endX = 500;
      let endY = 500;

      let x = Math.floor((endX - startX) / 2);

      // drag top
      mouse.down(x, 10);

      await helper.waitForStateChange("resizing");
      let state = await helper.getOverlayState();
      Assert.equal(state, "resizing", "The overlay is in the resizing state");

      mouse.move(x, 100);
      mouse.up(x, 100);

      await helper.waitForStateChange("selected");
      state = await helper.getOverlayState();
      Assert.equal(state, "selected", "The overlay is in the selected state");

      // drag bottom
      mouse.down(x, 500);

      await helper.waitForStateChange("resizing");
      state = await helper.getOverlayState();
      Assert.equal(state, "resizing", "The overlay is in the resizing state");

      mouse.move(x, 400);
      mouse.up(x, 400);

      await helper.waitForStateChange("selected");
      state = await helper.getOverlayState();
      Assert.equal(state, "selected", "The overlay is in the selected state");

      // drag right
      let y = Math.floor((endY - startY) / 2);
      mouse.down(500, y);

      await helper.waitForStateChange("resizing");
      state = await helper.getOverlayState();
      Assert.equal(state, "resizing", "The overlay is in the resizing state");

      mouse.move(400, y);
      mouse.up(400, y);

      await helper.waitForStateChange("selected");
      state = await helper.getOverlayState();
      Assert.equal(state, "selected", "The overlay is in the selected state");

      // drag left
      mouse.down(10, y);

      await helper.waitForStateChange("resizing");
      state = await helper.getOverlayState();
      Assert.equal(state, "resizing", "The overlay is in the resizing state");

      mouse.move(100, y);
      mouse.up(100, y);

      await helper.waitForStateChange("selected");
      state = await helper.getOverlayState();
      Assert.equal(state, "selected", "The overlay is in the selected state");

      let clipboardChanged = helper.waitForRawClipboardChange(
        expected,
        expected
      );

      helper.endX = 400;
      helper.endY = 400;

      await helper.clickCopyButton();

      info("Waiting for clipboard change");
      let result = await clipboardChanged;

      info("result: " + JSON.stringify(result, null, 2));

      Assert.equal(
        result.width,
        expected,
        `The copied image from the overlay is ${expected}px in width`
      );
      Assert.equal(
        result.height,
        expected,
        `The copied image from the overlay is ${expected}px in height`
      );
    }
  );
});

/**
 * This function drags a 490x490 area and resizes it to a 300x300 area
 *  with the 4 corners of the box
 */
add_task(async function resizeAllCorners() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);

      let contentInfo = await helper.getContentDimensions();
      ok(contentInfo, "Got dimensions back from the content");
      let expected = Math.floor(
        300 * (await getContentDevicePixelRatio(browser))
      );

      helper.triggerUIFromToolbar();

      await helper.waitForOverlay();

      await helper.dragOverlay(10, 10, 500, 500);

      // drag topright
      mouse.down(500, 10);

      await helper.waitForStateChange("resizing");
      let state = await helper.getOverlayState();
      Assert.equal(state, "resizing", "The overlay is in the resizing state");

      mouse.move(450, 50);
      mouse.up(450, 50);

      await helper.waitForStateChange("selected");
      state = await helper.getOverlayState();
      Assert.equal(state, "selected", "The overlay is in the selected state");

      // drag bottomright
      mouse.down(450, 500);

      await helper.waitForStateChange("resizing");
      state = await helper.getOverlayState();
      Assert.equal(state, "resizing", "The overlay is in the resizing state");

      mouse.move(400, 450);
      mouse.up(400, 450);

      await helper.waitForStateChange("selected");
      state = await helper.getOverlayState();
      Assert.equal(state, "selected", "The overlay is in the selected state");

      // drag bottomleft
      mouse.down(10, 450);

      await helper.waitForStateChange("resizing");
      state = await helper.getOverlayState();
      Assert.equal(state, "resizing", "The overlay is in the resizing state");

      mouse.move(50, 400);
      mouse.up(50, 400);

      await helper.waitForStateChange("selected");
      state = await helper.getOverlayState();
      Assert.equal(state, "selected", "The overlay is in the selected state");

      // drag topleft
      mouse.down(50, 50);

      await helper.waitForStateChange("resizing");
      state = await helper.getOverlayState();
      Assert.equal(state, "resizing", "The overlay is in the resizing state");

      mouse.move(100, 100);
      mouse.up(100, 100);

      await helper.waitForStateChange("selected");
      state = await helper.getOverlayState();
      Assert.equal(state, "selected", "The overlay is in the selected state");

      let clipboardChanged = helper.waitForRawClipboardChange(
        expected,
        expected
      );

      helper.endX = 400;
      helper.endY = 400;

      await helper.clickCopyButton();

      info("Waiting for clipboard change");
      let result = await clipboardChanged;

      info("result: " + JSON.stringify(result, null, 2));

      Assert.equal(
        result.width,
        expected,
        `The copied image from the overlay is ${expected}px in width`
      );
      Assert.equal(
        result.height,
        expected,
        `The copied image from the overlay is ${expected}px in height`
      );
    }
  );
});
