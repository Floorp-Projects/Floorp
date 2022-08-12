/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const exampleOrgDocument = `https://example.org/document-builder.sjs`;
const exampleComDocument = `https://example.com/document-builder.sjs`;

const TEST_URL = `${exampleOrgDocument}?html=
  <style>
    body {
      margin: 0;
      height: 10001px;
    }
    iframe {
      height: 50px;
      border:none;
      display: block;
    }
  </style>
  <iframe
    src="${exampleOrgDocument}?html=<body style='margin:0;height:30px;background:rgb(255,0,0)'></body>"
    id="same-origin"></iframe>
  <iframe
    src="${exampleComDocument}?html=<body style='margin:0;height:30px;background:rgb(0,255,0)'></body>"
    id="remote"></iframe>`;

add_task(async function() {
  await pushPref("devtools.command-button-screenshot.enabled", true);

  await addTab(TEST_URL);

  info("Open the toolbox");
  const toolbox = await gDevTools.showToolboxForTab(gBrowser.selectedTab);

  const onScreenshotDownloaded = waitUntilScreenshot();
  toolbox.doc.querySelector("#command-button-screenshot").click();
  const filePath = await onScreenshotDownloaded;

  ok(!!filePath, "The screenshot was taken");

  info("Create an image using the downloaded file as source");
  const image = new Image();
  const onImageLoad = once(image, "load");
  image.src = PathUtils.toFileURI(filePath);
  await onImageLoad;

  const dpr = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => content.wrappedJSObject.devicePixelRatio
  );

  info("Check that the same-origin iframe is rendered in the screenshot");
  await checkImageColorAt({
    image,
    y: 10 * dpr,
    expectedColor: `rgb(255, 0, 0)`,
    label: "The same-origin iframe is rendered properly in the screenshot",
  });

  info("Check that the remote iframe is rendered in the screenshot");
  await checkImageColorAt({
    image,
    y: 60 * dpr,
    expectedColor: `rgb(0, 255, 0)`,
    label: "The remote iframe is rendered properly in the screenshot",
  });

  info(
    "Check that a warning message was displayed to indicate the screenshot was truncated"
  );
  const notificationBox = await waitFor(() =>
    toolbox.doc.querySelector(".notificationbox")
  );

  const message = notificationBox.querySelector(".notification").textContent;
  ok(
    message.startsWith("The image was cut off"),
    `The warning message is rendered as expected (${message})`
  );

  // Remove the downloaded screenshot file
  await IOUtils.remove(filePath);

  info(
    "Check that taking a screenshot in a private window doesn't appear in the non-private window"
  );
  const privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  ok(PrivateBrowsingUtils.isWindowPrivate(privateWindow), "window is private");
  const privateBrowser = privateWindow.gBrowser;
  privateBrowser.selectedTab = BrowserTestUtils.addTab(
    privateBrowser,
    TEST_URL
  );

  info("private tab opened");
  ok(
    PrivateBrowsingUtils.isBrowserPrivate(privateBrowser.selectedBrowser),
    "tab window is private"
  );

  const privateToolbox = await gDevTools.showToolboxForTab(
    privateBrowser.selectedTab
  );

  const onPrivateScreenshotDownloaded = waitUntilScreenshot({
    isWindowPrivate: true,
  });
  privateToolbox.doc.querySelector("#command-button-screenshot").click();
  const privateScreenshotFilePath = await onPrivateScreenshotDownloaded;
  ok(
    !!privateScreenshotFilePath,
    "The screenshot was taken in the private window"
  );

  // Remove the downloaded screenshot file
  await IOUtils.remove(privateScreenshotFilePath);

  // cleanup the downloads
  await resetDownloads();

  const closePromise = BrowserTestUtils.windowClosed(privateWindow);
  privateWindow.BrowserTryToCloseWindow();
  await closePromise;
});
