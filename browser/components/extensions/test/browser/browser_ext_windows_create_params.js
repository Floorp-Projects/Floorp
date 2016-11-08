/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";


// Tests that incompatible parameters can't be used together.
add_task(function* testWindowCreateParams() {
  let extension = ExtensionTestUtils.loadExtension({
    async background() {
      function* getCalls() {
        for (let state of ["minimized", "maximized", "fullscreen"]) {
          for (let param of ["left", "top", "width", "height"]) {
            let expected = `"state": "${state}" may not be combined with "left", "top", "width", or "height"`;

            yield browser.windows.create({state, [param]: 100}).then(
              val => {
                browser.test.fail(`Expected error but got "${val}" instead`);
              },
              error => {
                browser.test.assertTrue(
                  error.message.includes(expected),
                  `Got expected error (got: '${error.message}', expected: '${expected}'`);
              });
          }
        }
      }

      try {
        await Promise.all(getCalls());

        browser.test.notifyPass("window-create-params");
      } catch (e) {
        browser.test.fail(`${e} :: ${e.stack}`);
        browser.test.notifyFail("window-create-params");
      }
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("window-create-params");
  yield extension.unload();
});
