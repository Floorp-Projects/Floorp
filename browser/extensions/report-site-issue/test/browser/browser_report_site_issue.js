"use strict";

async function clickToReportAndAwaitReportTabLoad() {
  const helpMenu = new HelpMenuHelper();
  await helpMenu.open();

  // click on "report site issue" and wait for the new tab to open
  const tab = await new Promise(resolve => {
    gBrowser.tabContainer.addEventListener(
      "TabOpen",
      event => {
        resolve(event.target);
      },
      { once: true }
    );
    document.getElementById("help_reportSiteIssue").click();
  });

  // wait for the new tab to acknowledge that it received a screenshot
  await BrowserTestUtils.waitForContentEvent(
    gBrowser.selectedBrowser,
    "ScreenshotReceived",
    false,
    null,
    true
  );

  await helpMenu.close();

  return tab;
}

add_task(async function start_issue_server() {
  requestLongerTimeout(2);

  const serverLanding = await startIssueServer();

  // ./head.js sets the value for PREF_WC_REPORTER_ENDPOINT
  await SpecialPowers.pushPrefEnv({
    set: [
      ["datareporting.healthreport.uploadEnabled", true],
      [PREF_WC_REPORTER_ENABLED, true],
      [PREF_WC_REPORTER_ENDPOINT, serverLanding],
    ],
  });
});

/* Test that clicking on the Report Site Issue button opens a new tab
   and sends a postMessaged blob to it. */
add_task(async function test_opened_page() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE);
  let tab2 = await clickToReportAndAwaitReportTabLoad();

  await SpecialPowers.spawn(
    tab2.linkedBrowser,
    [{ TEST_PAGE }],
    async function (args) {
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
        function isPixelGreenFuzzy(p) {
          // jpeg, so it will be off slightly
          const fuzz = 4;
          return p[0] < fuzz && Math.abs(p[1] - 128) < fuzz && p[2] < fuzz;
        }
        ok(isPixelGreenFuzzy(getPixel(0, 0)), "The pixels were green");
      }

      let doc = content.document;
      let urlParam = doc.getElementById("url").innerText;
      let preview = doc.getElementById("screenshot-preview");
      const URL =
        "http://example.com/browser/browser/extensions/report-site-issue/test/browser/test.html";
      is(
        urlParam,
        args.TEST_PAGE,
        "Reported page is correctly added to the url param"
      );

      let docShell = content.docShell;
      is(
        typeof docShell.getHasTrackingContentBlocked,
        "function",
        "docShell.hasTrackingContentBlocked is available"
      );

      let detailsParam = doc.getElementById("details").innerText;
      const details = JSON.parse(detailsParam);
      ok(
        typeof details == "object",
        "Details param is a stringified JSON object."
      );
      ok(Array.isArray(details.consoleLog), "Details has a consoleLog array.");

      const log1 = details.consoleLog[0];
      is(log1.log[0], null, "Can handle degenerate console logs");
      is(log1.level, "log", "Reports correct log level");
      is(log1.uri, URL, "Reports correct url");
      is(log1.pos, "7:13", "Reports correct line and column");

      const log2 = details.consoleLog[1];
      is(log2.log[0], "colored message", "Can handle fancy console logs");
      is(log2.level, "error", "Reports correct log level");
      is(log2.uri, URL, "Reports correct url");
      is(log2.pos, "8:13", "Reports correct line and column");

      const log3 = details.consoleLog[2];
      const loggedObject = log3.log[0];
      is(loggedObject.testobj, "{...}", "Reports object inside object");
      is(
        loggedObject.testSymbol,
        "Symbol(sym)",
        "Reports symbol inside object"
      );
      is(loggedObject.testnumber, 1, "Reports number inside object");
      is(loggedObject.testArray, "(4)[...]", "Reports array inside object");
      is(loggedObject.testUndf, "undefined", "Reports undefined inside object");
      is(loggedObject.testNull, null, "Reports null inside object");
      is(
        loggedObject.testFunc,
        undefined,
        "Reports function inside object as undefined due to security reasons"
      );
      is(loggedObject.testString, "string", "Reports string inside object");
      is(loggedObject.c, "{...}", "Reports circular reference inside object");
      is(
        Object.keys(loggedObject).length,
        10,
        "Preview has 10 keys inside object"
      );
      is(log3.level, "log", "Reports correct log level");
      is(log3.uri, URL, "Reports correct url");
      is(log3.pos, "24:13", "Reports correct line and column");

      const log4 = details.consoleLog[3];
      const loggedArray = log4.log[0];
      is(loggedArray[0], "string", "Reports string inside array");
      is(loggedArray[1], "{...}", "Reports object inside array");
      is(loggedArray[2], null, "Reports null inside array");
      is(loggedArray[3], 90, "Reports number inside array");
      is(loggedArray[4], "undefined", "Reports undefined inside array");
      is(
        loggedArray[5],
        "undefined",
        "Reports function inside array as undefined due to security reasons"
      );
      is(loggedArray[6], "(4)[...]", "Reports array inside array");
      is(loggedArray[7], "(8)[...]", "Reports circular array inside array");

      const log5 = details.consoleLog[4];
      ok(
        log5.log[0].match(/TypeError: .*document\.access is undefined/),
        "Script errors are logged"
      );
      is(log5.level, "error", "Reports correct log level");
      is(log5.uri, URL, "Reports correct url");
      is(log5.pos, "36:5", "Reports correct line and column");

      ok(typeof details.buildID == "string", "Details has a buildID string.");
      ok(typeof details.channel == "string", "Details has a channel string.");
      ok(
        typeof details.hasTouchScreen == "boolean",
        "Details has a hasTouchScreen flag."
      );
      ok(
        typeof details.hasFastClick == "undefined",
        "Details does not have FastClick if not found."
      );
      ok(
        typeof details.hasMobify == "undefined",
        "Details does not have Mobify if not found."
      );
      ok(
        typeof details.hasMarfeel == "undefined",
        "Details does not have Marfeel if not found."
      );
      ok(
        typeof details["mixed active content blocked"] == "boolean",
        "Details has a mixed active content blocked flag."
      );
      ok(
        typeof details["mixed passive content blocked"] == "boolean",
        "Details has a mixed passive content blocked flag."
      );
      ok(
        typeof details["tracking content blocked"] == "string",
        "Details has a tracking content blocked string."
      );
      ok(
        typeof details["gfx.webrender.all"] == "boolean",
        "Details has gfx.webrender.all."
      );

      is(
        preview.innerText,
        "Pass",
        "A Blob object was successfully transferred to the test page."
      );

      const bgUrl = preview.style.backgroundImage.match(/url\(\"(.*)\"\)/)[1];
      ok(
        bgUrl.startsWith("data:image/jpeg;base64,"),
        "A jpeg screenshot was successfully postMessaged"
      );
      await isGreen(bgUrl);
    }
  );

  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab1);
});

add_task(async function test_framework_detection() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    FRAMEWORKS_TEST_PAGE
  );
  let tab2 = await clickToReportAndAwaitReportTabLoad();

  await SpecialPowers.spawn(tab2.linkedBrowser, [], async function (args) {
    let doc = content.document;
    let detailsParam = doc.getElementById("details").innerText;
    const details = JSON.parse(detailsParam);
    ok(
      typeof details == "object",
      "Details param is a stringified JSON object."
    );
    is(details.hasFastClick, true, "FastClick was found.");
    is(details.hasMobify, true, "Mobify was found.");
    is(details.hasMarfeel, true, "Marfeel was found.");
  });

  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab1);
});

add_task(async function test_fastclick_detection() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    FASTCLICK_TEST_PAGE
  );
  let tab2 = await clickToReportAndAwaitReportTabLoad();

  await SpecialPowers.spawn(tab2.linkedBrowser, [], async function (args) {
    let doc = content.document;
    let detailsParam = doc.getElementById("details").innerText;
    const details = JSON.parse(detailsParam);
    ok(
      typeof details == "object",
      "Details param is a stringified JSON object."
    );
    is(details.hasFastClick, true, "FastClick was found.");
  });

  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab1);
});

add_task(async function test_framework_label() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    FRAMEWORKS_TEST_PAGE
  );
  let tab2 = await clickToReportAndAwaitReportTabLoad();

  await SpecialPowers.spawn(tab2.linkedBrowser, [], async function (args) {
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
