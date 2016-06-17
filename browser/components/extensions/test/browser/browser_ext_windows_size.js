/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* testWindowCreate() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
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
      function checkWindow(expected, retries = 5) {
        return getWindowSize().then(geom => {
          if (retries && KEYS.some(key => expected[key] != geom[key])) {
            browser.test.log(`Got mismatched size (${JSON.stringify(expected)} != ${JSON.stringify(geom)}). ` +
                             `Retrying after a short delay.`);

            return new Promise(resolve => {
              setTimeout(resolve, 200);
            }).then(() => {
              return checkWindow(expected, retries - 1);
            });
          }

          browser.test.log(`Check actual window size`);
          checkGeom(expected, geom);

          browser.test.log("Check API-reported window size");
          return browser.windows.get(windowId).then(geom => {
            checkGeom(expected, geom);
          });
        });
      }

      let geom = {left: 100, top: 100, width: 500, height: 300};

      return browser.windows.create(geom).then(window => {
        windowId = window.id;

        return checkWindow(geom);
      }).then(() => {
        let update = {left: 150, width: 600};
        Object.assign(geom, update);

        return browser.windows.update(windowId, update);
      }).then(() => {
        return checkWindow(geom);
      }).then(() => {
        let update = {top: 150, height: 400};
        Object.assign(geom, update);

        return browser.windows.update(windowId, update);
      }).then(() => {
        return checkWindow(geom);
      }).then(() => {
        geom = {left: 200, top: 200, width: 800, height: 600};

        return browser.windows.update(windowId, geom);
      }).then(() => {
        return checkWindow(geom);
      }).then(() => {
        return browser.runtime.getPlatformInfo();
      }).then((platformInfo) => {
        if (platformInfo.os != "linux") {
          geom = {left: -50, top: -50, width: 800, height: 600};

          return browser.windows.update(windowId, geom).then(() => {
            return checkWindow(geom);
          });
        }
      }).then(() => {
        return browser.windows.remove(windowId);
      }).then(() => {
        browser.test.notifyPass("window-size");
      }).catch(e => {
        browser.test.fail(`${e} :: ${e.stack}`);
        browser.test.notifyFail("window-size");
      });
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
