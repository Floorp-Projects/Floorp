/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests gBrowser#updateBrowserSharing
 */
add_task(async function testBrowserSharingStateSetter() {
  const WEBRTC_TEST_STATE = {
    camera: 0,
    microphone: 1,
    paused: false,
    sharing: "microphone",
    showMicrophoneIndicator: true,
    showScreenSharingIndicator: "",
    windowId: 0,
  };

  const WEBRTC_TEST_STATE2 = {
    camera: 1,
    microphone: 1,
    paused: false,
    sharing: "camera",
    showCameraIndicator: true,
    showMicrophoneIndicator: true,
    showScreenSharingIndicator: "",
    windowId: 1,
  };

  await BrowserTestUtils.withNewTab("https://example.com", async browser => {
    let tab = gBrowser.selectedTab;
    is(tab._sharingState, undefined, "No sharing state initially.");
    ok(!tab.hasAttribute("sharing"), "No tab sharing attribute initially.");

    // Set an active sharing state for webrtc
    gBrowser.updateBrowserSharing(browser, { webRTC: WEBRTC_TEST_STATE });
    Assert.deepEqual(
      tab._sharingState,
      { webRTC: WEBRTC_TEST_STATE },
      "Should have correct webRTC sharing state."
    );
    is(
      tab.getAttribute("sharing"),
      WEBRTC_TEST_STATE.sharing,
      "Tab sharing attribute reflects webRTC sharing state."
    );

    // Set sharing state for geolocation
    gBrowser.updateBrowserSharing(browser, { geo: true });
    Assert.deepEqual(
      tab._sharingState,
      {
        webRTC: WEBRTC_TEST_STATE,
        geo: true,
      },
      "Should have sharing state for both webRTC and geolocation."
    );
    is(
      tab.getAttribute("sharing"),
      WEBRTC_TEST_STATE.sharing,
      "Geolocation sharing doesn't update the tab sharing attribute."
    );

    // Update webRTC sharing state
    gBrowser.updateBrowserSharing(browser, { webRTC: WEBRTC_TEST_STATE2 });
    Assert.deepEqual(
      tab._sharingState,
      { geo: true, webRTC: WEBRTC_TEST_STATE2 },
      "Should have updated webRTC sharing state while maintaining geolocation state."
    );
    is(
      tab.getAttribute("sharing"),
      WEBRTC_TEST_STATE2.sharing,
      "Tab sharing attribute reflects webRTC sharing state."
    );

    // Clear webRTC sharing state
    gBrowser.updateBrowserSharing(browser, { webRTC: null });
    Assert.deepEqual(
      tab._sharingState,
      { geo: true, webRTC: null },
      "Should only have sharing state for geolocation."
    );
    ok(
      !tab.hasAttribute("sharing"),
      "Ending webRTC sharing should remove tab sharing attribute."
    );

    // Clear geolocation sharing state
    gBrowser.updateBrowserSharing(browser, { geo: null });
    Assert.deepEqual(tab._sharingState, { geo: null, webRTC: null });
    ok(
      !tab.hasAttribute("sharing"),
      "Tab sharing attribute should not be set."
    );
  });
});
