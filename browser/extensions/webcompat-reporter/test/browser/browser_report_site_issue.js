/* Test that clicking on the Report Site Issue button opens a new tab
   and sends a postMessaged blob to it. */
add_task(async function test_opened_page() {
  requestLongerTimeout(2);

  // ./head.js sets the value for PREF_WC_REPORTER_ENDPOINT
  await SpecialPowers.pushPrefEnv({set: [
    [PREF_WC_REPORTER_ENABLED, true],
    [PREF_WC_REPORTER_ENDPOINT, NEW_ISSUE_PAGE]
  ]});

  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE);

  await openPageActions();
  let webcompatButton = document.getElementById(WC_PAGE_ACTION_ID);
  ok(webcompatButton, "Report Site Issue button exists.");

  let screenshotPromise;
  let newTabPromise = new Promise(resolve => {
    gBrowser.tabContainer.addEventListener("TabOpen", event => {
      let tab = event.target;
      screenshotPromise = BrowserTestUtils.waitForContentEvent(
        tab.linkedBrowser, "ScreenshotReceived", false, null, true);
      resolve(tab);
    }, { once: true });
  });
  webcompatButton.click();
  let tab2 = await newTabPromise;
  await screenshotPromise;

  await ContentTask.spawn(tab2.linkedBrowser, {TEST_PAGE}, function(args) {
    let doc = content.document;
    let urlParam = doc.getElementById("url").innerText;
    let preview = doc.getElementById("screenshot-preview");
    is(urlParam, args.TEST_PAGE, "Reported page is correctly added to the url param");

    let detailsParam = doc.getElementById("details").innerText;
    ok(typeof JSON.parse(detailsParam) == "object", "Details param is a stringified JSON object.");

    is(preview.innerText, "Pass", "A Blob object was successfully transferred to the test page.");
    ok(preview.style.backgroundImage.startsWith("url(\"data:image/png;base64,iVBOR"), "A green screenshot was successfully postMessaged");
  });

  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab1);
});
