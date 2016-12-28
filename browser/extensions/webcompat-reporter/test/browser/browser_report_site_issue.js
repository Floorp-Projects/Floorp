/* Test that clicking on the Report Site Issue button opens a new tab
   and sends a postMessaged blob at it.
   testing/profiles/prefs_general.js sets the value for
   "extensions.webcompat-reporter.newIssueEndpoint" */
add_task(function* test_screenshot() {
  yield SpecialPowers.pushPrefEnv({set: [[PREF_WC_REPORTER_ENDPOINT, NEW_ISSUE_PAGE]]});

  let tab1 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE);
  yield PanelUI.show();

  let webcompatButton = document.getElementById("webcompat-reporter-button");
  ok(webcompatButton, "Report Site Issue button exists.");

  let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);
  webcompatButton.click();
  let tab2 = yield newTabPromise;

  yield BrowserTestUtils.waitForContentEvent(tab2.linkedBrowser, "ScreenshotReceived", false, null, true);

  yield ContentTask.spawn(tab2.linkedBrowser, {TEST_PAGE}, function(args) {
    let doc = content.document;
    let urlParam = doc.getElementById("url").innerText;
    let preview = doc.getElementById("screenshot-preview");
    is(urlParam, args.TEST_PAGE, "Reported page is correctly added to the url param");

    is(preview.innerText, "Pass", "A Blob object was successfully transferred to the test page.")
    ok(preview.style.backgroundImage.startsWith("url(\"data:image/png;base64,iVBOR"), "A green screenshot was successfully postMessaged");
  });

  yield BrowserTestUtils.removeTab(tab2);
  yield BrowserTestUtils.removeTab(tab1);
});
