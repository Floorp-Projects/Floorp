/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gTestTab;
let gContentAPI;
let gContentWindow;

const { UrlClassifierTestUtils } = Cu.import("resource://testing-common/UrlClassifierTestUtils.jsm", {});

const TP_ENABLED_PREF = "privacy.trackingprotection.enabled";

function test() {
  UITourTest();
}

let tests = [
  taskify(function* test_setup() {
    Services.prefs.setBoolPref("privacy.trackingprotection.enabled", true);
    yield UrlClassifierTestUtils.addTestTrackers();

    registerCleanupFunction(function() {
      UrlClassifierTestUtils.cleanupTestTrackers();
      Services.prefs.clearUserPref("privacy.trackingprotection.enabled");
    });
  }),

  taskify(function* test_unblock_target() {
    yield* checkToggleTarget("controlCenter-trackingUnblock");
  }),

  taskify(function* setup_block_target() {
    // Preparation for test_block_target. These are separate since the reload
    // interferes with UITour as it does a teardown. All we really care about
    // is the permission manager entry but UITour tests shouldn't rely on that
    // implementation detail.
    TrackingProtection.disableForCurrentPage();
  }),

  taskify(function* test_block_target() {
    yield* checkToggleTarget("controlCenter-trackingBlock");
    TrackingProtection.enableForCurrentPage();
  }),
];


function* checkToggleTarget(targetID) {
  let popup = document.getElementById("UITourTooltip");

  yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function () {
    let doc = content.document;
    let iframe = doc.createElement("iframe");
    iframe.setAttribute("id", "tracking-element");
    iframe.setAttribute("src", "https://tracking.example.com/");
    doc.body.insertBefore(iframe, doc.body.firstChild);
  });

  let testTargetAvailability = function* (expectedAvailable) {
    let data = yield getConfigurationPromise("availableTargets");
    let available = (data.targets.indexOf(targetID) != -1);
    is(available, expectedAvailable, "Target has expected availability.");
  };
  yield testTargetAvailability(false);
  yield showMenuPromise("controlCenter");
  yield testTargetAvailability(true);

  yield showInfoPromise(targetID, "This is " + targetID,
                        "My arrow should be on the side");
  is(popup.popupBoxObject.alignmentPosition, "end_before",
     "Check " + targetID + " position");

  let hideMenuPromise =
        promisePanelElementHidden(window, gIdentityHandler._identityPopup);
  gContentAPI.hideMenu("controlCenter");
  yield hideMenuPromise;

  ok(!is_visible(popup), "The tooltip should now be hidden.");
  yield testTargetAvailability(false);

  yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function () {
    content.document.getElementById("tracking-element").remove();
  });
}
