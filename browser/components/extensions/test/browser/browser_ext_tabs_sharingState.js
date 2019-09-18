"use strict";

add_task(async function test_tabs_mediaIndicators() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.webextensions.tabhide.enabled", true]],
  });

  let initialTab = gBrowser.selectedTab;
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com/#tab-sharing"
  );

  // Ensure that the tab to hide is not selected (otherwise
  // it will not be hidden because it is selected).
  gBrowser.selectedTab = initialTab;

  // updateBrowserSharing is called when a request for media icons occurs.  We're
  // just testing that extension tabs get the info and are updated when it is
  // called.
  gBrowser.updateBrowserSharing(tab.linkedBrowser, {
    webRTC: {
      sharing: "screen",
      screen: "Window",
      microphone: Ci.nsIMediaManagerService.STATE_CAPTURE_ENABLED,
      camera: Ci.nsIMediaManagerService.STATE_CAPTURE_ENABLED,
    },
  });

  async function background() {
    let tabs = await browser.tabs.query({ url: "http://example.com/*" });
    let testTab = tabs[0];

    browser.test.assertEq(
      testTab.url,
      "http://example.com/#tab-sharing",
      "Got the expected tab url"
    );

    browser.test.assertFalse(testTab.active, "test tab should not be selected");

    let state = testTab.sharingState;
    browser.test.assertTrue(state.camera, "sharing camera was turned on");
    browser.test.assertTrue(state.microphone, "sharing mic was turned on");
    browser.test.assertEq(state.screen, "Window", "sharing screen is window");

    tabs = await browser.tabs.query({ screen: true });
    browser.test.assertEq(tabs.length, 1, "screen sharing tab was found");

    tabs = await browser.tabs.query({ screen: "Window" });
    browser.test.assertEq(
      tabs.length,
      1,
      "screen sharing (window) tab was found"
    );

    tabs = await browser.tabs.query({ screen: "Screen" });
    browser.test.assertEq(tabs.length, 0, "screen sharing tab was not found");

    // Verify we cannot hide a sharing tab.
    let hidden = await browser.tabs.hide(testTab.id);
    browser.test.assertEq(hidden.length, 0, "unable to hide sharing tab");
    tabs = await browser.tabs.query({ hidden: true });
    browser.test.assertEq(tabs.length, 0, "unable to hide sharing tab");

    browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {
      if (testTab.id !== tabId) {
        return;
      }
      let state = changeInfo.sharingState;

      // Ignore tab update events unrelated to the sharing state.
      if (!state) {
        return;
      }

      browser.test.assertFalse(state.camera, "sharing camera was turned off");
      browser.test.assertFalse(state.microphone, "sharing mic was turned off");
      browser.test.assertFalse(state.screen, "sharing screen was turned off");

      // Verify we can hide the tab once it is not shared over webRTC anymore.
      let hidden = await browser.tabs.hide(testTab.id);
      browser.test.assertEq(hidden.length, 1, "tab hidden successfully");
      tabs = await browser.tabs.query({ hidden: true });
      browser.test.assertEq(tabs.length, 1, "hidden tab found");

      browser.test.notifyPass("done");
    });
    browser.test.sendMessage("ready");
  }

  let extdata = {
    manifest: { permissions: ["tabs", "tabHide"] },
    useAddonManager: "temporary",
    background,
  };
  let extension = ExtensionTestUtils.loadExtension(extdata);
  await extension.startup();

  // Test that onUpdated is called after the sharing state is changed from
  // chrome code.
  await extension.awaitMessage("ready");

  info("Updating browser sharing on the test tab");

  // Clear only the webRTC part of the browser sharing state
  // (used to test Bug 1577480 regression fix).
  gBrowser.updateBrowserSharing(tab.linkedBrowser, { webRTC: null });

  await extension.awaitFinish("done");
  await extension.unload();

  BrowserTestUtils.removeTab(tab);
});
