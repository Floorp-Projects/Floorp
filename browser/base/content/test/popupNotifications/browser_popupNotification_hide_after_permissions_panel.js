/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test_hide_popup_with_permissions_panel_showing() {
  await BrowserTestUtils.withNewTab(
    "https://test1.example.com/",
    async function(browser) {
      // Request location permissions and wait for that prompt to appear.
      let popupShownPromise = waitForNotificationPanel();
      await SpecialPowers.spawn(browser, [], async function() {
        content.navigator.geolocation.getCurrentPosition(() => {});
      });
      await popupShownPromise;

      // Grant this site an unrelated permission, so that the icon for the
      // permissions settings doorhanger appears.
      let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
        "https://test1.example.com"
      );
      SitePermissions.setForPrincipal(
        principal,
        "camera",
        SitePermissions.ALLOW
      );

      // Click on the icon for the permissions settings doorhanger to show it.
      popupShownPromise = BrowserTestUtils.waitForEvent(
        window,
        "popupshown",
        true,
        event => event.target == gPermissionPanel._permissionPopup
      );
      EventUtils.synthesizeMouseAtCenter(
        document.getElementById("identity-permission-box"),
        {}
      );
      await popupShownPromise;

      // Make sure the location permission prompt closed.
      Assert.ok(!PopupNotifications.isPanelOpen, "Geolocation popup is hidden");

      // Close the permissions settings doorhanger.
      let popupHidden = BrowserTestUtils.waitForEvent(
        gPermissionPanel._permissionPopup,
        "popuphidden"
      );
      gPermissionPanel._permissionPopup.hidePopup();
      await popupHidden;

      // Make sure the location permission prompt came back.
      Assert.ok(PopupNotifications.isPanelOpen, "Geolocation popup is showing");
    }
  );
});
