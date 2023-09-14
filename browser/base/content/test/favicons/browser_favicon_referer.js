/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const FOLDER = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "http://mochi.test:8888/"
);

add_task(async function test_check_referrer_for_discovered_favicon() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async browser => {
      let referrerPromise = TestUtils.topicObserved(
        "http-on-modify-request",
        (s, t, d) => {
          let chan = s.QueryInterface(Ci.nsIHttpChannel);
          return chan.URI.spec == "http://mochi.test:8888/favicon.ico";
        }
      ).then(([chan]) => chan.getRequestHeader("Referer"));

      BrowserTestUtils.startLoadingURIString(
        browser,
        `${FOLDER}discovery.html`
      );

      let referrer = await referrerPromise;
      is(
        referrer,
        `${FOLDER}discovery.html`,
        "Should have sent referrer for autodiscovered favicon."
      );
    }
  );
});

add_task(
  async function test_check_referrer_for_referrerpolicy_explicit_favicon() {
    await BrowserTestUtils.withNewTab(
      { gBrowser, url: "about:blank" },
      async browser => {
        let referrerPromise = TestUtils.topicObserved(
          "http-on-modify-request",
          (s, t, d) => {
            let chan = s.QueryInterface(Ci.nsIHttpChannel);
            return chan.URI.spec == `${FOLDER}file_favicon.png`;
          }
        ).then(([chan]) => chan.getRequestHeader("Referer"));

        BrowserTestUtils.startLoadingURIString(
          browser,
          `${FOLDER}file_favicon_no_referrer.html`
        );

        let referrer = await referrerPromise;
        is(
          referrer,
          "http://mochi.test:8888/",
          "Should have sent the origin referrer only due to the per-link referrer policy specified."
        );
      }
    );
  }
);
