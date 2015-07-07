/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function whenBrowserLoaded(aBrowser, aCallback) {
  aBrowser.addEventListener("load", function onLoad(event) {
    if (event.target == aBrowser.contentDocument) {
      aBrowser.removeEventListener("load", onLoad, true);
      executeSoon(aCallback);
    }
  }, true);
}

function whenTabRestored(aTab, aCallback) {
  aTab.addEventListener("SSTabRestored", function onRestored(aEvent) {
    aTab.removeEventListener("SSTabRestored", onRestored, true);
    executeSoon(function executeWhenTabRestored() {
      aCallback();
    });
  }, true);
}

function whenBrowserUnloaded(aBrowser, aCallback) {
  aBrowser.addEventListener("unload", function onUnload() {
    aBrowser.removeEventListener("unload", onUnload, true);
    executeSoon(aCallback);
  }, true);
}

function test() {

  waitForExplicitFinish();

  let testURL = "http://mochi.test:8888/browser/" +
    "dom/media/webaudio/test/browser_mozAudioChannel_muted.html";

  SpecialPowers.pushPrefEnv({"set": [["media.defaultAudioChannel", "content" ],
                                     ["media.useAudioChannelAPI", true ],
                                     ["media.useAudioChannelService", true ]]},
    function() {
      let tab1 = gBrowser.addTab(testURL);
      gBrowser.selectedTab = tab1;

      whenBrowserLoaded(tab1.linkedBrowser, function() {
        let doc = tab1.linkedBrowser.contentDocument;
        is(doc.getElementById("mozAudioChannelTest").textContent, "READY",
           "Test is ready to run");

        SpecialPowers.pushPrefEnv({"set": [["media.defaultAudioChannel", "telephony" ]]},
          function() {
            let tab2 = gBrowser.duplicateTab(tab1);
            gBrowser.selectedTab = tab2;
            whenTabRestored(tab2, function() {
              is(doc.getElementById("mozAudioChannelTest").textContent, "mozinterruptbegin",
                 "AudioContext should be muted by the second tab.");

              whenBrowserUnloaded(tab2.linkedBrowser, function() {
                is(doc.getElementById("mozAudioChannelTest").textContent, "mozinterruptend",
                   "AudioContext should be muted by the second tab.");
                gBrowser.removeTab(tab1);
                finish();
              });

              gBrowser.removeTab(tab2);
              gBrowser.selectedTab = tab1;
            });
          }
        );
      });
    }
  );
}
