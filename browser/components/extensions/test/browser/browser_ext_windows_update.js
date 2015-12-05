/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* () {
  function promiseWaitForFocus(window) {
    return new Promise(resolve => {
      waitForFocus(function() {
        ok(Services.focus.activeWindow === window, "correct window focused");
        resolve();
      }, window);
    });
  }

  let window1 = window;
  let window2 = yield BrowserTestUtils.openNewBrowserWindow();

  Services.focus.activeWindow = window2;
  yield promiseWaitForFocus(window2);

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["windows"],
    },

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

        browser.windows.update(wins[0].id, {focused: true}, function() {
          browser.test.sendMessage("check");
        });
      });
    },
  });

  yield Promise.all([extension.startup(), extension.awaitMessage("check")]);

  yield promiseWaitForFocus(window1);

  yield extension.unload();

  yield BrowserTestUtils.closeWindow(window2);
});
