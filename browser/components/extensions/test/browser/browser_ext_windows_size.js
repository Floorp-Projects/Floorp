/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* testWindowCreate() {
  let extension = ExtensionTestUtils.loadExtension({
    async background() {
      let _checkWindowPromise;
      browser.test.onMessage.addListener((msg, arg) => {
        if (msg == "checked-window") {
          _checkWindowPromise.resolve(arg);
          _checkWindowPromise = null;
        }
      });

      let getWindowSize = () => {
        return new Promise(resolve => {
          _checkWindowPromise = {resolve};
          browser.test.sendMessage("check-window");
        });
      };

      const KEYS = ["left", "top", "width", "height"];
      function checkGeom(expected, actual) {
        for (let key of KEYS) {
          browser.test.assertEq(expected[key], actual[key], `Expected '${key}' value`);
        }
      }

      let windowId;
      async function checkWindow(expected, retries = 5) {
        let geom = await getWindowSize();

        if (retries && KEYS.some(key => expected[key] != geom[key])) {
          browser.test.log(`Got mismatched size (${JSON.stringify(expected)} != ${JSON.stringify(geom)}). ` +
                           `Retrying after a short delay.`);

          await new Promise(resolve => setTimeout(resolve, 200));

          return checkWindow(expected, retries - 1);
        }

        browser.test.log(`Check actual window size`);
        checkGeom(expected, geom);

        browser.test.log("Check API-reported window size");

        geom = await browser.windows.get(windowId);

        checkGeom(expected, geom);
      }

      try {
        let geom = {left: 100, top: 100, width: 500, height: 300};

        let window = await browser.windows.create(geom);
        windowId = window.id;

        await checkWindow(geom);

        let update = {left: 150, width: 600};
        Object.assign(geom, update);
        await browser.windows.update(windowId, update);
        await checkWindow(geom);

        update = {top: 150, height: 400};
        Object.assign(geom, update);
        await browser.windows.update(windowId, update);
        await checkWindow(geom);

        geom = {left: 200, top: 200, width: 800, height: 600};
        await browser.windows.update(windowId, geom);
        await checkWindow(geom);

        let platformInfo = await browser.runtime.getPlatformInfo();
        if (platformInfo.os != "linux") {
          geom = {left: -50, top: -50, width: 800, height: 600};
          await browser.windows.update(windowId, geom);
          await checkWindow(geom);
        }

        await browser.windows.remove(windowId);
        browser.test.notifyPass("window-size");
      } catch (e) {
        browser.test.fail(`${e} :: ${e.stack}`);
        browser.test.notifyFail("window-size");
      }
    },
  });

  let latestWindow;
  let windowListener = (window, topic) => {
    if (topic == "domwindowopened") {
      latestWindow = window;
    }
  };
  Services.ww.registerNotification(windowListener);

  extension.onMessage("check-window", () => {
    extension.sendMessage("checked-window", {
      top: latestWindow.screenY,
      left: latestWindow.screenX,
      width: latestWindow.outerWidth,
      height: latestWindow.outerHeight,
    });
  });

  yield extension.startup();
  yield extension.awaitFinish("window-size");
  yield extension.unload();

  Services.ww.unregisterNotification(windowListener);
  latestWindow = null;
});
