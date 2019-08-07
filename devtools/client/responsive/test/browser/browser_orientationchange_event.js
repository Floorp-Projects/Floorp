/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the "orientationchange" event is fired when the "rotate button" is clicked.

const TEST_URL = "data:text/html;charset=utf-8,";

addRDMTask(TEST_URL, async function({ ui }) {
  info("Rotate viewport to trigger 'orientationchange' event.");
  await pushPref("devtools.responsive.viewport.angle", 0);
  rotateViewport(ui);

  await ContentTask.spawn(ui.getViewportBrowser(), {}, async function() {
    info("Check the original orientation values before the orientationchange");
    is(
      content.screen.orientation.type,
      "portrait-primary",
      "Primary orientation type is portrait-primary."
    );
    is(
      content.screen.orientation.angle,
      0,
      "Original angle is set at 0 degrees"
    );

    const orientationChange = new Promise(resolve => {
      content.window.addEventListener("orientationchange", () => {
        ok(true, "'orientationchange' event fired");
        is(
          content.screen.orientation.type,
          "landscape-primary",
          "Orientation state was updated to landscape-primary"
        );
        is(
          content.screen.orientation.angle,
          90,
          "Orientation angle was updated to 90 degrees."
        );
        resolve();
      });
    });

    await orientationChange;
  });

  info(
    "Check that the viewport orientation changed, but not the angle after a reload"
  );
  const browser = ui.getViewportBrowser();
  const reload = browser.reload();
  const onViewportOrientationChange = once(
    ui,
    "only-viewport-orientation-changed"
  );
  await reload;
  await onViewportOrientationChange;
  ok(true, "orientationchange event was not dispatched on reload.");

  await ContentTask.spawn(ui.getViewportBrowser(), {}, async function() {
    info("Check that we still have the previous orientation values.");
    is(content.screen.orientation.angle, 90, "Orientation angle is still 90");
    is(
      content.screen.orientation.type,
      "landscape-primary",
      "Orientation is still landscape-primary."
    );
  });
});
