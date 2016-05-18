"use strict";

var gTestTab;
var gContentAPI;
var gContentWindow;

add_task(setup_UITourTest);

// Test that a reset profile dialog appears when "resetFirefox" event is triggered
add_UITour_task(function* test_resetFirefox() {
  let canReset = yield getConfigurationPromise("canReset");
  ok(!canReset, "Shouldn't be able to reset from mochitest's temporary profile.");
  let dialogPromise = new Promise((resolve) => {
    let winWatcher = Cc["@mozilla.org/embedcomp/window-watcher;1"].
                     getService(Ci.nsIWindowWatcher);
    winWatcher.registerNotification(function onOpen(subj, topic, data) {
      if (topic == "domwindowopened" && subj instanceof Ci.nsIDOMWindow) {
        subj.addEventListener("load", function onLoad() {
          subj.removeEventListener("load", onLoad);
          if (subj.document.documentURI ==
              "chrome://global/content/resetProfile.xul") {
            winWatcher.unregisterNotification(onOpen);
            ok(true, "Observed search manager window open");
            is(subj.opener, window,
               "Reset Firefox event opened a reset profile window.");
            subj.close();
            resolve();
          }
        });
      }
    });
  });

  // make reset possible.
  let profileService = Cc["@mozilla.org/toolkit/profile-service;1"].
                       getService(Ci.nsIToolkitProfileService);
  let currentProfileDir = Services.dirsvc.get("ProfD", Ci.nsIFile);
  let profileName = "mochitest-test-profile-temp-" + Date.now();
  let tempProfile = profileService.createProfile(currentProfileDir, profileName);
  canReset = yield getConfigurationPromise("canReset");
  ok(canReset, "Should be able to reset from mochitest's temporary profile once it's in the profile manager.");
  yield gContentAPI.resetFirefox();
  yield dialogPromise;
  tempProfile.remove(false);
  canReset = yield getConfigurationPromise("canReset");
  ok(!canReset, "Shouldn't be able to reset from mochitest's temporary profile once removed from the profile manager.");
});

