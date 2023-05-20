/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test steps:
// 1. Load file_redirect_tainting.sjs?html.
// 2. The server returns an html which loads an image at http://example.net.
// 3. The image request will be upgraded to HTTPS since HTTPS-only mode is on.
// 4. In file_redirect_tainting.sjs, we set "Access-Control-Allow-Origin" to
//    the value of the Origin header.
// 5. If the vlaue does not match, the image won't be loaded.
async function do_test() {
  let requestUrl = `https://example.com/browser/dom/security/test/https-only/file_redirect_tainting.sjs?html`;

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: requestUrl,
      waitForLoad: true,
    },
    async function (browser) {
      let imageLoaded = await SpecialPowers.spawn(browser, [], function () {
        let image = content.document.getElementById("test_image");
        return image && image.complete && image.naturalHeight !== 0;
      });
      await Assert.ok(imageLoaded, "test_image should be loaded");
    }
  );
}

add_task(async function test_https_only_redirect_tainting() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_only_mode", true]],
  });

  await do_test();

  await SpecialPowers.popPrefEnv();
});
