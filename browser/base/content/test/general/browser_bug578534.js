/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test() {
  let uriString = "http://example.com/";
  let cookieBehavior = "network.cookie.cookieBehavior";
  let uriObj = Services.io.newURI(uriString);

  await SpecialPowers.pushPrefEnv({ set: [[cookieBehavior, 2]] });
  Services.perms.add(uriObj, "cookie", Services.perms.ALLOW_ACTION);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: uriString },
    async function(browser) {
      await ContentTask.spawn(browser, null, function() {
        is(
          content.navigator.cookieEnabled,
          true,
          "navigator.cookieEnabled should be true"
        );
      });
    }
  );

  Services.perms.add(uriObj, "cookie", Services.perms.UNKNOWN_ACTION);
});
