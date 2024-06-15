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

async function doKeyPress(key, options, window) {
  let { repeat } = options;
  if (repeat) {
    delete options.repeat;
    for (let i = 0; i < repeat; i++) {
      let mouseEvent = BrowserTestUtils.waitForEvent(window, "mousemove");
      EventUtils.synthesizeKey(key, options, window);
      await mouseEvent;
    }
  } else {
    let mouseEvent = BrowserTestUtils.waitForEvent(window, "mousemove");
    EventUtils.synthesizeKey(key, options, window);
    await mouseEvent;
  }
}

function assertSelectionRegionDimensions(actualDimensions, expectedDimensions) {
  is(
    Math.round(actualDimensions.top),
    expectedDimensions.top,
    "Top dimension is correct"
  );
  is(
    Math.round(actualDimensions.left),
    expectedDimensions.left,
    "Left dimension is correct"
  );
  is(
    Math.round(actualDimensions.bottom),
    expectedDimensions.bottom,
    "Bottom dimension is correct"
  );
  is(
    Math.round(actualDimensions.right),
    expectedDimensions.right,
    "Right dimension is correct"
  );
}

add_setup(async function () {
  let tmpDir = PathUtils.join(
    PathUtils.tempDir,
    "testsavedir" + Math.floor(Math.random() * 2 ** 32)
  );
  // Create this dir if it doesn't exist (ignores existing dirs)
  await IOUtils.makeDirectory(tmpDir);
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.start_downloads_in_tmp_dir", true],
      ["browser.helperApps.deleteTempFileOnExit", true],
      ["browser.download.folderList", 2],
      ["browser.download.dir", tmpDir],
    ],
  });

  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref("browser.download.dir");
    Services.prefs.clearUserPref("browser.download.folderList");
  });
});

add_task(async function test_elementSelectedOnEnter() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);
      let { clientWidth, clientHeight, scrollbarWidth, scrollbarHeight } =
        await helper.getContentDimensions();
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

      // Focus should move to the browser
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

      let mouseEvent = BrowserTestUtils.waitForEvent(window, "mousemove");
      const windowMiddleX =
        (window.innerWidth / 2 + window.mozInnerScreenX) *
        window.devicePixelRatio;
      const windowMiddleY =
        (browser.clientHeight / 2) * window.devicePixelRatio;
      const contentTop =
        (window.mozInnerScreenY + (window.innerHeight - browser.clientHeight)) *
        window.devicePixelRatio;

      window.windowUtils.sendNativeMouseEvent(
        windowMiddleX,
        windowMiddleY + contentTop,
        window.windowUtils.NATIVE_MOUSE_MESSAGE_MOVE,
        0,
        0,
        window.document.documentElement
      );
      await mouseEvent;

      await helper.waitForContentMousePosition(
        (clientWidth + scrollbarWidth) / 2,
        (clientHeight + scrollbarHeight) / 2
      );

      let x = {};
      let y = {};
      window.windowUtils.getLastOverWindowPointerLocationInCSSPixels(x, y);
      let currentCursorX = x.value;
      let currentCursorY = y.value;

      let rect = await helper.getTestPageElementRect();

      info(JSON.stringify({ currentCursorX, currentCursorY }));
      info(JSON.stringify(rect));

      let repeatShiftLeft = Math.round((currentCursorX - rect.right) / 10);
      await doKeyPress(
        "ArrowLeft",
        { shiftKey: true, repeat: repeatShiftLeft },
        window
      );

      let repeatLeft = (currentCursorX - rect.right) % 10;
      await doKeyPress("ArrowLeft", { repeat: repeatLeft }, window);

      let repeatShiftRight = Math.round((currentCursorY - rect.bottom) / 10);
      await doKeyPress(
        "ArrowUp",
        { shiftKey: true, repeat: repeatShiftRight },
        window
      );

      let repeatRight = (currentCursorY - rect.bottom) % 10;
      await doKeyPress("ArrowUp", { repeat: repeatRight }, window);

      await helper.waitForHoverElementRect(rect.width, rect.height);

      EventUtils.synthesizeKey("Enter", {});
      await helper.waitForStateChange(["selected"]);

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

      await doKeyPress("ArrowRight", {}, window);

      let mouseEvent = BrowserTestUtils.waitForEvent(window, "mousemove");
      const window100X =
        (100 + window.mozInnerScreenX) * window.devicePixelRatio;
      const contentTop =
        (window.mozInnerScreenY + (window.innerHeight - browser.clientHeight)) *
        window.devicePixelRatio;
      const window100Y = 100 * window.devicePixelRatio + contentTop;

      info(JSON.stringify({ window100X, window100Y }));

      window.windowUtils.sendNativeMouseEvent(
        Math.floor(window100X),
        Math.floor(window100Y),
        window.windowUtils.NATIVE_MOUSE_MESSAGE_MOVE,
        0,
        0,
        window.document.documentElement
      );
      await mouseEvent;

      await helper.waitForContentMousePosition(100, 100);

      EventUtils.synthesizeKey(" ");
      await helper.waitForStateChange(["dragging"]);

      let lastX = 100;
      let lastY = 100;
      for (let [key, expectedDimensions] of KEY_TO_EXPECTED_POSITION_ARRAY) {
        await doKeyPress(key, { repeat: 10 }, window);
        if (key.includes("Left")) {
          lastX = expectedDimensions.left;
        } else if (key.includes("Right")) {
          lastX = expectedDimensions.right;
        } else if (key.includes("Down")) {
          lastY = expectedDimensions.bottom;
        } else if (key.includes("Up")) {
          lastY = expectedDimensions.top;
        }
        await TestUtils.waitForTick();
        await helper.waitForContentMousePosition(lastX, lastY);
        let actualDimensions = await helper.getSelectionRegionDimensions();
        info(`Key: ${key}`);
        info(`Actual dimensions: ${JSON.stringify(actualDimensions, null, 2)}`);
        info(
          `Expected dimensions: ${JSON.stringify(expectedDimensions, null, 2)}`
        );
        assertSelectionRegionDimensions(actualDimensions, expectedDimensions);
      }

      await doKeyPress("ArrowRight", { repeat: 10 }, window);
      await doKeyPress("ArrowDown", { repeat: 10 }, window);
      await helper.waitForContentMousePosition(110, 110);

      EventUtils.synthesizeKey(" ");
      await helper.waitForStateChange(["selected"]);

      let region = await helper.getSelectionRegionDimensions();

      is(Math.round(region.left), 100, "The selected region left is 100");
      is(Math.round(region.right), 110, "The selected region right is 110");
      is(Math.round(region.top), 100, "The selected region top is 100");
      is(Math.round(region.bottom), 110, "The selected region bottom is 110");

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

      await doKeyPress("ArrowRight", {}, window);

      let mouseEvent = BrowserTestUtils.waitForEvent(window, "mousemove");
      const window100X =
        (100 + window.mozInnerScreenX) * window.devicePixelRatio;
      const contentTop =
        (window.mozInnerScreenY + (window.innerHeight - browser.clientHeight)) *
        window.devicePixelRatio;
      const window100Y = 100 * window.devicePixelRatio + contentTop;

      info(JSON.stringify({ window100X, window100Y }));

      window.windowUtils.sendNativeMouseEvent(
        window100X,
        window100Y,
        window.windowUtils.NATIVE_MOUSE_MESSAGE_MOVE,
        0,
        0,
        window.document.documentElement
      );
      await mouseEvent;

      await helper.waitForContentMousePosition(100, 100);

      EventUtils.synthesizeKey(" ");
      await helper.waitForStateChange(["dragging"]);

      let lastX = 100;
      let lastY = 100;
      for (let [
        key,
        expectedDimensions,
      ] of SHIFT_PLUS_KEY_TO_EXPECTED_POSITION_ARRAY) {
        await doKeyPress(key, { shiftKey: true, repeat: 10 }, window);
        if (key.includes("Left")) {
          lastX = expectedDimensions.left;
        } else if (key.includes("Right")) {
          lastX = expectedDimensions.right;
        } else if (key.includes("Down")) {
          lastY = expectedDimensions.bottom;
        } else if (key.includes("Up")) {
          lastY = expectedDimensions.top;
        }
        await TestUtils.waitForTick();
        await helper.waitForContentMousePosition(lastX, lastY);
        let actualDimensions = await helper.getSelectionRegionDimensions();
        info(`Key: ${key}`);
        info(`Actual dimensions: ${JSON.stringify(actualDimensions, null, 2)}`);
        info(
          `Expected dimensions: ${JSON.stringify(expectedDimensions, null, 2)}`
        );
        assertSelectionRegionDimensions(actualDimensions, expectedDimensions);
      }

      await doKeyPress("ArrowRight", { shiftKey: true, repeat: 10 }, window);
      await doKeyPress("ArrowDown", { shiftKey: true, repeat: 10 }, window);
      await helper.waitForContentMousePosition(200, 200);

      EventUtils.synthesizeKey(" ");
      await helper.waitForStateChange(["selected"]);

      let region = await helper.getSelectionRegionDimensions();

      is(Math.round(region.left), 100, "The selected region left is 100");
      is(Math.round(region.right), 200, "The selected region right is 200");
      is(Math.round(region.top), 100, "The selected region top is 100");
      is(Math.round(region.bottom), 200, "The selected region bottom is 200");

      helper.triggerUIFromToolbar();
      await helper.waitForOverlayClosed();
    }
  );
});

add_task(async function test_clickingButtonsWithKeyDown() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      let publicDownloads = await Downloads.getList(Downloads.PUBLIC);
      // First ensure we catch the download finishing.
      let downloadFinishedPromise = new Promise(resolve => {
        publicDownloads.addView({
          onDownloadChanged(download) {
            info("Download changed!");
            if (download.succeeded || download.error) {
              info("Download succeeded or errored");
              publicDownloads.removeView(this);
              resolve(download);
            }
          },
        });
      });

      let helper = new ScreenshotsHelper(browser);
      let expected = Math.floor(
        300 * (await getContentDevicePixelRatio(browser))
      );

      for (let aKey of [" ", "Enter"]) {
        info(`Running tests with ${aKey} key`);

        info("Testing keydown on crosshairs cancel button");
        helper.triggerUIFromToolbar();
        await helper.waitForOverlay();

        await helper.hoverTestPageElement();

        await SpecialPowers.spawn(browser, [], () => {
          let screenshotsChild = content.windowGlobalChild.getActor(
            "ScreenshotsComponent"
          );

          screenshotsChild.overlay.previewCancelButton.focus({
            focusVisible: true,
          });
        });

        // Keydown on crosshairs cancel button
        key.down(aKey);

        await helper.waitForOverlayClosed();

        info("Testing keydown on cancel button");
        helper.triggerUIFromToolbar();
        await helper.waitForOverlay();

        await helper.dragOverlay(100, 100, 400, 400);

        await SpecialPowers.spawn(browser, [], () => {
          let screenshotsChild = content.windowGlobalChild.getActor(
            "ScreenshotsComponent"
          );

          screenshotsChild.overlay.cancelButton.focus({
            focusVisible: true,
          });
        });

        // Keydown on cancel button
        key.down(aKey);

        await helper.waitForStateChange("crosshairs");

        info("Testing keydown on copy button");

        await helper.dragOverlay(100, 100, 400, 400);

        await SpecialPowers.spawn(browser, [], () => {
          let screenshotsChild = content.windowGlobalChild.getActor(
            "ScreenshotsComponent"
          );

          screenshotsChild.overlay.copyButton.focus({
            focusVisible: true,
          });
        });

        let clipboardChanged = helper.waitForRawClipboardChange(
          expected,
          expected
        );

        // Keydown on copy button
        key.down(aKey);

        await clipboardChanged;

        await helper.waitForOverlayClosed();

        info("Testing keydown on download button");

        helper.triggerUIFromToolbar();
        await helper.waitForOverlay();

        await helper.dragOverlay(100, 100, 400, 400);

        await SpecialPowers.spawn(browser, [], () => {
          let screenshotsChild = content.windowGlobalChild.getActor(
            "ScreenshotsComponent"
          );

          screenshotsChild.overlay.downloadButton.focus({
            focusVisible: true,
          });
        });

        let screenshotExit = TestUtils.topicObserved("screenshots-exit");

        // Keydown on download button
        key.down(aKey);

        await helper.waitForOverlayClosed();

        info("wait for download to finish");
        let download = await downloadFinishedPromise;

        ok(download.succeeded, "Download should succeed");

        await publicDownloads.removeFinished();
        await screenshotExit;
      }
    }
  );
});
