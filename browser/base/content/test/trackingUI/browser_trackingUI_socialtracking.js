/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TRACKING_PAGE =
  "http://example.org/browser/browser/base/content/test/trackingUI/trackingPage.html";

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.socialtracking.block_cookies.enabled", true],
      ["privacy.trackingprotection.socialtracking.enabled", true],
      [
        "urlclassifier.features.socialtracking.blacklistHosts",
        "socialtracking.example.com",
      ],
      [
        "urlclassifier.features.socialtracking.annotate.blacklistHosts",
        "socialtracking.example.com",
      ],
      ["privacy.trackingprotection.enabled", false],
      ["privacy.trackingprotection.annotate_channels", false],
      ["privacy.trackingprotection.cryptomining.enabled", false],
      ["urlclassifier.features.cryptomining.annotate.blacklistHosts", ""],
      ["urlclassifier.features.cryptomining.annotate.blacklistTables", ""],
      ["privacy.trackingprotection.fingerprinting.enabled", false],
      ["urlclassifier.features.fingerprinting.annotate.blacklistHosts", ""],
      ["urlclassifier.features.fingerprinting.annotate.blacklistTables", ""],
    ],
  });
});

async function testPopup(hasPopup, buttonToClick) {
  let numPageLoaded = gProtectionsHandler._socialTrackingSessionPageLoad;
  let numPopupShown = SpecialPowers.getIntPref(
    "privacy.socialtracking.notification.counter",
    0
  );

  let promise = BrowserTestUtils.openNewForegroundTab({
    url: TRACKING_PAGE,
    gBrowser,
  });
  let [tab] = await Promise.all([promise, waitForContentBlockingEvent()]);

  ok(
    !gProtectionsHandler._protectionsPopup.hasAttribute("detected"),
    "socialtrackings are not detected"
  );
  ok(
    !BrowserTestUtils.is_visible(
      gProtectionsHandler._socialblockPopupNotification
    ),
    "popup is not visible"
  );

  promise = waitForContentBlockingEvent();
  let popup = document.getElementById("notification-popup");
  let popupShown = hasPopup
    ? BrowserTestUtils.waitForEvent(popup, "PanelUpdated")
    : null;

  await ContentTask.spawn(tab.linkedBrowser, {}, function() {
    content.postMessage("socialtracking", "*");
  });

  await Promise.all([promise, popupShown]);

  ok(
    gProtectionsHandler._protectionsPopup.hasAttribute("detected"),
    "trackers are detected"
  );
  is(
    BrowserTestUtils.is_visible(
      gProtectionsHandler._socialblockPopupNotification
    ),
    hasPopup,
    hasPopup ? "popup is visible" : "popup is not visible"
  );

  is(
    gProtectionsHandler._socialTrackingSessionPageLoad,
    numPageLoaded + 1,
    "page loaded once"
  );

  if (hasPopup) {
    // click on the button of the popup notification
    if (typeof buttonToClick === "string") {
      is(
        SpecialPowers.getBoolPref(
          "privacy.socialtracking.notification.enabled",
          false
        ),
        true,
        "notification still enabled"
      );

      let notification = PopupNotifications.panel.firstElementChild;
      EventUtils.synthesizeMouseAtCenter(notification[buttonToClick], {});

      is(
        SpecialPowers.getBoolPref(
          "privacy.socialtracking.notification.enabled",
          true
        ),
        false,
        "notification disabled now"
      );
    }

    let lastSeen = SpecialPowers.getIntPref(
      "privacy.socialtracking.notification.lastSeen",
      0
    );
    ok(lastSeen, "last seen timestamp updated");
    is(
      SpecialPowers.getIntPref(
        "privacy.socialtracking.notification.counter",
        0
      ),
      numPopupShown + 1,
      "notification counter increased"
    );
  }

  BrowserTestUtils.removeTab(tab);
}

async function testIdentityState(hasException) {
  let promise = BrowserTestUtils.openNewForegroundTab({
    url: TRACKING_PAGE,
    gBrowser,
  });
  let [tab] = await Promise.all([promise, waitForContentBlockingEvent()]);

  if (hasException) {
    let loaded = BrowserTestUtils.browserLoaded(
      tab.linkedBrowser,
      false,
      TRACKING_PAGE
    );
    gProtectionsHandler.disableForCurrentPage();
    await loaded;
  }

  ok(
    !gProtectionsHandler._protectionsPopup.hasAttribute("detected"),
    "socialtrackings are not detected"
  );

  ok(
    BrowserTestUtils.is_visible(gProtectionsHandler.iconBox),
    "icon box is visible regardless the exception"
  );

  promise = waitForContentBlockingEvent();

  await ContentTask.spawn(tab.linkedBrowser, {}, function() {
    content.postMessage("socialtracking", "*");
  });

  await promise;

  ok(
    gProtectionsHandler._protectionsPopup.hasAttribute("detected"),
    "trackers are detected"
  );
  ok(
    BrowserTestUtils.is_visible(gProtectionsHandler.iconBox),
    "icon box is visible"
  );
  is(
    gProtectionsHandler.iconBox.hasAttribute("hasException"),
    hasException,
    "Shows an exception when appropriate"
  );

  if (hasException) {
    let loaded = BrowserTestUtils.browserLoaded(
      tab.linkedBrowser,
      false,
      TRACKING_PAGE
    );
    gProtectionsHandler.enableForCurrentPage();
    await loaded;
  }

  BrowserTestUtils.removeTab(tab);
}

async function testSubview(hasException) {
  let promise = BrowserTestUtils.openNewForegroundTab({
    url: TRACKING_PAGE,
    gBrowser,
  });
  let [tab] = await Promise.all([promise, waitForContentBlockingEvent()]);

  if (hasException) {
    let loaded = BrowserTestUtils.browserLoaded(
      tab.linkedBrowser,
      false,
      TRACKING_PAGE
    );
    gProtectionsHandler.disableForCurrentPage();
    await loaded;
  }

  promise = waitForContentBlockingEvent();
  await ContentTask.spawn(tab.linkedBrowser, {}, function() {
    content.postMessage("socialtracking", "*");
  });
  await promise;

  await openProtectionsPopup();

  let categoryItem = document.getElementById(
    "protections-popup-category-socialblock"
  );
  ok(BrowserTestUtils.is_visible(categoryItem), "TP category item is visible");
  let subview = document.getElementById("protections-popup-socialblockView");
  let viewShown = BrowserTestUtils.waitForEvent(subview, "ViewShown");
  categoryItem.click();
  await viewShown;

  let listItems = subview.querySelectorAll(".protections-popup-list-item");
  is(listItems.length, 1, "We have 1 item in the list");
  let listItem = listItems[0];
  ok(BrowserTestUtils.is_visible(listItem), "List item is visible");
  is(
    listItem.querySelector("label").value,
    "socialtracking.example.com",
    "Has the correct host"
  );

  let mainView = document.getElementById("protections-popup-mainView");
  viewShown = BrowserTestUtils.waitForEvent(mainView, "ViewShown");
  let backButton = subview.querySelector(".subviewbutton-back");
  backButton.click();
  await viewShown;

  ok(true, "Main view was shown");

  if (hasException) {
    let loaded = BrowserTestUtils.browserLoaded(
      tab.linkedBrowser,
      false,
      TRACKING_PAGE
    );
    gProtectionsHandler.enableForCurrentPage();
    await loaded;
  }

  BrowserTestUtils.removeTab(tab);
}

add_task(async function testSocialTrackingPopups() {
  let configs = [
    {
      description: "always-on notification",
      results: [true, true],
      prefs: [
        ["privacy.socialtracking.notification.enabled", true],
        ["privacy.socialtracking.notification.session.pageload.min", -1],
        ["privacy.socialtracking.notification.lastSeen", 0],
        ["privacy.socialtracking.notification.period.min", 0],
        ["privacy.socialtracking.notification.counter", 0],
        ["privacy.socialtracking.notification.max", 999],
      ],
    },
    {
      description: "always-off notification",
      results: [false, false],
      prefs: [
        ["privacy.socialtracking.notification.enabled", false],
        ["privacy.socialtracking.notification.session.pageload.min", -1],
        ["privacy.socialtracking.notification.lastSeen", 0],
        ["privacy.socialtracking.notification.period.min", 0],
        ["privacy.socialtracking.notification.counter", 0],
        ["privacy.socialtracking.notification.max", 999],
      ],
    },

    {
      description: "set minimum page loaded limitation",
      results: [false, false, true],
      prefs: [
        ["privacy.socialtracking.notification.enabled", true],
        ["privacy.socialtracking.notification.session.pageload.min", 2],
        ["privacy.socialtracking.notification.lastSeen", 0],
        ["privacy.socialtracking.notification.period.min", 0],
        ["privacy.socialtracking.notification.counter", 0],
        ["privacy.socialtracking.notification.max", 999],
      ],
    },
    {
      description: "set minimum period between popups",
      results: [true, false],
      prefs: [
        ["privacy.socialtracking.notification.enabled", true],
        ["privacy.socialtracking.notification.session.pageload.min", -1],
        ["privacy.socialtracking.notification.lastSeen", 0],
        ["privacy.socialtracking.notification.period.min", 3600000], // 1H
        ["privacy.socialtracking.notification.counter", 0],
        ["privacy.socialtracking.notification.max", 999],
      ],
    },
    {
      description: "define maximum number of notifications",
      results: [true, true, false],
      prefs: [
        ["privacy.socialtracking.notification.enabled", true],
        ["privacy.socialtracking.notification.session.pageload.min", -1],
        ["privacy.socialtracking.notification.lastSeen", 0],
        ["privacy.socialtracking.notification.period.min", 0],
        ["privacy.socialtracking.notification.counter", 3],
        ["privacy.socialtracking.notification.max", 5],
      ],
    },
    {
      description: "disable notification by clicking primaryButton",
      results: [true, false],
      button: "button",
      prefs: [
        ["privacy.socialtracking.notification.enabled", true],
        ["privacy.socialtracking.notification.session.pageload.min", -1],
        ["privacy.socialtracking.notification.lastSeen", 0],
        ["privacy.socialtracking.notification.period.min", 0],
        ["privacy.socialtracking.notification.counter", 0],
        ["privacy.socialtracking.notification.max", 999],
      ],
    },
    {
      description: "disable notification by clicking secondaryButton",
      results: [true, false],
      button: "secondaryButton",
      prefs: [
        ["privacy.socialtracking.notification.enabled", true],
        ["privacy.socialtracking.notification.session.pageload.min", -1],
        ["privacy.socialtracking.notification.lastSeen", 0],
        ["privacy.socialtracking.notification.period.min", 0],
        ["privacy.socialtracking.notification.counter", 0],
        ["privacy.socialtracking.notification.max", 999],
      ],
    },
  ];

  for (let config of configs) {
    ok(config.description, config.description);
    SpecialPowers.pushPrefEnv({
      set: config.prefs,
    });
    for (let result of config.results) {
      await testPopup(result, config.button);
    }
    SpecialPowers.popPrefEnv();
  }
});

add_task(async function testIdentityUI() {
  requestLongerTimeout(2);

  await testIdentityState(false);
  await testIdentityState(true);

  await testSubview(false);
  await testSubview(true);
});
