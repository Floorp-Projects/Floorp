/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function() {
  function promiseWaitForFocus(window) {
    return new Promise(resolve => {
      waitForFocus(function() {
        ok(Services.focus.activeWindow === window, "correct window focused");
        resolve();
      }, window);
    });
  }

  let window1 = window;
  let window2 = await BrowserTestUtils.openNewBrowserWindow();

  window2.focus();
  await promiseWaitForFocus(window2);

  let extension = ExtensionTestUtils.loadExtension({
    background: function() {
      browser.windows.getAll(undefined, function(wins) {
        browser.test.assertEq(wins.length, 2, "should have two windows");

        // Sort the unfocused window to the lower index.
        wins.sort(function(win1, win2) {
          if (win1.focused === win2.focused) {
            return 0;
          }

          return win1.focused ? 1 : -1;
        });

        browser.windows.update(wins[0].id, { focused: true }, function() {
          browser.test.sendMessage("check");
        });
      });
    },
  });

  await Promise.all([extension.startup(), extension.awaitMessage("check")]);

  await promiseWaitForFocus(window1);

  await extension.unload();

  await BrowserTestUtils.closeWindow(window2);
});

add_task(async function testWindowUpdate() {
  let extension = ExtensionTestUtils.loadExtension({
    async background() {
      let _checkWindowPromise;
      browser.test.onMessage.addListener(msg => {
        if (msg == "checked-window") {
          _checkWindowPromise.resolve();
          _checkWindowPromise = null;
        }
      });

      let os;
      function checkWindow(expected) {
        return new Promise(resolve => {
          _checkWindowPromise = { resolve };
          browser.test.sendMessage("check-window", expected);
        });
      }

      let currentWindowId;
      async function updateWindow(windowId, params, expected) {
        let window = await browser.windows.update(windowId, params);

        browser.test.assertEq(
          currentWindowId,
          window.id,
          "Expected WINDOW_ID_CURRENT to refer to the same window"
        );
        for (let key of Object.keys(params)) {
          if (key == "state" && os == "mac" && params.state == "normal") {
            // OS-X doesn't have a hard distinction between "normal" and
            // "maximized" states.
            browser.test.assertTrue(
              window.state == "normal" || window.state == "maximized",
              `Expected window.state (currently ${window.state}) to be "normal" but will accept "maximized"`
            );
          } else {
            browser.test.assertEq(
              params[key],
              window[key],
              `Got expected value for window.${key}`
            );
          }
        }

        return checkWindow(expected);
      }

      try {
        let windowId = browser.windows.WINDOW_ID_CURRENT;

        ({ os } = await browser.runtime.getPlatformInfo());

        let window = await browser.windows.getCurrent();
        currentWindowId = window.id;

        await updateWindow(
          windowId,
          { state: "maximized" },
          { state: "STATE_MAXIMIZED" }
        );
        await updateWindow(
          windowId,
          { state: "minimized" },
          { state: "STATE_MINIMIZED" }
        );
        await updateWindow(
          windowId,
          { state: "normal" },
          { state: "STATE_NORMAL" }
        );
        await updateWindow(
          windowId,
          { state: "fullscreen" },
          { state: "STATE_FULLSCREEN" }
        );
        await updateWindow(
          windowId,
          { state: "normal" },
          { state: "STATE_NORMAL" }
        );

        browser.test.notifyPass("window-update");
      } catch (e) {
        browser.test.fail(`${e} :: ${e.stack}`);
        browser.test.notifyFail("window-update");
      }
    },
  });

  extension.onMessage("check-window", expected => {
    if (expected.state != null) {
      let { windowState } = window;
      if (window.fullScreen) {
        windowState = window.STATE_FULLSCREEN;
      }

      // Temporarily accepting STATE_MAXIMIZED on Linux because of bug 1307759.
      if (
        expected.state == "STATE_NORMAL" &&
        (AppConstants.platform == "macosx" || AppConstants.platform == "linux")
      ) {
        ok(
          windowState == window.STATE_NORMAL ||
            windowState == window.STATE_MAXIMIZED,
          `Expected windowState (currently ${windowState}) to be STATE_NORMAL but will accept STATE_MAXIMIZED`
        );
      } else {
        is(
          windowState,
          window[expected.state],
          `Expected window state to be ${expected.state}`
        );
      }
    }

    extension.sendMessage("checked-window");
  });

  await extension.startup();
  await extension.awaitFinish("window-update");
  await extension.unload();
});

add_task(async function() {
  let window2 = await BrowserTestUtils.openNewBrowserWindow();

  let extension = ExtensionTestUtils.loadExtension({
    background: function() {
      browser.windows.getAll(undefined, function(wins) {
        browser.test.assertEq(wins.length, 2, "should have two windows");

        let unfocused = wins.find(win => !win.focused);
        browser.windows.update(
          unfocused.id,
          { drawAttention: true },
          function() {
            browser.test.sendMessage("check");
          }
        );
      });
    },
  });

  await Promise.all([extension.startup(), extension.awaitMessage("check")]);

  await extension.unload();

  await BrowserTestUtils.closeWindow(window2);
});

// Tests that incompatible parameters can't be used together.
add_task(async function testWindowUpdateParams() {
  let extension = ExtensionTestUtils.loadExtension({
    async background() {
      try {
        for (let state of ["minimized", "maximized", "fullscreen"]) {
          for (let param of ["left", "top", "width", "height"]) {
            let expected = `"state": "${state}" may not be combined with "left", "top", "width", or "height"`;

            let windowId = browser.windows.WINDOW_ID_CURRENT;
            await browser.test.assertRejects(
              browser.windows.update(windowId, { state, [param]: 100 }),
              RegExp(expected),
              `Got expected error for create(${param}=100`
            );
          }
        }

        browser.test.notifyPass("window-update-params");
      } catch (e) {
        browser.test.fail(`${e} :: ${e.stack}`);
        browser.test.notifyFail("window-update-params");
      }
    },
  });

  await extension.startup();
  await extension.awaitFinish("window-update-params");
  await extension.unload();
});
