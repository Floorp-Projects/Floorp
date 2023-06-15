/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that :screenshot command works properly in the Browser Console

"use strict";

const COLOR_DIV_1 = "rgb(255, 0, 0)";
const COLOR_DIV_2 = "rgb(0, 200, 0)";
const COLOR_DIV_3 = "rgb(0, 0, 150)";
const COLOR_DIV_4 = "rgb(100, 0, 100)";

const TEST_URI = `data:text/html,<!DOCTYPE html><meta charset=utf8>
    <style>
      body {
        margin: 0;
        height: 100vh;
        display: grid;
        grid-template-columns: 1fr 1fr;
        grid-template-rows: 1fr 1fr;
      }
      div:nth-child(1) { background-color: ${COLOR_DIV_1}; }
      div:nth-child(2) { background-color: ${COLOR_DIV_2}; }
      div:nth-child(3) { background-color: ${COLOR_DIV_3}; }
      div:nth-child(4) { background-color: ${COLOR_DIV_4}; }
    </style>
    <body>
      <div></div>
      <div></div>
      <div></div>
      <div></div>
    </body>`;

add_task(async function () {
  await addTab(TEST_URI);
  const hud = await BrowserConsoleManager.toggleBrowserConsole();

  info("Execute :screenshot");
  const file = new FileUtils.File(
    PathUtils.join(PathUtils.tempDir, "TestScreenshotFile.png")
  );
  // on some machines, such as macOS, dpr is set to 2. This is expected behavior, however
  // to keep tests consistant across OSs we are setting the dpr to 1
  const command = `:screenshot ${file.path} --dpr 1`;

  await executeAndWaitForMessageByType(
    hud,
    command,
    `Saved to ${file.path}`,
    ".console-api"
  );

  info("Create an image using the downloaded file as source");
  const image = new Image();
  image.src = PathUtils.toFileURI(file.path);
  await once(image, "load");

  info(
    "Retrieve the position of the elements relatively to the browser viewport"
  );
  const bodyBounds = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    async () => {
      return content.document.body.getBoxQuadsFromWindowOrigin()[0].getBounds();
    }
  );

  const center = [
    bodyBounds.left + bodyBounds.width / 2,
    bodyBounds.top + bodyBounds.height / 2,
  ];

  info(
    "Check that the divs of the content page were rendered on the screenshot"
  );
  checkImageColorAt({
    image,
    x: center[0] - 50,
    y: center[1] - 50,
    expectedColor: COLOR_DIV_1,
    label: "The screenshot did render the first div of the content page",
  });
  checkImageColorAt({
    image,
    x: center[0] + 50,
    y: center[1] - 50,
    expectedColor: COLOR_DIV_2,
    label: "The screenshot did render the second div of the content page",
  });
  checkImageColorAt({
    image,
    x: center[0] - 50,
    y: center[1] + 50,
    expectedColor: COLOR_DIV_3,
    label: "The screenshot did render the third div of the content page",
  });
  checkImageColorAt({
    image,
    x: center[0] + 50,
    y: center[1] + 50,
    expectedColor: COLOR_DIV_4,
    label: "The screenshot did render the fourth div of the content page",
  });

  info("Remove the downloaded screenshot file and cleanup downloads");
  await IOUtils.remove(file.path);
  await resetDownloads();
});
