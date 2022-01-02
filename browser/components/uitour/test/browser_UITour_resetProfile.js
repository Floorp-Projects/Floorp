"use strict";

var gTestTab;
var gContentAPI;

add_task(setup_UITourTest);

// Test that a reset profile dialog appears when "resetFirefox" event is triggered
add_UITour_task(async function test_resetFirefox() {
  let canReset = await getConfigurationPromise("canReset");
  ok(
    !canReset,
    "Shouldn't be able to reset from mochitest's temporary profile."
  );
  let dialogPromise = BrowserTestUtils.promiseAlertDialog(
    "cancel",
    "chrome://global/content/resetProfile.xhtml",
    {
      isSubDialog: true,
    }
  );

  // make reset possible.
  let profileService = Cc["@mozilla.org/toolkit/profile-service;1"].getService(
    Ci.nsIToolkitProfileService
  );
  let currentProfileDir = Services.dirsvc.get("ProfD", Ci.nsIFile);
  let profileName = "mochitest-test-profile-temp-" + Date.now();
  let tempProfile = profileService.createProfile(
    currentProfileDir,
    profileName
  );
  canReset = await getConfigurationPromise("canReset");
  ok(
    canReset,
    "Should be able to reset from mochitest's temporary profile once it's in the profile manager."
  );
  await gContentAPI.resetFirefox();
  await dialogPromise;
  tempProfile.remove(false);
  canReset = await getConfigurationPromise("canReset");
  ok(
    !canReset,
    "Shouldn't be able to reset from mochitest's temporary profile once removed from the profile manager."
  );
});
