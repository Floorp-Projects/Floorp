/* Test that clicking on the Report Site Issue button opens a new tab
   and sends a postMessaged blob to it. */
add_task(async function test_screenshot() {
  requestLongerTimeout(2);

  // ./head.js sets the value for PREF_WC_REPORTER_ENDPOINT
  await SpecialPowers.pushPrefEnv({set: [[PREF_WC_REPORTER_ENDPOINT, NEW_ISSUE_PAGE]]});

  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE);

  let webcompatButton = document.getElementById(WC_PAGE_ACTION_ID);
  ok(webcompatButton, "Report Site Issue button exists.");

  let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);
  openPageActions();
  webcompatButton.click();
  let tab2 = await newTabPromise;

  await BrowserTestUtils.waitForContentEvent(tab2.linkedBrowser, "ScreenshotReceived", false, null, true);

  await ContentTask.spawn(tab2.linkedBrowser, {TEST_PAGE}, function(args) {
    let doc = content.document;
    let urlParam = doc.getElementById("url").innerText;
    let preview = doc.getElementById("screenshot-preview");
    is(urlParam, args.TEST_PAGE, "Reported page is correctly added to the url param");

    is(preview.innerText, "Pass", "A Blob object was successfully transferred to the test page.");
    ok(preview.style.backgroundImage.startsWith("url(\"data:image/png;base64,iVBOR"), "A green screenshot was successfully postMessaged");
  });

  await BrowserTestUtils.removeTab(tab2);
  await BrowserTestUtils.removeTab(tab1);
});
