/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test() {
  let uriString = "http://example.com/";
  let cookieBehavior = "network.cookie.cookieBehavior";
  let uriObj = Services.io.newURI(uriString);
  let cp = Components.classes["@mozilla.org/cookie/permission;1"]
                     .getService(Components.interfaces.nsICookiePermission);

  await SpecialPowers.pushPrefEnv({ set: [[ cookieBehavior, 2 ]] });
  cp.setAccess(uriObj, cp.ACCESS_ALLOW);

  await BrowserTestUtils.withNewTab({ gBrowser, url: uriString }, async function(browser) {
    await ContentTask.spawn(browser, null, function() {
      is(content.navigator.cookieEnabled, true,
         "navigator.cookieEnabled should be true");
    });
  });

  cp.setAccess(uriObj, cp.ACCESS_DEFAULT);
});
