/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that screenshot command leads to the proper warning and error messages in the
// console when necessary.

"use strict";

// The test times out on slow platforms (e.g. linux ccov)
requestLongerTimeout(2);

// We create a very big page here in order to make the :screenshot command fail on
// purpose.
const TEST_URI = `data:text/html;charset=utf8,<!DOCTYPE html>
   <style>
     body { margin:0; }
     .big { width:20000px; height:20000px; }
     .small { width:5px; height:5px; }
   </style>
   <div class="big"></div>
   <div class="small"></div>`;

add_task(async function () {
  await addTab(TEST_URI);

  const hud = await openConsole();
  ok(hud, "web console opened");

  await testTruncationWarning(hud);
  await testDPRWarning(hud);
});

async function testTruncationWarning(hud) {
  info("Check that large screenshots get cut off if necessary");

  let onMessages = waitForMessagesByType({
    hud,
    messages: [
      {
        text: "Screenshot copied to clipboard.",
        typeSelector: ".console-api",
      },
      {
        text: "The image was cut off to 10000×10000 as the resulting image was too large",
        typeSelector: ".console-api",
      },
    ],
  });
  // Note, we put the screenshot in the clipboard so we can easily measure the resulting
  // image. We also pass --dpr 1 so we don't need to worry about different machines having
  // different screen resolutions.
  execute(hud, ":screenshot --clipboard --selector .big --dpr 1");
  await onMessages;

  let { width, height } = await getImageSizeFromClipboard();
  is(width, 10000, "The resulting image is 10000px wide");
  is(height, 10000, "The resulting image is 10000px high");

  onMessages = waitForMessageByType(
    hud,
    "Screenshot copied to clipboard.",
    ".console-api"
  );
  execute(hud, ":screenshot --clipboard --selector .small --dpr 1");
  await onMessages;

  ({ width, height } = await getImageSizeFromClipboard());
  is(width, 5, "The resulting image is 5px wide");
  is(height, 5, "The resulting image is 5px high");
}

async function testDPRWarning(hud) {
  info("Check that DPR is reduced to 1 after failure");

  const onMessages = waitForMessagesByType({
    hud,
    messages: [
      {
        text: "Screenshot copied to clipboard.",
        typeSelector: ".console-api",
      },
      {
        text: "The image was cut off to 10000×10000 as the resulting image was too large",
        typeSelector: ".console-api",
      },
      {
        text: "The device pixel ratio was reduced to 1 as the resulting image was too large",
        typeSelector: ".console-api",
      },
    ],
  });
  execute(hud, ":screenshot --clipboard --fullpage --dpr 1000");
  await onMessages;

  const { width, height } = await getImageSizeFromClipboard();
  is(width, 10000, "The resulting image is 10000px wide");
  is(height, 10000, "The resulting image is 10000px high");
}
