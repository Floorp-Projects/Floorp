/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

registerCleanupFunction(function() {
  gBrowser.removeCurrentTab();
});

var gTests = [

{
  desc: "device sharing animation on background tabs",
  run: function* checkAudioVideo() {
    function* getStreamAndCheckBackgroundAnim(aAudio, aVideo, aSharing) {
      // Get a stream
      let popupPromise = promisePopupNotificationShown("webRTC-shareDevices");
      yield promiseRequestDevice(aAudio, aVideo);
      yield popupPromise;
      yield expectObserverCalled("getUserMedia:request");

      yield promiseMessage("ok", () => {
        PopupNotifications.panel.firstChild.button.click();
      });
      yield expectObserverCalled("getUserMedia:response:allow");
      yield expectObserverCalled("recording-device-events");
      let expected = [];
      if (aVideo)
        expected.push("Camera");
      if (aAudio)
        expected.push("Microphone");
      is((yield getMediaCaptureState()), expected.join("And"),
         "expected stream to be shared");

      // Check the attribute on the tab, and check there's no visible
      // sharing icon on the tab
      let tab = gBrowser.selectedTab;
      is(tab.getAttribute("sharing"), aSharing,
         "the tab has the attribute to show the " + aSharing + " icon");
      let icon =
        document.getAnonymousElementByAttribute(tab, "anonid", "sharing-icon");
      is(window.getComputedStyle(icon).display, "none",
         "the animated sharing icon of the tab is hidden");

      // After selecting a new tab, check the attribute is still there,
      // and the icon is now visible.
      yield BrowserTestUtils.switchTab(gBrowser, gBrowser.addTab());
      is(gBrowser.selectedTab.getAttribute("sharing"), "",
         "the new tab doesn't have the 'sharing' attribute");
      is(tab.getAttribute("sharing"), aSharing,
         "the tab still has the 'sharing' attribute");
      isnot(window.getComputedStyle(icon).display, "none",
            "the animated sharing icon of the tab is now visible");

      // Ensure the icon disappears when selecting the tab.
      yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
      ok(tab.selected, "the tab with ongoing sharing is selected again");
      is(window.getComputedStyle(icon).display, "none",
         "the animated sharing icon is gone after selecting the tab again");

      // And finally verify the attribute is removed when closing the stream.
      yield closeStream();

      // TODO(Bug 1304997): Fix the race in closeStream() and remove this
      // promiseWaitForCondition().
      yield promiseWaitForCondition(() => !tab.getAttribute("sharing"));
      is(tab.getAttribute("sharing"), "",
         "the tab no longer has the 'sharing' attribute after closing the stream");
    }

    yield getStreamAndCheckBackgroundAnim(true, true, "camera");
    yield getStreamAndCheckBackgroundAnim(false, true, "camera");
    yield getStreamAndCheckBackgroundAnim(true, false, "microphone");
  }
}

];

function test() {
  waitForExplicitFinish();

  let tab = gBrowser.addTab();
  gBrowser.selectedTab = tab;
  let browser = tab.linkedBrowser;

  browser.messageManager.loadFrameScript(CONTENT_SCRIPT_HELPER, true);

  browser.addEventListener("load", function onload() {
    browser.removeEventListener("load", onload, true);

    is(PopupNotifications._currentNotifications.length, 0,
       "should start the test without any prior popup notification");

    Task.spawn(function* () {
      yield SpecialPowers.pushPrefEnv({"set": [[PREF_PERMISSION_FAKE, true]]});

      for (let testCase of gTests) {
        info(testCase.desc);
        yield testCase.run();
      }
    }).then(finish, ex => {
     Cu.reportError(ex);
     ok(false, "Unexpected Exception: " + ex);
     finish();
    });
  }, true);
  let rootDir = getRootDirectory(gTestPath);
  rootDir = rootDir.replace("chrome://mochitests/content/",
                            "https://example.com/");
  content.location = rootDir + "get_user_media.html";
}
