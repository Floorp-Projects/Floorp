/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { PermissionTestUtils } = ChromeUtils.import(
  "resource://testing-common/PermissionTestUtils.jsm"
);

add_task(async function test() {
  let uriString = "http://example.com/";
  let cookieBehavior = "network.cookie.cookieBehavior";

  await SpecialPowers.pushPrefEnv({ set: [[cookieBehavior, 2]] });
  PermissionTestUtils.add(uriString, "cookie", Services.perms.ALLOW_ACTION);

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

  PermissionTestUtils.add(uriString, "cookie", Services.perms.UNKNOWN_ACTION);
});
