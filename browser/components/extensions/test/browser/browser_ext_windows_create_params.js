/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

AddonTestUtils.initMochitest(this);

// Tests that incompatible parameters can't be used together.
add_task(async function testWindowCreateParams() {
  let extension = ExtensionTestUtils.loadExtension({
    async background() {
      try {
        for (let state of ["minimized", "maximized", "fullscreen"]) {
          for (let param of ["left", "top", "width", "height"]) {
            let expected = `"state": "${state}" may not be combined with "left", "top", "width", or "height"`;

            await browser.test.assertRejects(
              browser.windows.create({ state, [param]: 100 }),
              RegExp(expected),
              `Got expected error from create(${param}=100)`
            );
          }
        }

        browser.test.notifyPass("window-create-params");
      } catch (e) {
        browser.test.fail(`${e} :: ${e.stack}`);
        browser.test.notifyFail("window-create-params");
      }
    },
  });

  await extension.startup();
  await extension.awaitFinish("window-create-params");
  await extension.unload();
});

// We do not support the focused param, however we do not want
// to fail despite an error when it is passed.  This provides
// better code level compatibility with chrome.
add_task(async function testWindowCreateFocused() {
  let extension = ExtensionTestUtils.loadExtension({
    async background() {
      async function doWaitForWindow(createOpts, resolve) {
        let created;
        browser.windows.onFocusChanged.addListener(async function listener(
          wid
        ) {
          if (wid == browser.windows.WINDOW_ID_NONE) {
            return;
          }
          let win = await created;
          if (win.id !== wid) {
            return;
          }
          browser.windows.onFocusChanged.removeListener(listener);
          // update the window object
          let window = await browser.windows.get(wid);
          resolve(window);
        });
        created = browser.windows.create(createOpts);
      }
      async function awaitNewFocusedWindow(createOpts) {
        return new Promise(resolve => {
          // eslint doesn't like an async promise function, so
          // we need to wrap it like this.
          doWaitForWindow(createOpts, resolve);
        });
      }
      try {
        let window = await awaitNewFocusedWindow({});
        browser.test.assertEq(
          window.focused,
          true,
          "window is focused without focused param"
        );
        browser.test.log("removeWindow");
        await browser.windows.remove(window.id);
        window = await awaitNewFocusedWindow({ focused: true });
        browser.test.assertEq(
          window.focused,
          true,
          "window is focused with focused: true"
        );
        browser.test.log("removeWindow");
        await browser.windows.remove(window.id);
        window = await awaitNewFocusedWindow({ focused: false });
        browser.test.assertEq(
          window.focused,
          true,
          "window is focused with focused: false"
        );
        browser.test.log("removeWindow");
        await browser.windows.remove(window.id);
        browser.test.notifyPass("window-create-params");
      } catch (e) {
        browser.test.fail(`${e} :: ${e.stack}`);
        browser.test.notifyFail("window-create-params");
      }
    },
  });

  ExtensionTestUtils.failOnSchemaWarnings(false);
  let { messages } = await AddonTestUtils.promiseConsoleOutput(async () => {
    await extension.startup();
    await extension.awaitFinish("window-create-params");
    await extension.unload();
  });

  AddonTestUtils.checkMessages(
    messages,
    {
      expected: [
        {
          message:
            /Warning processing focused: Opening inactive windows is not supported/,
        },
      ],
    },
    "Expected warning processing focused"
  );

  ExtensionTestUtils.failOnSchemaWarnings(true);
});

add_task(async function testPopupTypeWithDimension() {
  let extension = ExtensionTestUtils.loadExtension({
    async background() {
      await browser.windows.create({
        type: "popup",
        left: 123,
        top: 123,
        width: 151,
        height: 152,
      });
      await browser.windows.create({
        type: "popup",
        left: 123,
        width: 152,
        height: 153,
      });
      await browser.windows.create({
        type: "popup",
        top: 123,
        width: 153,
        height: 154,
      });
      await browser.windows.create({
        type: "popup",
        left: screen.availWidth * 100,
        top: screen.availHeight * 100,
        width: 154,
        height: 155,
      });
      await browser.windows.create({
        type: "popup",
        left: -screen.availWidth * 100,
        top: -screen.availHeight * 100,
        width: 155,
        height: 156,
      });
      browser.test.sendMessage("windows-created");
    },
  });

  const baseWindow = await BrowserTestUtils.openNewBrowserWindow();
  baseWindow.resizeTo(150, 150);
  baseWindow.moveTo(50, 50);

  let windows = [];
  let windowListener = (window, topic) => {
    if (topic == "domwindowopened") {
      windows.push(window);
    }
  };
  Services.ww.registerNotification(windowListener);

  await extension.startup();
  await extension.awaitMessage("windows-created");
  await extension.unload();

  const regularScreen = getScreenAt(0, 0, 150, 150);
  const roundedX = roundCssPixcel(123, regularScreen);
  const roundedY = roundCssPixcel(123, regularScreen);

  const availRectLarge = getCssAvailRect(
    getScreenAt(screen.width * 100, screen.height * 100, 150, 150)
  );
  const maxRight = availRectLarge.right;
  const maxBottom = availRectLarge.bottom;

  const availRectSmall = getCssAvailRect(
    getScreenAt(-screen.width * 100, -screen.height * 100, 150, 150150)
  );
  const minLeft = availRectSmall.left;
  const minTop = availRectSmall.top;

  const actualCoordinates = windows
    .slice(0, 3)
    .map(window => `${window.screenX},${window.screenY}`);
  const offsetFromBase = 10;
  const expectedCoordinates = [
    `${roundedX},${roundedY}`,
    // Missing top should be +10 from the last browser window.
    `${roundedX},${baseWindow.screenY + offsetFromBase}`,
    // Missing left should be +10 from the last browser window.
    `${baseWindow.screenX + offsetFromBase},${roundedY}`,
  ];
  is(
    actualCoordinates.join(" / "),
    expectedCoordinates.join(" / "),
    "expected popup type windows are opened at given coordinates"
  );

  const actualSizes = windows
    .slice(0, 3)
    .map(window => `${window.outerWidth}x${window.outerHeight}`);
  const expectedSizes = [`151x152`, `152x153`, `153x154`];
  is(
    actualSizes.join(" / "),
    expectedSizes.join(" / "),
    "expected popup type windows are opened with given size"
  );

  const actualRect = {
    top: windows[4].screenY,
    bottom: windows[3].screenY + windows[3].outerHeight,
    left: windows[4].screenX,
    right: windows[3].screenX + windows[3].outerWidth,
  };
  const maxRect = {
    top: minTop,
    bottom: maxBottom,
    left: minLeft,
    right: maxRight,
  };
  isRectContained(actualRect, maxRect);

  for (const window of windows) {
    window.close();
  }

  Services.ww.unregisterNotification(windowListener);
  windows = null;
  await BrowserTestUtils.closeWindow(baseWindow);
});
