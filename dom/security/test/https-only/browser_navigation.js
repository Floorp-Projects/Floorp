"use strict";

// For each FIRST_URL_* this test does the following:
// 1. Navigate to FIRST_URL_*
// 2. Check if we are on a HTTPS-Only error page
// 3. Navigate to SECOND_URL
// 4. Navigate back
// 5. Check if we are on a HTTPS-Only error page

const FIRST_URL_SECURE = "https://example.com";
const FIRST_URL_INSECURE_REDIRECT =
  "http://example.com/browser/dom/security/test/https-only/file_redirect_to_insecure.sjs";
const FIRST_URL_INSECURE_NOCERT = "http://nocert.example.com";
const SECOND_URL = "https://example.org";

function waitForPage() {
  return new Promise(resolve => {
    BrowserTestUtils.waitForErrorPage(gBrowser.selectedBrowser).then(resolve);
    BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(resolve);
  });
}

async function verifyErrorPage(expectErrorPage = true) {
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [expectErrorPage],
    async function (_expectErrorPage) {
      let doc = content.document;
      let innerHTML = doc.body.innerHTML;
      let errorPageL10nId = "about-httpsonly-title-alert";

      is(
        innerHTML.includes(errorPageL10nId) &&
          doc.documentURI.startsWith("about:httpsonlyerror"),
        _expectErrorPage,
        "we should be on the https-only error page"
      );
    }
  );
}

async function runTest(
  firstUrl,
  expectErrorPageOnFirstVisit,
  expectErrorPageOnSecondVisit
) {
  let loaded = waitForPage();
  info("Loading first page");
  BrowserTestUtils.startLoadingURIString(gBrowser, firstUrl);
  await loaded;
  await verifyErrorPage(expectErrorPageOnFirstVisit);

  loaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  info("Navigating to second page");
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [SECOND_URL],
    async url => (content.location.href = url)
  );
  await loaded;

  // Go back one site by clicking the back button
  loaded = BrowserTestUtils.waitForLocationChange(gBrowser);
  info("Clicking back button");
  let backButton = document.getElementById("back-button");
  backButton.click();
  await loaded;
  await verifyErrorPage(expectErrorPageOnSecondVisit);
}

add_task(async function () {
  waitForExplicitFinish();

  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_only_mode", true]],
  });

  // We don't expect any HTTPS-Only error pages, on the first and second visit of this URL,
  // since the URL is reachable via https.
  await runTest(FIRST_URL_SECURE, false, false);

  // Since trying to upgrade this url will result in being redirected again to the insecure
  // site, we are not able to upgrade it and a HTTPS-Only error page is shown.
  // This is happening both on the first and second visit.
  await runTest(FIRST_URL_INSECURE_REDIRECT, true, true);

  // Similar to the previous case, we can not upgrade this URL, since this time it has a
  // invalid certificate. We would expect a HTTPS-Only error page on both vists, but it is only
  // shown on the first one, on the second one we get an errror page about the invalid
  // certificate instead (Bug 1848117).
  await runTest(FIRST_URL_INSECURE_NOCERT, true, false);

  finish();
});
