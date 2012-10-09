/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that users are prevented from toggling the private
// browsing mode too quickly, hence be proctected from symptoms in bug 526194.

function test() {
  // initialization
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
  let pbCmd = document.getElementById("Tools:PrivateBrowsing");
  waitForExplicitFinish();

  let pass = 1;
  function observer(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "private-browsing-transition-complete":
        if (pass++ == 1) {
          setTimeout(function () {
            ok(!pbCmd.hasAttribute("disabled"),
               "The private browsing command should be re-enabled after entering the private browsing mode");

            pb.privateBrowsingEnabled = false;
          }, 100);
        }
        else {
          setTimeout(function () {
            ok(!pbCmd.hasAttribute("disabled"),
               "The private browsing command should be re-enabled after exiting the private browsing mode");

            finish();
          }, 100);
        }
        break;
    }
    Services.obs.removeObserver(observer, "private-browsing-transition-complete");
  }
  let originalOnEnter = gPrivateBrowsingUI.onEnterPrivateBrowsing;
  gPrivateBrowsingUI.onEnterPrivateBrowsing = function() {
    originalOnEnter.apply(gPrivateBrowsingUI, arguments);
    ok(pbCmd.hasAttribute("disabled"),
       "The private browsing command should be disabled right after entering the private browsing mode");
    Services.obs.addObserver(observer, "private-browsing-transition-complete", false);
  };
  let originalOnExit = gPrivateBrowsingUI.onExitPrivateBrowsing;
  gPrivateBrowsingUI.onExitPrivateBrowsing = function() {
    originalOnExit.apply(gPrivateBrowsingUI, arguments);
    ok(pbCmd.hasAttribute("disabled"),
       "The private browsing command should be disabled right after exiting the private browsing mode");
    Services.obs.addObserver(observer, "private-browsing-transition-complete", false);
  };
  registerCleanupFunction(function() {
    gPrivateBrowsingUI.onEnterPrivateBrowsing = originalOnEnter;
    gPrivateBrowsingUI.onExitPrivateBrowsing = originalOnExit;
  });

  pb.privateBrowsingEnabled = true;
}
