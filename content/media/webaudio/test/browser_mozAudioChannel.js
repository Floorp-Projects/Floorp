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

function whenBrowserUnloaded(aBrowser, aCallback) {
  aBrowser.addEventListener("unload", function onUnload() {
    aBrowser.removeEventListener("unload", onUnload, true);
    executeSoon(aCallback);
  }, true);
}

var event;
var next = function() {}

function eventListener(evt) {
  info("Event has been received!");
  is(evt.detail.msg, event, "AudioContext has been received the right event: " + event);
  next();
}

function test() {

  waitForExplicitFinish();

  let testURL = "http://mochi.test:8888/browser/" +
    "content/media/webaudio/test/browser_mozAudioChannel.html";

  SpecialPowers.pushPrefEnv({"set": [["media.defaultAudioChannel", "content" ],
                                     ["media.useAudioChannelService", true ]]},
    function() {
      let tab1 = gBrowser.addTab(testURL);
      gBrowser.selectedTab = tab1;

      whenBrowserLoaded(tab1.linkedBrowser, function() {
        let doc = tab1.linkedBrowser.contentDocument;
        tab1.linkedBrowser.contentWindow.addEventListener('testmozchannel', eventListener, false);

        SpecialPowers.pushPrefEnv({"set": [["media.defaultAudioChannel", "telephony" ]]},
          function() {
            event = 'mozinterruptbegin';
            next = function() {
              info("Next is called.");
              event = 'mozinterruptend';
              next =  function() {
                info("Next is called again.");
                tab1.linkedBrowser.contentWindow.removeEventListener('testmozchannel', eventListener);
                gBrowser.removeTab(tab1);
                finish();
              }

              info("Unloading a tab...");
              whenBrowserUnloaded(tab2.linkedBrowser, function() { info("Tab unloaded."); });

              gBrowser.removeTab(tab2);
              gBrowser.selectedTab = tab1;
            }

            let tab2 = gBrowser.addTab(testURL);
            gBrowser.selectedTab = tab2;

            info("Loading the tab...");
            whenBrowserLoaded(tab2.linkedBrowser, function() { info("Tab restored."); });
          }
        );
      });
    }
  );
}
