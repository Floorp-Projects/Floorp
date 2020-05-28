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
