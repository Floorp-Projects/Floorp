"use strict";

const {PlacesTestUtils} =
  Cu.import("resource://testing-common/PlacesTestUtils.jsm", {});

let notificationURL = "http://example.org/browser/browser/base/content/test/alerts/file_dom_notifications.html";
let oldShowFavicons;

add_task(function* test_notificationClose() {
  let pm = Services.perms;
  let notificationURI = makeURI(notificationURL);
  pm.add(notificationURI, "desktop-notification", pm.ALLOW_ACTION);

  oldShowFavicons = Services.prefs.getBoolPref("alerts.showFavicons");
  Services.prefs.setBoolPref("alerts.showFavicons", true);

  yield PlacesTestUtils.addVisits(notificationURI);
  let faviconURI = yield new Promise(resolve => {
    let uri =
      makeURI("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVQI12P4//8/AAX+Av7czFnnAAAAAElFTkSuQmCC");
    PlacesUtils.favicons.setAndFetchFaviconForPage(notificationURI, uri,
      true, PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
      (uriResult) => resolve(uriResult),
      Services.scriptSecurityManager.getSystemPrincipal());
  });

  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: notificationURL
  }, function* dummyTabTask(aBrowser) {
    yield openNotification(aBrowser, "showNotification2");

    info("Notification alert showing");

    let alertWindow = Services.wm.getMostRecentWindow("alert:alert");
    if (!alertWindow) {
      ok(true, "Notifications don't use XUL windows on all platforms.");
      yield closeNotification(aBrowser);
      return;
    }

    let alertTitleLabel = alertWindow.document.getElementById("alertTitleLabel");
    is(alertTitleLabel.value, "Test title", "Title text of notification should be present");
    let alertTextLabel = alertWindow.document.getElementById("alertTextLabel");
    is(alertTextLabel.textContent, "Test body 2", "Body text of notification should be present");
    let alertIcon = alertWindow.document.getElementById("alertIcon");
    is(alertIcon.src, faviconURI.spec, "Icon of notification should be present");

    let alertCloseButton = alertWindow.document.querySelector(".alertCloseButton");
    is(alertCloseButton.localName, "toolbarbutton", "close button found");
    let promiseBeforeUnloadEvent =
      BrowserTestUtils.waitForEvent(alertWindow, "beforeunload");
    let closedTime = alertWindow.Date.now();
    alertCloseButton.click();
    info("Clicked on close button");
    yield promiseBeforeUnloadEvent;

    ok(true, "Alert should close when the close button is clicked");
    let currentTime = alertWindow.Date.now();
    // The notification will self-close at 12 seconds, so this checks
    // that the notification closed before the timeout.
    ok(currentTime - closedTime < 5000,
       "Close requested at " + closedTime + ", actually closed at " + currentTime);
  });
});

add_task(function* cleanup() {
  Services.perms.remove(makeURI(notificationURL), "desktop-notification");
  if (typeof oldShowFavicons == "boolean") {
    Services.prefs.setBoolPref("alerts.showFavicons", oldShowFavicons);
  }
});
