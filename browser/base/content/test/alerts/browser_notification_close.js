"use strict";

const { PlacesTestUtils } = ChromeUtils.import(
  "resource://testing-common/PlacesTestUtils.jsm"
);

const { PermissionTestUtils } = ChromeUtils.import(
  "resource://testing-common/PermissionTestUtils.jsm"
);

let notificationURL =
  "http://example.org/browser/browser/base/content/test/alerts/file_dom_notifications.html";
let oldShowFavicons;

add_task(async function test_notificationClose() {
  let notificationURI = makeURI(notificationURL);
  await addNotificationPermission(notificationURL);

  oldShowFavicons = Services.prefs.getBoolPref("alerts.showFavicons");
  Services.prefs.setBoolPref("alerts.showFavicons", true);

  await PlacesTestUtils.addVisits(notificationURI);
  let faviconURI = await new Promise(resolve => {
    let uri = makeURI(
      "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVQI12P4//8/AAX+Av7czFnnAAAAAElFTkSuQmCC"
    );
    PlacesUtils.favicons.setAndFetchFaviconForPage(
      notificationURI,
      uri,
      true,
      PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
      uriResult => resolve(uriResult),
      Services.scriptSecurityManager.getSystemPrincipal()
    );
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: notificationURL,
    },
    async function dummyTabTask(aBrowser) {
      await openNotification(aBrowser, "showNotification2");

      info("Notification alert showing");

      let alertWindow = Services.wm.getMostRecentWindow("alert:alert");
      if (!alertWindow) {
        ok(true, "Notifications don't use XUL windows on all platforms.");
        await closeNotification(aBrowser);
        return;
      }

      let alertTitleLabel = alertWindow.document.getElementById(
        "alertTitleLabel"
      );
      is(
        alertTitleLabel.value,
        "Test title",
        "Title text of notification should be present"
      );
      let alertTextLabel = alertWindow.document.getElementById(
        "alertTextLabel"
      );
      is(
        alertTextLabel.textContent,
        "Test body 2",
        "Body text of notification should be present"
      );
      let alertIcon = alertWindow.document.getElementById("alertIcon");
      is(
        alertIcon.src,
        faviconURI.spec,
        "Icon of notification should be present"
      );

      let alertCloseButton = alertWindow.document.querySelector(".close-icon");
      is(alertCloseButton.localName, "toolbarbutton", "close button found");
      let promiseBeforeUnloadEvent = BrowserTestUtils.waitForEvent(
        alertWindow,
        "beforeunload"
      );
      let closedTime = alertWindow.Date.now();
      alertCloseButton.click();
      info("Clicked on close button");
      await promiseBeforeUnloadEvent;

      ok(true, "Alert should close when the close button is clicked");
      let currentTime = alertWindow.Date.now();
      // The notification will self-close at 12 seconds, so this checks
      // that the notification closed before the timeout.
      ok(
        currentTime - closedTime < 5000,
        "Close requested at " +
          closedTime +
          ", actually closed at " +
          currentTime
      );
    }
  );
});

add_task(async function cleanup() {
  PermissionTestUtils.remove(notificationURL, "desktop-notification");
  if (typeof oldShowFavicons == "boolean") {
    Services.prefs.setBoolPref("alerts.showFavicons", oldShowFavicons);
  }
});
