/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the "orientationchange" event is fired when the "rotate button" is clicked.

const TEST_URL = "data:text/html;charset=utf-8,";
const testDevice = {
  name: "Fake Phone RDM Test",
  width: 320,
  height: 570,
  pixelRatio: 5.5,
  userAgent: "Mozilla/5.0 (Mobile; rv:39.0) Gecko/39.0 Firefox/39.0",
  touch: true,
  firefoxOS: true,
  os: "custom",
  featured: true,
};

// Add the new device to the list
addDeviceForTest(testDevice);

addRDMTask(TEST_URL, async function({ ui }) {
  info("Rotate viewport to trigger 'orientationchange' event.");
  await pushPref("devtools.responsive.viewport.angle", 0);
  rotateViewport(ui);

  await SpecialPowers.spawn(ui.getViewportBrowser(), [], async function() {
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

  info("Check that the viewport orientation values persist after reload");
  const browser = ui.getViewportBrowser();
  const reload = waitForViewportLoad(ui);
  browser.reload();
  await reload;

  await SpecialPowers.spawn(ui.getViewportBrowser(), [], async function() {
    info("Check that we still have the previous orientation values.");
    is(content.screen.orientation.angle, 90, "Orientation angle is still 90");
    is(
      content.screen.orientation.type,
      "landscape-primary",
      "Orientation is still landscape-primary."
    );
  });

  info(
    "Check the orientationchange event is not dispatched when changing devices."
  );
  const onViewportOrientationChange = once(
    ui,
    "only-viewport-orientation-changed"
  );
  await selectDevice(ui, "Fake Phone RDM Test");
  await onViewportOrientationChange;

  await ContentTask.spawn(ui.getViewportBrowser(), {}, async function() {
    info("Check the new orientation values after selecting device.");
    is(content.screen.orientation.angle, 0, "Orientation angle is 0");
    is(
      content.screen.orientation.type,
      "portrait-primary",
      "New orientation is portrait-primary."
    );
  });
});
