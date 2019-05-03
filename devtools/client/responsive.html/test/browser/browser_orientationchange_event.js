/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the "orientationchange" event is fired when the "rotate button" is clicked.

const TEST_URL = "data:text/html;charset=utf-8,";

addRDMTask(TEST_URL, async function({ ui }) {
  info("Rotate viewport to trigger 'orientationchange' event.");
  rotateViewport(ui);

  await ContentTask.spawn(ui.getViewportBrowser(), {},
    async function() {
      const orientationChange = new Promise(resolve => {
        content.window.addEventListener("orientationchange", () => {
          ok(true, "'orientationchange' event fired");
          resolve();
        });
      });

      await orientationChange;
    }
  );
});
