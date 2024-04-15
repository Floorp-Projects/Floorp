/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that screenshot command works properly

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test_jsterm_screenshot_command.html";

// on some machines, such as macOS, dpr is set to 2. This is expected behavior, however
// to keep tests consistant across OSs we are setting the dpr to 1
const dpr = "--dpr 1";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  info("wait for the iframes to be loaded");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async () => {
    await ContentTaskUtils.waitForCondition(
      () => content.document.querySelectorAll(".loaded-iframe").length == 2
    );
  });

  info("Test :screenshot to file");
  const file = new FileUtils.File(
    PathUtils.join(PathUtils.tempDir, "TestScreenshotFile.png")
  );
  const command = `:screenshot ${file.path} ${dpr}`;
  await executeAndWaitForMessageByType(
    hud,
    command,
    `Saved to ${file.path}`,
    ".console-api"
  );

  const fileExists = file.exists();
  if (!fileExists) {
    throw new Error(`${file.path} does not exist`);
  }

  ok(fileExists, `Screenshot was saved to ${file.path}`);

  info("Create an image using the downloaded file as source");
  const image = new Image();
  image.src = PathUtils.toFileURI(file.path);
  await once(image, "load");

  // The page has the following structure
  // +--------------------------------------------------+
  // |        Fixed header [50px tall, red]             |
  // +--------------------------------------------------+
  // | Same-origin iframe [50px tall, rgb(255, 255, 0)] |
  // +--------------------------------------------------+
  // |    Remote iframe [50px tall, rgb(0, 255, 255)]   |
  // +--------------------------------------------------+
  // |  Image  |
  // |  100px  |
  // |         |
  // +---------+

  info("Check that the header is rendered in the screenshot");
  checkImageColorAt({
    image,
    y: 0,
    expectedColor: `rgb(255, 0, 0)`,
    label:
      "The top-left corner has the expected red color, matching the header element",
  });

  info("Check that the same-origin iframe is rendered in the screenshot");
  checkImageColorAt({
    image,
    y: 60,
    expectedColor: `rgb(255, 255, 0)`,
    label: "The same-origin iframe is rendered properly in the screenshot",
  });

  info("Check that the remote iframe is rendered in the screenshot");
  checkImageColorAt({
    image,
    y: 110,
    expectedColor: `rgb(0, 255, 255)`,
    label: "The remote iframe is rendered properly in the screenshot",
  });

  info("Test :screenshot to file default filename");
  const message = await executeAndWaitForMessageByType(
    hud,
    `:screenshot ${dpr}`,
    `Saved to`,
    ".console-api"
  );
  const date = new Date();
  const monthString = (date.getMonth() + 1).toString().padStart(2, "0");
  const dayString = date.getDate().toString().padStart(2, "0");
  const expectedDateString = `${date.getFullYear()}-${monthString}-${dayString}`;

  const { Downloads } = ChromeUtils.importESModule(
    "resource://gre/modules/Downloads.sys.mjs"
  );
  const screenshotDir = await Downloads.getPreferredDownloadsDirectory();

  const { renderedDate, filePath } =
    /Saved to (?<filePath>.*Screen Shot (?<renderedDate>\d{4}-\d{2}-\d{2}) at \d{2}.\d{2}.\d{2}\.png)/.exec(
      message.node.textContent
    ).groups;
  is(
    renderedDate,
    expectedDateString,
    `Screenshot file has expected default name (full message: ${message.node.textContent})`
  );
  is(
    filePath.startsWith(screenshotDir),
    true,
    `Screenshot file is saved in default directory`
  );

  info("Remove the downloaded screenshot files and cleanup downloads");
  await IOUtils.remove(file.path);
  await IOUtils.remove(filePath);
  await resetDownloads();
});
