/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that fullpage screenshot command works properly with fixed elements

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/test/browser/test_jsterm_screenshot_command.html";

// on some machines, such as macOS, dpr is set to 2. This is expected behavior, however
// to keep tests consistant across OSs we are setting the dpr to 1
const dpr = "--dpr 1";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  info("Scroll in the content page");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async () => {
    // Overflow the page
    content.document.body.classList.add("overflow");
    content.wrappedJSObject.scrollTo(200, 350);
  });

  info("Execute :screenshot --fullpage");
  const file = new FileUtils.File(
    PathUtils.join(PathUtils.tempDir, "TestScreenshotFile.png")
  );
  const command = `:screenshot ${file.path} ${dpr} --fullpage`;
  // `-fullpage` is appended at the end of the provided filename
  const actualFilePath = file.path.replace(".png", "-fullpage.png");
  await executeAndWaitForMessageByType(
    hud,
    command,
    `Saved to ${file.path.replace(".png", "-fullpage.png")}`,
    ".console-api"
  );

  info("Create an image using the downloaded file as source");
  const image = new Image();
  image.src = PathUtils.toFileURI(actualFilePath);
  await once(image, "load");

  info("Check that the fixed element is rendered at the expected position");
  checkImageColorAt({
    image,
    x: 0,
    y: 0,
    expectedColor: `rgb(255, 0, 0)`,
    label:
      "The top-left corner has the expected red color, matching the header element",
  });

  const scrollPosition = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    async () => {
      return [content.wrappedJSObject.scrollX, content.wrappedJSObject.scrollY];
    }
  );
  is(
    scrollPosition.join("|"),
    "200|350",
    "The page still has the same scroll positions as before taking the screenshot"
  );

  info("Remove the downloaded screenshot file and cleanup downloads");
  await IOUtils.remove(actualFilePath);
  await resetDownloads();
});
