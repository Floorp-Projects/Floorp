/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test_displayURI_geo() {
  await BrowserTestUtils.withNewTab(
    "https://test1.example.com/",
    async function(browser) {
      let popupShownPromise = waitForNotificationPanel();
      await SpecialPowers.spawn(browser, [], async function() {
        content.navigator.geolocation.getCurrentPosition(() => {});
      });
      await popupShownPromise;

      popupShownPromise = BrowserTestUtils.waitForEvent(
        window,
        "popupshown",
        true,
        event => event.target == gIdentityHandler._identityPopup
      );
      EventUtils.synthesizeMouseAtCenter(gIdentityHandler._identityIconBox, {});
      await popupShownPromise;

      Assert.ok(!PopupNotifications.isPanelOpen, "Geolocation popup is hidden");

      let popupHidden = BrowserTestUtils.waitForEvent(
        gIdentityHandler._identityPopup,
        "popuphidden"
      );
      gIdentityHandler._identityPopup.hidePopup();
      await popupHidden;

      Assert.ok(PopupNotifications.isPanelOpen, "Geolocation popup is showing");
    }
  );
});
