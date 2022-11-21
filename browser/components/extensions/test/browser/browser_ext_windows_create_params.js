/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
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
          message: /Warning processing focused: Opening inactive windows is not supported/,
        },
      ],
    },
    "Expected warning processing focused"
  );

  ExtensionTestUtils.failOnSchemaWarnings(true);
});
