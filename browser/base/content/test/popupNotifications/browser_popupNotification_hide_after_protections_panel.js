/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test_hide_popup_with_protections_panel_showing() {
  await BrowserTestUtils.withNewTab(
    "https://test1.example.com/",
    async function (browser) {
      // Request location permissions and wait for that prompt to appear.
      let popupShownPromise = waitForNotificationPanel();
      await SpecialPowers.spawn(browser, [], async function () {
        content.navigator.geolocation.getCurrentPosition(() => {});
      });
      await popupShownPromise;

      // Click on the icon for the protections panel, to show the panel.
      popupShownPromise = BrowserTestUtils.waitForEvent(
        window,
        "popupshown",
        true,
        event => event.target == gProtectionsHandler._protectionsPopup
      );
      EventUtils.synthesizeMouseAtCenter(
        document.getElementById("tracking-protection-icon-container"),
        {}
      );
      await popupShownPromise;

      // Make sure the location permission prompt closed.
      Assert.ok(!PopupNotifications.isPanelOpen, "Geolocation popup is hidden");

      // Close the protections panel.
      let popupHidden = BrowserTestUtils.waitForEvent(
        gProtectionsHandler._protectionsPopup,
        "popuphidden"
      );
      gProtectionsHandler._protectionsPopup.hidePopup();
      await popupHidden;

      // Make sure the location permission prompt came back.
      Assert.ok(PopupNotifications.isPanelOpen, "Geolocation popup is showing");
    }
  );
});
