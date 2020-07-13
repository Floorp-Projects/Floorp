/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

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

add_task(async function testPopupTypeWithCoordinates() {
  let extension = ExtensionTestUtils.loadExtension({
    async background() {
      await browser.windows.create({
        type: "popup",
        left: 123,
        top: 123,
        width: 150,
        height: 150,
      });
      await browser.windows.create({
        type: "popup",
        left: 123,
        width: 150,
        height: 150,
      });
      await browser.windows.create({
        type: "popup",
        top: 123,
        width: 150,
        height: 150,
      });
      await browser.windows.create({
        type: "popup",
        left: screen.availWidth * 100,
        top: screen.availHeight * 100,
        width: 150,
        height: 150,
      });
      await browser.windows.create({
        type: "popup",
        left: -screen.availWidth * 100,
        top: -screen.availHeight * 100,
        width: 150,
        height: 150,
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
    getScreenAt(screen.availWidth * 100, screen.availHeight * 100, 150, 150)
  );
  const maxLeft = availRectLarge.right - 150;
  const maxTop = availRectLarge.bottom - 150;

  const availRectSmall = getCssAvailRect(
    getScreenAt(-screen.availWidth * 100, -screen.availHeight * 100, 150, 150)
  );
  const minLeft = availRectSmall.left;
  const minTop = availRectSmall.top;

  const actualCoordinates = windows.map(
    window => `${window.screenX},${window.screenY}`
  );

  const offsetFromBase = 10;
  const expectedCoordinates = [
    `${roundedX},${roundedY}`,
    // Missing top should be +10 from the last browser window.
    `${roundedX},${baseWindow.screenY + offsetFromBase}`,
    // Missing left should be +10 from the last browser window.
    `${baseWindow.screenX + offsetFromBase},${roundedY}`,
    // Coordinates outside the visible area should be rounded.
    `${maxLeft},${maxTop}`,
    `${minLeft},${minTop}`,
  ];
  is(
    actualCoordinates.join(" / "),
    expectedCoordinates.join(" / "),
    "expected popup type windows are opened at given coordinates"
  );

  for (const window of windows) {
    window.close();
  }

  Services.ww.unregisterNotification(windowListener);
  windows = null;
  await BrowserTestUtils.closeWindow(baseWindow);
});
