"use strict";

var gTestTab;
var gContentAPI;
var gContentWindow;

const { UrlClassifierTestUtils } = ChromeUtils.import("resource://testing-common/UrlClassifierTestUtils.jsm", {});

const TP_ENABLED_PREF = "privacy.trackingprotection.enabled";

add_task(setup_UITourTest);

add_task(async function test_setup() {
  Services.prefs.setBoolPref("privacy.trackingprotection.enabled", true);
  await UrlClassifierTestUtils.addTestTrackers();

  registerCleanupFunction(function() {
    UrlClassifierTestUtils.cleanupTestTrackers();
    Services.prefs.clearUserPref("privacy.trackingprotection.enabled");
  });
});

add_UITour_task(async function test_unblock_target() {
  await checkToggleTarget("controlCenter-trackingUnblock");
});

add_UITour_task(function setup_block_target() {
  // Preparation for test_block_target. These are separate since the reload
  // interferes with UITour as it does a teardown. All we really care about
  // is the permission manager entry but UITour tests shouldn't rely on that
  // implementation detail.
  TrackingProtection.disableForCurrentPage();
});

add_UITour_task(async function test_block_target() {
  await checkToggleTarget("controlCenter-trackingBlock");
  TrackingProtection.enableForCurrentPage();
});


async function checkToggleTarget(targetID) {
  let popup = document.getElementById("UITourTooltip");

  let trackerOpened = new Promise(function(resolve, reject) {
    Services.obs.addObserver(function onopen(subject) {
      let asciiSpec = subject.QueryInterface(Ci.nsIHttpChannel).URI.asciiSpec;
      if (asciiSpec === "https://tracking.example.com/") {
        Services.obs.removeObserver(onopen, "http-on-opening-request");
        resolve();
      }
    }, "http-on-opening-request");
  });

  await ContentTask.spawn(gBrowser.selectedBrowser, {}, function() {
    let doc = content.document;
    let iframe = doc.createElement("iframe");
    iframe.setAttribute("id", "tracking-element");
    iframe.setAttribute("src", "https://tracking.example.com/");
    doc.body.insertBefore(iframe, doc.body.firstChild);
  });

  await trackerOpened;

  let testTargetAvailability = async function(expectedAvailable) {
    let data = await getConfigurationPromise("availableTargets");
    let available = (data.targets.includes(targetID));
    is(available, expectedAvailable, "Target has expected availability.");
  };
  await testTargetAvailability(false);
  await showMenuPromise("controlCenter");
  await testTargetAvailability(true);

  await showInfoPromise(targetID, "This is " + targetID,
                        "My arrow should be on the side");
  is(popup.alignmentPosition, "end_before",
     "Check " + targetID + " position");

  let hideMenuPromise =
        promisePanelElementHidden(window, gIdentityHandler._identityPopup);
  await gContentAPI.hideMenu("controlCenter");
  await hideMenuPromise;

  ok(!is_visible(popup), "The tooltip should now be hidden.");
  await testTargetAvailability(false);

  await ContentTask.spawn(gBrowser.selectedBrowser, {}, function() {
    content.document.getElementById("tracking-element").remove();
  });
}
