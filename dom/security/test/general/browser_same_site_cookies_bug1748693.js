"use strict";

const HTTPS_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);
const HTTP_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  // Disable eslint, since we explicitly need a insecure URL here for this test.
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.com"
);

function checkCookies(expectedCookies = {}) {
  info(JSON.stringify(expectedCookies));
  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [expectedCookies],
    async function (expectedCookies) {
      let cookies = content.document.getElementById("msg").innerHTML;
      info(cookies);
      for (const [cookie, expected] of Object.entries(expectedCookies)) {
        if (expected) {
          ok(cookies.includes(cookie), `${cookie} should be sent`);
        } else {
          ok(!cookies.includes(cookie), `${cookie} should not be sent`);
        }
      }
    }
  );
}

add_task(async function bug1748693() {
  waitForExplicitFinish();

  // HTTPS-First would interfere with this test. We want to check wether
  // cookies orignally set on a secure site without a "Secure" attribute
  // get loaded on a insecure site. For that, we need to visit a
  // insecure site, which would otherwise be upgraded by HTTPS-First.
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_first", false]],
  });

  let loaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  BrowserTestUtils.startLoadingURIString(
    gBrowser,
    `${HTTPS_PATH}file_same_site_cookies_bug1748693.sjs?setcookies`
  );
  await loaded;

  loaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  BrowserTestUtils.startLoadingURIString(
    gBrowser,
    `${HTTP_PATH}file_same_site_cookies_bug1748693.sjs`
  );
  await loaded;

  await checkCookies({ auth: true, auth_secure: false });

  finish();
});
