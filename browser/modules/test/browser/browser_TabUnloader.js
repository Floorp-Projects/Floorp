/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { TabUnloader } = ChromeUtils.importESModule(
  "resource:///modules/TabUnloader.sys.mjs"
);

const BASE_URL = "https://example.com/browser/browser/modules/test/browser/";

async function play(tab) {
  let browser = tab.linkedBrowser;

  let waitForAudioPromise = BrowserTestUtils.waitForEvent(
    tab,
    "TabAttrModified",
    false,
    event => {
      return (
        event.detail.changed.includes("soundplaying") &&
        tab.hasAttribute("soundplaying")
      );
    }
  );

  await SpecialPowers.spawn(browser, [], async function () {
    let audio = content.document.querySelector("audio");
    await audio.play();
  });

  await waitForAudioPromise;
}

async function addTab(win = window) {
  return BrowserTestUtils.openNewForegroundTab({
    gBrowser: win.gBrowser,
    url: BASE_URL + "dummy_page.html",
    waitForLoad: true,
  });
}

async function addPrivTab(win = window) {
  const tab = BrowserTestUtils.addTab(
    win.gBrowser,
    BASE_URL + "dummy_page.html"
  );
  const browser = win.gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);
  return tab;
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
      function () {
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

async function pressure() {
  let tabDiscarded = BrowserTestUtils.waitForEvent(
    document,
    "TabBrowserDiscarded",
    true
  );
  TabUnloader.unloadTabAsync(null);
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
  TabUnloader.unloadTabAsync(null);
  return promise;
}

async function compareTabOrder(expectedOrder) {
  let tabInfo = await TabUnloader.getSortedTabs(null);

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
const PREF_MAC_LOW_MEM_RESPONSE = "browser.lowMemoryResponseMask";

add_task(async function test() {
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(PREF_ENABLE_UNLOADER);
    if (AppConstants.platform == "macosx") {
      Services.prefs.clearUserPref(PREF_MAC_LOW_MEM_RESPONSE);
    }
  });
  Services.prefs.setBoolPref(PREF_ENABLE_UNLOADER, true);

  // On Mac, tab unloading and memory pressure notifications are limited
  // to Nightly so force them on for this test for non-Nightly builds. i.e.,
  // tests on Release and Beta builds. Mac tab unloading and memory pressure
  // notifications require this pref to be set.
  if (AppConstants.platform == "macosx") {
    Services.prefs.setIntPref(PREF_MAC_LOW_MEM_RESPONSE, 3);
  }

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

  // Open a new private window and add a tab
  const windowPriv = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  const tabPriv0 = windowPriv.gBrowser.tabs[0];
  const tabPriv1 = await addPrivTab(windowPriv);

  // Move the original window to the foreground to pass the tests
  gBrowser.selectedTab = tab0;
  tab0.ownerGlobal.focus();

  // Pretend we've visited the tabs
  await BrowserTestUtils.switchTab(windowPriv.gBrowser, tabPriv1);
  await BrowserTestUtils.switchTab(windowPriv.gBrowser, tabPriv0);
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
    tabPriv1,
    soundTab,
    tab0,
    pinnedSoundTab,
    tabPriv0,
  ]);

  // Check that the tabs are present
  ok(
    tab1.linkedPanel &&
      tab2.linkedPanel &&
      pinnedTab.linkedPanel &&
      soundTab.linkedPanel &&
      pinnedSoundTab.linkedPanel &&
      tabPriv0.linkedPanel &&
      tabPriv1.linkedPanel,
    "tabs are present"
  );

  // Check that low-memory memory-pressure events unload tabs
  await pressure();
  ok(
    !tab1.linkedPanel,
    "low-memory memory-pressure notification unloaded the LRU tab"
  );

  await compareTabOrder([
    tab2,
    pinnedTab,
    tabPriv1,
    soundTab,
    tab0,
    pinnedSoundTab,
    tabPriv0,
  ]);

  // If no normal tab is available unload pinned tabs
  await pressure();
  ok(!tab2.linkedPanel, "unloaded a second tab in LRU order");
  await compareTabOrder([
    pinnedTab,
    tabPriv1,
    soundTab,
    tab0,
    pinnedSoundTab,
    tabPriv0,
  ]);

  ok(soundTab.soundPlaying, "tab is still playing sound");

  await pressure();
  ok(!pinnedTab.linkedPanel, "unloaded a pinned tab");
  await compareTabOrder([tabPriv1, soundTab, tab0, pinnedSoundTab, tabPriv0]);

  ok(pinnedSoundTab.soundPlaying, "tab is still playing sound");

  // There are no unloadable tabs.
  TabUnloader.unloadTabAsync(null);
  ok(tabPriv1.linkedPanel, "a tab in a private window is never unloaded");

  const histogram = TelemetryTestUtils.getAndClearHistogram(
    "TAB_UNLOAD_TO_RELOAD"
  );

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
  await BrowserTestUtils.switchTab(gBrowser, pinnedTab);

  const hist = histogram.snapshot();
  const numEvents = Object.values(hist.values).reduce((a, b) => a + b);
  Assert.equal(numEvents, 2, "two tabs have been reloaded.");

  // tab0 has never been unloaded.  No data is added to the histogram.
  await BrowserTestUtils.switchTab(gBrowser, tab0);

  await compareTabOrder([
    tab1,
    pinnedTab,
    tabPriv1,
    soundTab,
    webrtcTab,
    anotherSoundTab,
    tab0,
    pinnedSoundTab,
    tabPriv0,
  ]);

  await BrowserTestUtils.closeWindow(windowPriv);

  let window2 = await BrowserTestUtils.openNewBrowserWindow();
  let win2tab1 = window2.gBrowser.selectedTab;
  let win2tab2 = await addTab(window2);
  let win2winrtcTab = await addWebRTCTab(window2);
  let win2tab3 = await addTab(window2);

  await compareTabOrder([
    tab1,
    win2tab1,
    win2tab2,
    pinnedTab,
    soundTab,
    webrtcTab,
    anotherSoundTab,
    win2winrtcTab,
    tab0,
    win2tab3,
    pinnedSoundTab,
  ]);

  await BrowserTestUtils.closeWindow(window2);

  await compareTabOrder([
    tab1,
    pinnedTab,
    soundTab,
    webrtcTab,
    anotherSoundTab,
    tab0,
    pinnedSoundTab,
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
