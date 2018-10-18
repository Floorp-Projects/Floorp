"use strict";

/* Test that clicking on the Report Site Issue button opens a new tab
   and sends a postMessaged blob to it. */
add_task(async function test_opened_page() {
  requestLongerTimeout(2);

  const serverLanding = await startIssueServer();

  // ./head.js sets the value for PREF_WC_REPORTER_ENDPOINT
  await SpecialPowers.pushPrefEnv({set: [
    [PREF_WC_REPORTER_ENABLED, true],
    [PREF_WC_REPORTER_ENDPOINT, serverLanding],
  ]});

  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE);

  await openPageActions();
  await isPanelItemEnabled();

  let screenshotPromise;
  let newTabPromise = new Promise(resolve => {
    gBrowser.tabContainer.addEventListener("TabOpen", event => {
      let tab = event.target;
      screenshotPromise = BrowserTestUtils.waitForContentEvent(
        tab.linkedBrowser, "ScreenshotReceived", false, null, true);
      resolve(tab);
    }, {once: true});
  });
  document.getElementById(WC_PAGE_ACTION_PANEL_ID).click();
  let tab2 = await newTabPromise;
  await screenshotPromise;

  await ContentTask.spawn(tab2.linkedBrowser, {TEST_PAGE}, async function(args) {
    async function isGreen(dataUrl) {
      const getPixel = await new Promise(resolve => {
        const myCanvas = content.document.createElement("canvas");
        const ctx = myCanvas.getContext("2d");
        const img = new content.Image();
        img.onload = () => {
          ctx.drawImage(img, 0, 0);
          resolve((x, y) => {
            return ctx.getImageData(x, y, 1, 1).data;
          });
        };
        img.src = dataUrl;
      });
      function isPixelGreenFuzzy(p) { // jpeg, so it will be off slightly
        const fuzz = 4;
        return p[0] < fuzz && Math.abs(p[1] - 128) < fuzz && p[2] < fuzz;
      }
      ok(isPixelGreenFuzzy(getPixel(0, 0)), "The pixels were green");
    }

    let doc = content.document;
    let urlParam = doc.getElementById("url").innerText;
    let preview = doc.getElementById("screenshot-preview");
    is(urlParam, args.TEST_PAGE, "Reported page is correctly added to the url param");

    let detailsParam = doc.getElementById("details").innerText;
    const details = JSON.parse(detailsParam);
    ok(typeof details == "object", "Details param is a stringified JSON object.");
    ok(Array.isArray(details.consoleLog), "Details has a consoleLog array.");
    ok(details.consoleLog[0].match(/console\.log\(null\)[\s\S]*test.html:\d+:\d+/m), "Can handle degenerate console logs");
    ok(details.consoleLog[1].match(/console\.error\(colored message\)[\s\S]*test.html:\d+:\d+/m), "Can handle fancy console logs");
    ok(details.consoleLog[2].match(/document\.access is undefined[\s\S]*test.html:\d+:\d+/m), "Script errors are logged");
    ok(typeof details.buildID == "string", "Details has a buildID string.");
    ok(typeof details.channel == "string", "Details has a channel string.");
    ok(typeof details.hasTouchScreen == "boolean", "Details has a hasTouchScreen flag.");
    ok(typeof details["mixed active content blocked"] == "boolean", "Details has a mixed active content blocked flag.");
    ok(typeof details["mixed passive content blocked"] == "boolean", "Details has a mixed passive content blocked flag.");
    ok(typeof details["tracking content blocked"] == "string", "Details has a tracking content blocked string.");
    ok(typeof details["gfx.webrender.all"] == "boolean", "Details has gfx.webrender.all.");
    ok(typeof details["gfx.webrender.blob-images"] == "boolean", "Details has gfx.webrender.blob-images.");
    ok(typeof details["gfx.webrender.enabled"] == "boolean", "Details has gfx.webrender.enabled.");
    ok(typeof details["image.mem.shared"] == "boolean", "Details has image.mem.shared.");

    is(preview.innerText, "Pass", "A Blob object was successfully transferred to the test page.");

    const bgUrl = preview.style.backgroundImage.match(/url\(\"(.*)\"\)/)[1];
    ok(bgUrl.startsWith("data:image/jpeg;base64,"), "A jpeg screenshot was successfully postMessaged");
    await isGreen(bgUrl);
  });

  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab1);
});
