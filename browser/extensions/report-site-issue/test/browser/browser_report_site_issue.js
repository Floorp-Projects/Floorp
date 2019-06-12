"use strict";

async function clickToReportAndAwaitReportTabLoad() {
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
  const tab = await newTabPromise;
  await screenshotPromise;
  return tab;
}

add_task(async function start_issue_server() {
  requestLongerTimeout(2);

  const serverLanding = await startIssueServer();

  // ./head.js sets the value for PREF_WC_REPORTER_ENDPOINT
  await SpecialPowers.pushPrefEnv({set: [
    [PREF_WC_REPORTER_ENABLED, true],
    [PREF_WC_REPORTER_ENDPOINT, serverLanding],
  ]});
});

/* Test that clicking on the Report Site Issue button opens a new tab
   and sends a postMessaged blob to it. */
add_task(async function test_opened_page() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE);
  let tab2 = await clickToReportAndAwaitReportTabLoad();

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

    let docShell = content.docShell;
    is(typeof docShell.hasMixedActiveContentBlocked, "boolean", "docShell.hasMixedActiveContentBlocked is available");
    is(typeof docShell.hasMixedDisplayContentBlocked, "boolean", "docShell.hasMixedDisplayContentBlocked is available");
    is(typeof docShell.hasTrackingContentBlocked, "boolean", "docShell.hasTrackingContentBlocked is available");

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
    ok(typeof details.hasFastClick == "undefined", "Details does not have FastClick if not found.");
    ok(typeof details.hasMobify == "undefined", "Details does not have Mobify if not found.");
    ok(typeof details.hasMarfeel == "undefined", "Details does not have Marfeel if not found.");
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

add_task(async function test_framework_detection() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, FRAMEWORKS_TEST_PAGE);
  let tab2 = await clickToReportAndAwaitReportTabLoad();

  await ContentTask.spawn(tab2.linkedBrowser, {}, async function(args) {
    let doc = content.document;
    let detailsParam = doc.getElementById("details").innerText;
    const details = JSON.parse(detailsParam);
    ok(typeof details == "object", "Details param is a stringified JSON object.");
    is(details.hasFastClick, true, "FastClick was found.");
    is(details.hasMobify, true, "Mobify was found.");
    is(details.hasMarfeel, true, "Marfeel was found.");
  });

  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab1);
});

add_task(async function test_fastclick_detection() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, FASTCLICK_TEST_PAGE);
  let tab2 = await clickToReportAndAwaitReportTabLoad();

  await ContentTask.spawn(tab2.linkedBrowser, {}, async function(args) {
    let doc = content.document;
    let detailsParam = doc.getElementById("details").innerText;
    const details = JSON.parse(detailsParam);
    ok(typeof details == "object", "Details param is a stringified JSON object.");
    is(details.hasFastClick, true, "FastClick was found.");
  });

  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab1);
});

add_task(async function test_framework_label() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, FRAMEWORKS_TEST_PAGE);
  let tab2 = await clickToReportAndAwaitReportTabLoad();

  await ContentTask.spawn(tab2.linkedBrowser, {}, async function(args) {
    let doc = content.document;
    let labelParam = doc.getElementById("label").innerText;
    const label = JSON.parse(labelParam);
    ok(typeof label == "object", "Label param is a stringified JSON object.");
    is(label.includes("type-fastclick"), true, "FastClick was found.");
    is(label.includes("type-mobify"), true, "Mobify was found.");
    is(label.includes("type-marfeel"), true, "Marfeel was found.");
  });

  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab1);
});
