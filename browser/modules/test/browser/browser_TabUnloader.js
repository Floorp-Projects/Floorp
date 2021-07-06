/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { TabUnloader } = ChromeUtils.import(
  "resource:///modules/TabUnloader.jsm"
);

const BASE_URL = "https://example.com/browser/browser/modules/test/browser/";

async function play(tab) {
  let browser = tab.linkedBrowser;
  await SpecialPowers.spawn(browser, [], async function() {
    let audio = content.document.querySelector("audio");
    await audio.play();
  });
}

async function addTab(win = window) {
  return BrowserTestUtils.openNewForegroundTab({
    gBrowser: win.gBrowser,
    url: BASE_URL + "dummy_page.html",
    waitForLoad: true,
  });
}

async function addAudioTab(win = window) {
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser: win.gBrowser,
    url: BASE_URL + "file_mediaPlayback.html",
    waitForLoad: true,
    waitForStateStop: true,
  });

  await play(tab);
  return tab;
}

async function addWebRTCTab(win = window) {
  let popupPromise = new Promise(resolve => {
    win.PopupNotifications.panel.addEventListener(
      "popupshown",
      function() {
        executeSoon(resolve);
      },
      { once: true }
    );
  });

  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser: win.gBrowser,
    url: BASE_URL + "file_webrtc.html",
    waitForLoad: true,
    waitForStateStop: true,
  });

  await popupPromise;

  let recordingPromise = BrowserTestUtils.contentTopicObserved(
    tab.linkedBrowser.browsingContext,
    "recording-device-events"
  );
  win.PopupNotifications.panel.firstElementChild.button.click();
  await recordingPromise;

  return tab;
}

async function pressure(tab) {
  let tabDiscarded = BrowserTestUtils.waitForEvent(
    document,
    "TabBrowserDiscarded",
    true
  );
  TabUnloader.unloadTabAsync();
  return tabDiscarded;
}

function pressureAndObserve(aExpectedTopic) {
  const promise = new Promise(resolve => {
    const observer = {
      QueryInterface: ChromeUtils.generateQI([
        "nsIObserver",
        "nsISupportsWeakReference",
      ]),
      observe(aSubject, aTopicInner, aData) {
        if (aTopicInner == aExpectedTopic) {
          Services.obs.removeObserver(observer, aTopicInner);
          resolve(aData);
        }
      },
    };
    Services.obs.addObserver(observer, aExpectedTopic);
  });
  TabUnloader.unloadTabAsync();
  return promise;
}

async function compareTabOrder(expectedOrder) {
  let tabInfo = await TabUnloader.getSortedTabs();

  is(
    tabInfo.length,
    expectedOrder.length,
    "right number of tabs in discard sort list"
  );
  for (let idx = 0; idx < expectedOrder.length; idx++) {
    is(tabInfo[idx].tab, expectedOrder[idx], "index " + idx + " is correct");
  }
}

const PREF_PERMISSION_FAKE = "media.navigator.permission.fake";
const PREF_AUDIO_LOOPBACK = "media.audio_loopback_dev";
const PREF_VIDEO_LOOPBACK = "media.video_loopback_dev";
const PREF_FAKE_STREAMS = "media.navigator.streams.fake";
const PREF_ENABLE_UNLOADER = "browser.tabs.unloadOnLowMemory";

add_task(async function test() {
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(PREF_ENABLE_UNLOADER);
  });
  Services.prefs.setBoolPref(PREF_ENABLE_UNLOADER, true);
  TabUnloader.init();

  // Set some WebRTC simulation preferences.
  let prefs = [
    [PREF_PERMISSION_FAKE, true],
    [PREF_AUDIO_LOOPBACK, ""],
    [PREF_VIDEO_LOOPBACK, ""],
    [PREF_FAKE_STREAMS, true],
  ];
  await SpecialPowers.pushPrefEnv({ set: prefs });

  // Set up 6 tabs, three normal ones, one pinned, one playing sound and one
  // pinned playing sound
  let tab0 = gBrowser.tabs[0];
  let tab1 = await addTab();
  let tab2 = await addTab();
  let pinnedTab = await addTab();
  gBrowser.pinTab(pinnedTab);
  let soundTab = await addAudioTab();
  let pinnedSoundTab = await addAudioTab();
  gBrowser.pinTab(pinnedSoundTab);

  // Pretend we've visited the tabs
  await BrowserTestUtils.switchTab(gBrowser, tab1);
  await BrowserTestUtils.switchTab(gBrowser, tab2);
  await BrowserTestUtils.switchTab(gBrowser, pinnedTab);
  await BrowserTestUtils.switchTab(gBrowser, soundTab);
  await BrowserTestUtils.switchTab(gBrowser, pinnedSoundTab);
  await BrowserTestUtils.switchTab(gBrowser, tab0);

  // Checks the tabs are in the state we expect them to be
  ok(pinnedTab.pinned, "tab is pinned");
  ok(pinnedSoundTab.soundPlaying, "tab is playing sound");
  ok(
    pinnedSoundTab.pinned && pinnedSoundTab.soundPlaying,
    "tab is pinned and playing sound"
  );

  await compareTabOrder([
    tab1,
    tab2,
    pinnedTab,
    soundTab,
    pinnedSoundTab,
    tab0,
  ]);

  // Check that the tabs are present
  ok(
    tab1.linkedPanel &&
      tab2.linkedPanel &&
      pinnedTab.linkedPanel &&
      soundTab.linkedPanel &&
      pinnedSoundTab.linkedPanel,
    "tabs are present"
  );

  // Check that low-memory memory-pressure events unload tabs
  await pressure(tab1);
  ok(
    !tab1.linkedPanel,
    "low-memory memory-pressure notification unloaded the LRU tab"
  );

  await compareTabOrder([tab2, pinnedTab, soundTab, pinnedSoundTab, tab0]);

  // If no normal tab is available unload pinned tabs
  await pressure(tab2);
  ok(!tab2.linkedPanel, "unloaded a second tab in LRU order");
  await compareTabOrder([pinnedTab, soundTab, pinnedSoundTab, tab0]);

  ok(soundTab.soundPlaying, "tab is no longer playing sound");

  await pressure(pinnedTab);
  ok(!pinnedTab.linkedPanel, "unloaded a pinned tab");
  await compareTabOrder([soundTab, pinnedSoundTab, tab0]);

  ok(pinnedSoundTab.soundPlaying, "tab is no longer playing sound");

  // If no pinned tab is available unload tabs playing sound
  await pressure(soundTab);
  ok(!soundTab.linkedPanel, "unloaded a tab playing sound");
  await compareTabOrder([pinnedSoundTab, tab0]);

  // If no pinned tab or tab playing sound is available unload tabs that are
  // both pinned and playing sound
  await pressure(pinnedSoundTab);
  ok(!pinnedSoundTab.linkedPanel, "unloaded a pinned tab playing sound");
  await compareTabOrder([]); // note that no tabs are returned when there are no discardable tabs.

  // It's possible that we're already in the memory-pressure state
  // and we may receive the "ongoing" message.
  const message = await pressureAndObserve("memory-pressure");
  Assert.ok(
    message == "low-memory" || message == "low-memory-ongoing",
    "observed the memory-pressure notification because of no discardable tab"
  );

  // Add a WebRTC tab and another sound tab.
  let webrtcTab = await addWebRTCTab();
  let anotherSoundTab = await addAudioTab();

  await BrowserTestUtils.switchTab(gBrowser, tab1);
  await BrowserTestUtils.switchTab(gBrowser, soundTab);
  await BrowserTestUtils.switchTab(gBrowser, pinnedTab);
  await BrowserTestUtils.switchTab(gBrowser, tab0);

  // Audio from the first sound tab was stopped when the tab was discarded earlier,
  // so it should be treated as if it isn't playing sound and should appear earlier
  // in the list.
  await compareTabOrder([
    tab1,
    soundTab,
    pinnedTab,
    webrtcTab,
    anotherSoundTab,
    tab0,
  ]);

  let window2 = await BrowserTestUtils.openNewBrowserWindow();
  let win2tab1 = window2.gBrowser.selectedTab;
  let win2tab2 = await addTab(window2);
  let win2winrtcTab = await addWebRTCTab(window2);
  let win2tab3 = await addTab(window2);

  await compareTabOrder([
    tab1,
    soundTab,
    win2tab1,
    win2tab2,
    pinnedTab,
    webrtcTab,
    anotherSoundTab,
    win2winrtcTab,
    tab0,
    win2tab3,
  ]);

  await BrowserTestUtils.closeWindow(window2);

  await compareTabOrder([
    tab1,
    soundTab,
    pinnedTab,
    webrtcTab,
    anotherSoundTab,
    tab0,
  ]);

  // Cleanup
  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(pinnedTab);
  BrowserTestUtils.removeTab(soundTab);
  BrowserTestUtils.removeTab(pinnedSoundTab);
  BrowserTestUtils.removeTab(webrtcTab);
  BrowserTestUtils.removeTab(anotherSoundTab);

  await awaitWebRTCClose();
});

// Wait for the WebRTC indicator window to close.
function awaitWebRTCClose() {
  if (
    Services.prefs.getBoolPref("privacy.webrtc.legacyGlobalIndicator", false) ||
    AppConstants.platform == "macosx"
  ) {
    return null;
  }

  let win = Services.wm.getMostRecentWindow("Browser:WebRTCGlobalIndicator");
  if (!win) {
    return null;
  }

  return new Promise(resolve => {
    win.addEventListener("unload", function listener(e) {
      if (e.target == win.document) {
        win.removeEventListener("unload", listener);
        executeSoon(resolve);
      }
    });
  });
}
