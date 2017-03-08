/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
"use strict";

const TEST_URI = "http://example.com/" +
                 "browser/dom/tests/browser/position.html";

add_task(function* testDismissHanger() {
  info("Check that location is not shared when dismissing the geolocation hanger");

  let promisePanelShown = BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown", true);
  yield BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URI);
  yield promisePanelShown;

  // click outside the Geolocation hanger to dismiss it
  window.document.getElementById("nav-bar").click();
  info("Clicked outside the Geolocation panel to dismiss it");

  let hasLocation = yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function*() {
    return content.document.body.innerHTML.includes("location...");
  });

  ok(hasLocation, "Location is not shared");

  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
