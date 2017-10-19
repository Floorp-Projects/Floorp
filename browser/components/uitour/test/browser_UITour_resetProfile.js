"use strict";

var gTestTab;
var gContentAPI;
var gContentWindow;

add_task(setup_UITourTest);

// Test that a reset profile dialog appears when "resetFirefox" event is triggered
add_UITour_task(async function test_resetFirefox() {
  let canReset = await getConfigurationPromise("canReset");
  ok(!canReset, "Shouldn't be able to reset from mochitest's temporary profile.");
  let dialogPromise = new Promise((resolve) => {
    Services.ww.registerNotification(function onOpen(subj, topic, data) {
      if (topic == "domwindowopened" && subj instanceof Ci.nsIDOMWindow) {
        subj.addEventListener("load", function() {
          if (subj.document.documentURI ==
              "chrome://global/content/resetProfile.xul") {
            Services.ww.unregisterNotification(onOpen);
            ok(true, "Observed search manager window open");
            is(subj.opener, window,
               "Reset Firefox event opened a reset profile window.");
            subj.close();
            resolve();
          }
        }, {once: true});
      }
    });
  });

  // make reset possible.
  let profileService = Cc["@mozilla.org/toolkit/profile-service;1"].
                       getService(Ci.nsIToolkitProfileService);
  let currentProfileDir = Services.dirsvc.get("ProfD", Ci.nsIFile);
  let profileName = "mochitest-test-profile-temp-" + Date.now();
  let tempProfile = profileService.createProfile(currentProfileDir, profileName);
  canReset = await getConfigurationPromise("canReset");
  ok(canReset, "Should be able to reset from mochitest's temporary profile once it's in the profile manager.");
  await gContentAPI.resetFirefox();
  await dialogPromise;
  tempProfile.remove(false);
  canReset = await getConfigurationPromise("canReset");
  ok(!canReset, "Shouldn't be able to reset from mochitest's temporary profile once removed from the profile manager.");
});
