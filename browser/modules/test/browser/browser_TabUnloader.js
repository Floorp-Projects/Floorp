/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { TabUnloader } = ChromeUtils.import(
  "resource:///modules/TabUnloader.jsm"
);

const BASE_URL = "http://example.com/browser/browser/modules/test/browser/";

async function play(tab) {
  let browser = tab.linkedBrowser;
  await ContentTask.spawn(browser, {}, async function() {
    let audio = content.document.querySelector("audio");
    await audio.play();
  });
}

async function addTab() {
  return BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: BASE_URL + "dummy_page.html",
    waitForLoad: true,
  });
}

async function addAudioTab() {
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: BASE_URL + "file_mediaPlayback.html",
    waitForLoad: true,
    waitForStateStop: true,
  });

  await play(tab);
  return tab;
}

add_task(async function test() {
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

  // Check that the tabs are present
  ok(
    tab1.linkedPanel &&
      tab2.linkedPanel &&
      pinnedTab.linkedPanel &&
      soundTab.linkedPanel &&
      pinnedSoundTab.linkedPanel,
    "tabs are present"
  );

  // Check that heap-minimize memory-pressure events do not unload tabs
  TabUnloader.observe(null, "memory-pressure", "heap-minimize");
  ok(
    tab1.linkedPanel &&
      tab2.linkedPanel &&
      pinnedTab.linkedPanel &&
      soundTab.linkedPanel &&
      pinnedSoundTab.linkedPanel,
    "heap-minimize memory-pressure notification did not unload a tab"
  );

  // Check that low-memory memory-pressure events unload tabs
  TabUnloader.observe(null, "memory-pressure", "low-memory");
  ok(
    !tab1.linkedPanel,
    "low-memory memory-pressure notification unloaded the LRU tab"
  );

  // If no normal tab is available unload pinned tabs
  TabUnloader.observe(null, "memory-pressure", "low-memory");
  ok(!tab2.linkedPanel, "unloaded a second tab in LRU order");
  TabUnloader.observe(null, "memory-pressure", "low-memory");
  ok(!pinnedTab.linkedPanel, "unloaded a pinned tab");

  // If no pinned tab is available unload tabs playing sound
  TabUnloader.observe(null, "memory-pressure", "low-memory");
  ok(!soundTab.linkedPanel, "unloaded a tab playing sound");

  // If no pinned tab or tab playing sound is available unload tabs that are
  // both pinned and playing sound
  TabUnloader.observe(null, "memory-pressure", "low-memory");
  ok(!pinnedSoundTab.linkedPanel, "unloaded a pinned tab playing sound");

  // Check low-memory-ongoing events
  await BrowserTestUtils.switchTab(gBrowser, tab1);
  await BrowserTestUtils.switchTab(gBrowser, tab0);
  TabUnloader.observe(null, "memory-pressure", "low-memory-ongoing");
  ok(
    !tab1.linkedPanel,
    "low-memory memory-pressure notification unloaded the LRU tab"
  );

  // Cleanup
  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(pinnedTab);
  BrowserTestUtils.removeTab(soundTab);
  BrowserTestUtils.removeTab(pinnedSoundTab);
});
