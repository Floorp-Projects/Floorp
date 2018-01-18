"use strict";

add_task(async function test_tabs_mediaIndicators() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");
  // setBrowserSharing is called when a request for media icons occurs.  We're
  // just testing that extension tabs get the info and are updated when it is
  // called.
  gBrowser.setBrowserSharing(tab.linkedBrowser, {screen: "Window", microphone: true, camera: true});

  async function background() {
    let tabs = await browser.tabs.query({microphone: true});
    let testTab = tabs[0];

    let state = testTab.sharingState;
    browser.test.assertTrue(state.camera, "sharing camera was turned on");
    browser.test.assertTrue(state.microphone, "sharing mic was turned on");
    browser.test.assertEq(state.screen, "Window", "sharing screen is window");

    tabs = await browser.tabs.query({screen: true});
    browser.test.assertEq(tabs.length, 1, "screen sharing tab was found");

    tabs = await browser.tabs.query({screen: "Window"});
    browser.test.assertEq(tabs.length, 1, "screen sharing (window) tab was found");

    tabs = await browser.tabs.query({screen: "Screen"});
    browser.test.assertEq(tabs.length, 0, "screen sharing tab was not found");

    browser.tabs.onUpdated.addListener((tabId, changeInfo, tab) => {
      if (testTab.id !== tabId) {
        return;
      }
      let state = tab.sharingState;
      browser.test.assertFalse(state.camera, "sharing camera was turned off");
      browser.test.assertFalse(state.microphone, "sharing mic was turned off");
      browser.test.assertFalse(state.screen, "sharing screen was turned off");
      browser.test.notifyPass("done");
    });
    browser.test.sendMessage("ready");
  }

  let extdata = {
    manifest: {permissions: ["tabs"]},
    useAddonManager: "temporary",
    background,
  };
  let extension = ExtensionTestUtils.loadExtension(extdata);
  await extension.startup();

  // Test that onUpdated is called after the sharing state is changed from
  // chrome code.
  await extension.awaitMessage("ready");
  gBrowser.setBrowserSharing(tab.linkedBrowser, {});

  await extension.awaitFinish("done");
  await extension.unload();

  await BrowserTestUtils.removeTab(tab);
});
