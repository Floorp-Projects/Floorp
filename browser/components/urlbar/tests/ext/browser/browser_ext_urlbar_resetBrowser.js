/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* global browser */

// This tests the browser.experiments.urlbar.resetBrowser
// WebExtension Experiment API.

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  ResetProfile: "resource://gre/modules/ResetProfile.jsm",
});

// Test that a reset profile dialog appears when
// browser.experiments.urlbar.resetBrowser is called.
add_task(async function test_resetFirefox() {
  Assert.ok(
    !ResetProfile.resetSupported(),
    "Shouldn't be able to reset from mochitest's temporary profile."
  );

  // Make reset possible.
  let profileService = Cc["@mozilla.org/toolkit/profile-service;1"].getService(
    Ci.nsIToolkitProfileService
  );
  let currentProfileDir = Services.dirsvc.get("ProfD", Ci.nsIFile);
  let profileName = "mochitest-test-profile-temp-" + Date.now();
  let tempProfile = profileService.createProfile(
    currentProfileDir,
    profileName
  );
  Assert.ok(
    ResetProfile.resetSupported(),
    "Should be able to reset from mochitest's temporary profile once it's in the profile manager."
  );
  await checkExtension();
  tempProfile.remove(false);
  Assert.ok(
    !ResetProfile.resetSupported(),
    "Shouldn't be able to reset from mochitest's temporary profile once removed from the profile manager."
  );
});

async function checkExtension() {
  let dialogPromise = new Promise(resolve => {
    Services.ww.registerNotification(function onOpen(subj, topic, data) {
      if (topic == "domwindowopened" && subj instanceof Ci.nsIDOMWindow) {
        subj.addEventListener(
          "load",
          function() {
            if (
              subj.document.documentURI ==
              "chrome://global/content/resetProfile.xhtml"
            ) {
              Services.ww.unregisterNotification(onOpen);
              Assert.ok(true, "Observed search manager window open");
              is(
                subj.opener,
                window,
                "Reset Firefox event opened a reset profile window."
              );
              subj.close();
              resolve();
            }
          },
          { once: true }
        );
      }
    });
  });
  let ext = await loadExtension(async () => {
    await browser.experiments.urlbar.resetBrowser();
  });
  await dialogPromise;
  Assert.ok(true, "Reset browser dialog is showing.");
  await ext.unload();
}
