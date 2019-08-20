/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

const TRACKING_PAGE =
  "http://example.org/browser/browser/base/content/test/trackingUI/trackingPage.html";

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.trackingprotection.socialtracking.enabled", true],
      ["privacy.trackingprotection.socialtracking.annotate.enabled", true],
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
      ["privacy.trackingprotection.cryptomining.annotate.enabled", false],
      ["privacy.trackingprotection.fingerprinting.enabled", false],
      ["privacy.trackingprotection.fingerprinting.annotate.enabled", false],
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

    let lastShown = SpecialPowers.getCharPref(
      "privacy.socialtracking.notification.lastShown",
      "0"
    );
    ok(lastShown !== "0", "last shown timestamp updated");
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

add_task(async function testSocialTrackingPopups() {
  let configs = [
    {
      description: "always-on notification",
      results: [true, true],
      prefs: [
        ["privacy.socialtracking.notification.enabled", true],
        ["privacy.socialtracking.notification.session.pageload.min", -1],
        ["privacy.socialtracking.notification.lastShown", "0"],
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
        ["privacy.socialtracking.notification.lastShown", "0"],
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
        ["privacy.socialtracking.notification.lastShown", "0"],
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
        ["privacy.socialtracking.notification.lastShown", "0"],
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
        ["privacy.socialtracking.notification.lastShown", "0"],
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
        ["privacy.socialtracking.notification.lastShown", "0"],
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
        ["privacy.socialtracking.notification.lastShown", "0"],
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
