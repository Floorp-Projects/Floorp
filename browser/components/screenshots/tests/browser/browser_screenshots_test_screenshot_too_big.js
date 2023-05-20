/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const SCREENSHOTS_EVENTS = [
  { category: "screenshots", method: "failed", object: "screenshot_too_large" },
  { category: "screenshots", method: "failed", object: "screenshot_too_large" },
  { category: "screenshots", method: "failed", object: "screenshot_too_large" },
  { category: "screenshots", method: "failed", object: "screenshot_too_large" },
];

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);
ChromeUtils.defineESModuleGetters(this, {
  ScreenshotsUtils: "resource:///modules/ScreenshotsUtils.sys.mjs",
});

add_task(async function test_screenshot_too_large_cropped() {
  await clearAllTelemetryEvents();
  const screenshotsLocalization = new Localization(
    ["browser/screenshots.ftl"],
    true
  );

  let [errorTitle, errorMessage] = screenshotsLocalization.formatMessagesSync([
    { id: "screenshots-too-large-error-title" },
    { id: "screenshots-too-large-error-details" },
  ]);
  let showAlertMessageStub = sinon
    .stub(ScreenshotsUtils, "showAlertMessage")
    .callsFake(function (title, message) {
      is(title, errorTitle.value, "Got error title");
      is(message, errorMessage.value, "Got error message");
    });

  let rect = { x: 0, y: 0, width: 40000, height: 40000, devicePixelRatio: 1 };

  ScreenshotsUtils.cropScreenshotRectIfNeeded(rect);

  is(rect.width, MAX_CAPTURE_DIMENSION, "The width is 32767");
  is(
    rect.height,
    Math.floor(MAX_CAPTURE_AREA / MAX_CAPTURE_DIMENSION),
    "The height is 124925329 / 32767"
  );

  rect.width = 40000;
  rect.hegith = 1;

  ScreenshotsUtils.cropScreenshotRectIfNeeded(rect);

  is(
    rect.width,
    MAX_CAPTURE_DIMENSION,
    "The width was cropped to the max capture dimension (32767)."
  );

  rect.width = 1;
  rect.height = 40000;

  ScreenshotsUtils.cropScreenshotRectIfNeeded(rect);

  is(
    rect.height,
    MAX_CAPTURE_DIMENSION,
    "The height was cropped to the max capture dimension (32767)."
  );

  rect.width = 12345;
  rect.height = 12345;

  ScreenshotsUtils.cropScreenshotRectIfNeeded(rect);

  is(rect.width, 12345, "The width was not cropped");
  is(
    rect.height,
    Math.floor(MAX_CAPTURE_AREA / 12345),
    "The height was cropped to 10119"
  );

  showAlertMessageStub.restore();

  await assertScreenshotsEvents(SCREENSHOTS_EVENTS);
});
