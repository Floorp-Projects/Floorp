/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  let uriString = "http://example.com/";
  let cookieBehavior = "network.cookie.cookieBehavior";
  let uriObj = Services.io.newURI(uriString)
  let cp = Components.classes["@mozilla.org/cookie/permission;1"]
                     .getService(Components.interfaces.nsICookiePermission);

  Services.prefs.setIntPref(cookieBehavior, 2);

  cp.setAccess(uriObj, cp.ACCESS_ALLOW);
  gBrowser.selectedTab = gBrowser.addTab(uriString);
  waitForExplicitFinish();
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(onTabLoaded);

  function onTabLoaded() {
    is(gBrowser.selectedBrowser.contentWindow.navigator.cookieEnabled, true,
       "navigator.cookieEnabled should be true");
    // Clean up
    gBrowser.removeTab(gBrowser.selectedTab);
    Services.prefs.setIntPref(cookieBehavior, 0);
    cp.setAccess(uriObj, cp.ACCESS_DEFAULT);
    finish();
  }
}
