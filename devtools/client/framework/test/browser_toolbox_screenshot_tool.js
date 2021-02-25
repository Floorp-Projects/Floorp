/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const exampleNetDocument = `http://example.net/document-builder.sjs`;
const exampleComDocument = `http://example.com/document-builder.sjs`;

const TEST_URL = `${exampleNetDocument}?html=
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
    src="${exampleNetDocument}?html=<body style='margin:0;height:30px;background:rgb(255,0,0)'></body>"
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

  ok(filePath, "The screenshot was taken");

  info("Create an image using the downloaded file as source");
  const image = new Image();
  const onImageLoad = once(image, "load");
  image.src = OS.Path.toFileURI(filePath);
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

  //Remove the downloaded screenshot file
  await OS.File.remove(filePath);
  await resetDownloads();
});
