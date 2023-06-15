/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that screenshot command works properly with the --selector arg

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test_jsterm_screenshot_command.html";

// on some machines, such as macOS, dpr is set to 2. This is expected behavior, however
// to keep tests consistant across OSs we are setting the dpr to 1
const dpr = "--dpr 1";

add_task(async function () {
  await pushPref("devtools.webconsole.input.context", true);

  const hud = await openNewTabAndConsole(TEST_URI);

  info("wait for the iframes to be loaded");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async () => {
    await ContentTaskUtils.waitForCondition(
      () => content.document.querySelectorAll(".loaded-iframe").length == 2
    );
  });

  info("Test :screenshot --selector iframe");
  const sameOriginIframeScreenshotFile = new FileUtils.File(
    PathUtils.join(
      PathUtils.tempDir,
      "TestScreenshotFile-same-origin-iframe.png"
    )
  );
  await executeAndWaitForMessageByType(
    hud,
    `:screenshot --selector #same-origin-iframe ${sameOriginIframeScreenshotFile.path} ${dpr}`,
    `Saved to ${sameOriginIframeScreenshotFile.path}`,
    ".console-api"
  );

  let fileExists = sameOriginIframeScreenshotFile.exists();
  if (!fileExists) {
    throw new Error(`${sameOriginIframeScreenshotFile.path} does not exist`);
  }

  ok(
    fileExists,
    `Screenshot was saved to ${sameOriginIframeScreenshotFile.path}`
  );

  info("Create an image using the downloaded file as source");
  let image = new Image();
  image.src = PathUtils.toFileURI(sameOriginIframeScreenshotFile.path);
  await once(image, "load");

  info("Check that the node was rendered as expected in the screenshot");
  checkImageColorAt({
    image,
    y: 0,
    expectedColor: `rgb(255, 255, 0)`,
    label:
      "The top-left corner has the expected color, matching the same-origin iframe",
  });

  // Remove the downloaded screenshot file and cleanup downloads
  await IOUtils.remove(sameOriginIframeScreenshotFile.path);
  await resetDownloads();

  info("Check using :screenshot --selector in a remote-iframe context");
  // Select the remote iframe in the context selector
  const evaluationContextSelectorButton = hud.ui.outputNode.querySelector(
    ".webconsole-evaluation-selector-button"
  );

  if (!isFissionEnabled() && !isEveryFrameTargetEnabled()) {
    is(
      evaluationContextSelectorButton,
      null,
      "context selector is only displayed when Fission or EFT is enabled"
    );
    return;
  }

  const remoteIframeUrl = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    async () => {
      return content.document.querySelector("#remote-iframe").src;
    }
  );
  selectTargetInContextSelector(hud, remoteIframeUrl);
  await waitFor(() =>
    evaluationContextSelectorButton.innerText.includes("example.org")
  );

  const remoteIframeSpanScreenshot = new FileUtils.File(
    PathUtils.join(PathUtils.tempDir, "TestScreenshotFile-remote-iframe.png")
  );
  await executeAndWaitForMessageByType(
    hud,
    `:screenshot --selector span ${remoteIframeSpanScreenshot.path} ${dpr}`,
    `Saved to ${remoteIframeSpanScreenshot.path}`,
    ".console-api"
  );

  fileExists = remoteIframeSpanScreenshot.exists();
  if (!fileExists) {
    throw new Error(`${remoteIframeSpanScreenshot.path} does not exist`);
  }

  ok(fileExists, `Screenshot was saved to ${remoteIframeSpanScreenshot.path}`);

  info("Create an image using the downloaded file as source");
  image = new Image();
  image.src = PathUtils.toFileURI(remoteIframeSpanScreenshot.path);
  await once(image, "load");

  info("Check that the node was rendered as expected in the screenshot");
  checkImageColorAt({
    image,
    y: 0,
    expectedColor: `rgb(0, 100, 0)`,
    label:
      "The top-left corner has the expected color, matching the span inside the iframe",
  });

  info(
    "Check that using a selector that doesn't match any element displays a warning in console"
  );
  await executeAndWaitForMessageByType(
    hud,
    `:screenshot --selector #this-element-does-not-exist`,
    `The ‘#this-element-does-not-exist’ selector does not match any element on the page.`,
    ".warn"
  );
  ok(
    true,
    "A warning message is emitted when the passed selector doesn't match any element"
  );

  // Remove the downloaded screenshot file and cleanup downloads
  await IOUtils.remove(remoteIframeSpanScreenshot.path);
  await resetDownloads();
});
