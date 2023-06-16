"use strict";

const HTTPS_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);
const HTTP_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
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

  let loaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  BrowserTestUtils.loadURIString(
    gBrowser,
    `${HTTPS_PATH}file_same_site_cookies_bug1748693.sjs?setcookies`
  );
  await loaded;

  loaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  BrowserTestUtils.loadURIString(
    gBrowser,
    `${HTTP_PATH}file_same_site_cookies_bug1748693.sjs`
  );
  await loaded;

  await checkCookies({ auth: true, auth_secure: false });

  finish();
});
