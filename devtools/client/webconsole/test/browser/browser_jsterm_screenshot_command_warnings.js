/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that screenshot command leads to the proper warning and error messages in the
// console when necessary.

"use strict";

// We create a very big page here in order to make the :screenshot command fail on
// purpose.
const TEST_URI = `data:text/html;charset=utf8,
   <style>
     body { margin:0; }
     .big { width:20000px; height:20000px; }
     .small { width:5px; height:5px; }
   </style>
   <div class="big"></div>
   <div class="small"></div>`;

add_task(async function() {
  await addTab(TEST_URI);

  const hud = await openConsole();
  ok(hud, "web console opened");

  await testTruncationWarning(hud);
  await testDPRWarning(hud);
  await testErrorMessage(hud);
});

async function testTruncationWarning(hud) {
  info("Check that large screenshots get cut off if necessary");

  let onMessages = waitForMessages({
    hud,
    messages: [
      { text: "Screenshot copied to clipboard." },
      {
        text:
          "The image was cut off to 10000×10000 as the resulting image was too large",
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

  onMessages = waitForMessages({
    hud,
    messages: [{ text: "Screenshot copied to clipboard." }],
  });
  execute(hud, ":screenshot --clipboard --selector .small --dpr 1");
  await onMessages;

  ({ width, height } = await getImageSizeFromClipboard());
  is(width, 5, "The resulting image is 5px wide");
  is(height, 5, "The resulting image is 5px high");
}

async function testDPRWarning(hud) {
  info("Check that fullpage screenshots are taken at dpr 1");

  // This is only relevant on machines that actually have a dpr that's higher than 1. If
  // the current test machine already has a dpr of 1, then the command won't change it and
  // no warning will be displayed in the console.
  const machineDPR = await getMachineDPR();
  if (machineDPR <= 1) {
    info("This machine already has a dpr of 1, no need to test this");
    return;
  }

  const onMessages = waitForMessages({
    hud,
    messages: [
      { text: "Screenshot copied to clipboard." },
      {
        text:
          "The image was cut off to 10000×10000 as the resulting image was too large",
      },
      {
        text:
          "The device pixel ratio was reduced to 1 as the resulting image was too large",
      },
    ],
  });
  execute(hud, ":screenshot --clipboard --fullpage");
  await onMessages;

  ok(true, "Expected messages were displayed");
}

async function testErrorMessage(hud) {
  info("Check that when a screenshot fails, an error message is displayed");

  await executeAndWaitForMessage(
    hud,
    ":screenshot --clipboard --dpr 1000",
    "Error creating the image. The resulting image was probably too large."
  );
}

function getMachineDPR() {
  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => content.devicePixelRatio
  );
}
