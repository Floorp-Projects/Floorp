const PREF_MULTISELECT_TABS = "browser.tabs.multiselect";
const PAGE = "https://example.com/browser/browser/base/content/test/general/file_mediaPlayback.html";

async function wait_for_tab_playing_event(tab, expectPlaying) {
  if (tab.soundPlaying == expectPlaying) {
    ok(true, "The tab should " + (expectPlaying ? "" : "not ") + "be playing");
    return true;
  }
  return BrowserTestUtils.waitForEvent(tab, "TabAttrModified", false, (event) => {
    if (event.detail.changed.includes("soundplaying")) {
      is(tab.hasAttribute("soundplaying"), expectPlaying, "The tab should " + (expectPlaying ? "" : "not ") + "be playing");
      is(tab.soundPlaying, expectPlaying, "The tab should " + (expectPlaying ? "" : "not ") + "be playing");
      return true;
    }
    return false;
  });
}

async function waitForTabMuteStateChangeEvent(tab) {
  return BrowserTestUtils.waitForEvent(tab, "TabAttrModified", false, (event) => {
    for (let attr of ["activemedia-blocked", "muted", "soundplaying"]) {
      if (event.detail.changed.includes(attr)) {
        return true;
      }
    }
    return false;
  });
}

async function is_audio_playing(tab) {
  let browser = tab.linkedBrowser;
  let isPlaying = await ContentTask.spawn(browser, {}, async function() {
    let audio = content.document.querySelector("audio");
    return !audio.paused;
  });
  return isPlaying;
}

async function play(tab) {
  let browser = tab.linkedBrowser;
  await ContentTask.spawn(browser, {}, async function() {
    let audio = content.document.querySelector("audio");
    audio.play();
  });

  // If the tab has already been muted, it means the tab won't get soundplaying,
  // so we don't need to check this attribute.
  if (browser.audioMuted) {
    return;
  }

  await waitForTabMuteStateChangeEvent(tab);
}

function disable_non_test_mouse(disable) {
  let utils = window.QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIDOMWindowUtils);
  utils.disableNonTestMouseEvents(disable);
}

function hover_icon(icon, tooltip) {
  disable_non_test_mouse(true);

  let popupShownPromise = BrowserTestUtils.waitForEvent(tooltip, "popupshown");
  EventUtils.synthesizeMouse(icon, 1, 1, {type: "mouseover"});
  EventUtils.synthesizeMouse(icon, 2, 2, {type: "mousemove"});
  EventUtils.synthesizeMouse(icon, 3, 3, {type: "mousemove"});
  EventUtils.synthesizeMouse(icon, 4, 4, {type: "mousemove"});
  return popupShownPromise;
}

function leave_icon(icon) {
  EventUtils.synthesizeMouse(icon, 0, 0, {type: "mouseout"});
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {type: "mousemove"});
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {type: "mousemove"});
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {type: "mousemove"});

  disable_non_test_mouse(false);
}

// The set of tabs which have ever had their mute state changed.
// Used to determine whether the tab should have a muteReason value.
let everMutedTabs = new WeakSet();

function get_wait_for_mute_promise(tab, expectMuted) {
  return BrowserTestUtils.waitForEvent(tab, "TabAttrModified", false, event => {
    if (event.detail.changed.includes("muted") || event.detail.changed.includes("activemedia-blocked")) {
      is(tab.hasAttribute("muted"), expectMuted, "The tab should " + (expectMuted ? "" : "not ") + "be muted");
      is(tab.muted, expectMuted, "The tab muted property " + (expectMuted ? "" : "not ") + "be true");

      if (expectMuted || everMutedTabs.has(tab)) {
        everMutedTabs.add(tab);
        is(tab.muteReason, null, "The tab should have a null muteReason value");
      } else {
        is(tab.muteReason, undefined, "The tab should have an undefined muteReason value");
      }
      return true;
    }
    return false;
  });
}

async function test_mute_tab(tab, icon, expectMuted) {
  let mutedPromise = waitForTabMuteStateChangeEvent(tab);

  let activeTab = gBrowser.selectedTab;

  let tooltip = document.getElementById("tabbrowser-tab-tooltip");

  await hover_icon(icon, tooltip);
  EventUtils.synthesizeMouseAtCenter(icon, {button: 0});
  leave_icon(icon);

  is(gBrowser.selectedTab, activeTab, "Clicking on mute should not change the currently selected tab");

  // If the audio is playing, we should check whether clicking on icon affects
  // the media element's playing state.
  let isAudioPlaying = await is_audio_playing(tab);
  if (isAudioPlaying) {
    await wait_for_tab_playing_event(tab, !expectMuted);
  }

  return mutedPromise;
}

function muted(tab) {
  return tab.linkedBrowser.audioMuted;
}

function activeMediaBlocked(tab) {
  return tab.activeMediaBlocked;
}

async function addMediaTab() {
  const tab = BrowserTestUtils.addTab(gBrowser, PAGE, { skipAnimation: true });
  const browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);
  return tab;
}

add_task(async function setPref() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_MULTISELECT_TABS, true]]
  });
});

add_task(async function muteTabs_usingButton() {
  let tab0 = await addMediaTab();
  let tab1 = await addMediaTab();
  let tab2 = await addMediaTab();
  let tab3 = await addMediaTab();
  let tab4 = await addMediaTab();

  let tabs = [tab0, tab1, tab2, tab3, tab4];

  await BrowserTestUtils.switchTab(gBrowser, tab0);
  await play(tab0);
  await play(tab1);
  await play(tab2);

  // Multiselecting tab1, tab2 and tab3
  await BrowserTestUtils.switchTab(gBrowser, tab1);
  await triggerClickOn(tab3, { shiftKey: true });

  is(gBrowser.multiSelectedTabsCount, 3, "Three multiselected tabs");
  ok(!tab0.multiselected, "Tab0 is not multiselected");
  ok(!tab4.multiselected, "Tab4 is not multiselected");

  // tab1,tab2 and tab3 should be multiselected.
  for (let i = 1; i <= 3; i++) {
    ok(tabs[i].multiselected, "Tab" + i + " is multiselected");
  }

  // All five tabs are unmuted
  for (let i = 0; i < 5; i++) {
    ok(!muted(tabs[i]), "Tab" + i + " is not muted");
  }

  // Mute tab0 which is not multiselected, thus other tabs muted state should not be affected
  let tab0MuteAudioBtn = document.getAnonymousElementByAttribute(tab0, "anonid", "soundplaying-icon");
  await test_mute_tab(tab0, tab0MuteAudioBtn, true);

  ok(muted(tab0), "Tab0 is muted");
  for (let i = 1; i <= 4; i++) {
    ok(!muted(tabs[i]), "Tab" + i + " is not muted");
  }

  // Now we multiselect tab0
  await triggerClickOn(tab0, { ctrlKey: true });

  // tab0, tab1, tab2, tab3 are multiselected
  for (let i = 0; i <= 3; i++) {
    ok(tabs[i].multiselected, "tab" + i + " is multiselected");
  }
  ok(!tab4.multiselected, "tab4 is not multiselected");

  // Check mute state
  ok(muted(tab0), "Tab0 is still muted");
  ok(!muted(tab1) && !activeMediaBlocked(tab1), "Tab1 is not muted");
  ok(activeMediaBlocked(tab2), "Tab2 is media-blocked");
  ok(!muted(tab3) && !activeMediaBlocked(tab3), "Tab3 is not muted and not activemedia-blocked");
  ok(!muted(tab4) && !activeMediaBlocked(tab4), "Tab4 is not muted and not activemedia-blocked");

  // Mute tab1 which is mutliselected, thus other multiselected tabs should be affected too
  // in the following way:
  //  a) muted tabs (tab0) will remain muted.
  //  b) unmuted tabs (tab1, tab3) will become muted.
  //  b) media-blocked tabs (tab2) will remain media-blocked.
  // However tab4 (unmuted) which is not multiselected should not be affected.
  let tab1MuteAudioBtn = document.getAnonymousElementByAttribute(tab1, "anonid", "soundplaying-icon");
  await test_mute_tab(tab1, tab1MuteAudioBtn, true);

  // Check mute state
  ok(muted(tab0), "Tab0 is still muted");
  ok(muted(tab1), "Tab1 is muted");
  ok(activeMediaBlocked(tab2), "Tab2 is still media-blocked");
  ok(muted(tab3), "Tab3 is now muted");
  ok(!muted(tab4) && !activeMediaBlocked(tab4), "Tab4 is not muted and not activemedia-blocked");


  for (let tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }
});

add_task(async function unmuteTabs_usingButton() {
  let tab0 = await addMediaTab();
  let tab1 = await addMediaTab();
  let tab2 = await addMediaTab();
  let tab3 = await addMediaTab();
  let tab4 = await addMediaTab();

  let tabs = [tab0, tab1, tab2, tab3, tab4];

  await BrowserTestUtils.switchTab(gBrowser, tab0);
  await play(tab0);
  await play(tab1);
  await play(tab2);

  // Mute tab3 and tab4
  tab3.toggleMuteAudio();
  tab4.toggleMuteAudio();

  // Multiselecting tab0, tab1, tab2 and tab3
  await triggerClickOn(tab3, { shiftKey: true });

  // Check mutliselection
  for (let i = 0; i <= 3; i++) {
    ok(tabs[i].multiselected, "tab" + i + " is multiselected");
  }
  ok(!tab4.multiselected, "tab4 is not multiselected");

  // Check tabs mute state
  ok(!muted(tab0) && !activeMediaBlocked(tab0), "Tab0 is not muted and not media-blocked");
  ok(activeMediaBlocked(tab1), "Tab1 is media-blocked");
  ok(activeMediaBlocked(tab2), "Tab2 is media-blocked");
  ok(muted(tab3), "Tab3 is muted");
  ok(muted(tab4), "Tab4 is muted");
  is(gBrowser.selectedTab, tab0, "Tab0 is active");

  // unmute tab0 which is mutliselected, thus other multiselected tabs should be affected too
  // in the following way:
  //  a) muted tabs (tab3) will become unmuted.
  //  b) unmuted tabs (tab0) will remain unmuted.
  //  b) media-blocked tabs (tab1, tab2) will get playing. (media not blocked anymore)
  // However tab4 (muted) which is not multiselected should not be affected.
  let tab3MuteAudioBtn = document.getAnonymousElementByAttribute(tab3, "anonid", "soundplaying-icon");
  await test_mute_tab(tab3, tab3MuteAudioBtn, false);

  ok(!muted(tab0) && !activeMediaBlocked(tab0), "Tab0 is unmuted and not media-blocked");
  ok(!muted(tab1) && !activeMediaBlocked(tab1), "Tab1 is unmuted and not media-blocked");
  ok(!muted(tab2) && !activeMediaBlocked(tab2), "Tab2 is unmuted and not media-blocked");
  ok(!muted(tab3) && !activeMediaBlocked(tab3), "Tab3 is unmuted and not media-blocked");
  ok(muted(tab4), "Tab4 is muted");
  is(gBrowser.selectedTab, tab0, "Tab0 is active");

  for (let tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }
});

add_task(async function playTabs_usingButton() {
  let tab0 = await addMediaTab();
  let tab1 = await addMediaTab();
  let tab2 = await addMediaTab();
  let tab3 = await addMediaTab();
  let tab4 = await addMediaTab();

  let tabs = [tab0, tab1, tab2, tab3, tab4];

  await BrowserTestUtils.switchTab(gBrowser, tab0);
  await play(tab0);
  await play(tab1);
  await play(tab2);

  // Multiselecting tab0, tab1, tab2 and tab3.
  await triggerClickOn(tab3, { shiftKey: true });

  // Mute tab0 and tab4
  tab0.toggleMuteAudio();
  tab4.toggleMuteAudio();

  // Check mutliselection
  for (let i = 0; i <= 3; i++) {
    ok(tabs[i].multiselected, "tab" + i + " is multiselected");
  }
  ok(!tab4.multiselected, "tab4 is not multiselected");

  // Check mute state
  ok(muted(tab0), "Tab0 is muted");
  ok(activeMediaBlocked(tab1), "Tab1 is media-blocked");
  ok(activeMediaBlocked(tab2), "Tab2 is media-blocked");
  ok(!muted(tab3) && !activeMediaBlocked(tab3), "Tab3 is not muted and not activemedia-blocked");
  ok(muted(tab4), "Tab4 is muted");
  is(gBrowser.selectedTab, tab0, "Tab0 is active");

  // play tab2 which is mutliselected, thus other multiselected tabs should be affected too
  // in the following way:
  //  a) muted tabs (tab0) will become unmuted.
  //  b) unmuted tabs (tab3) will remain unmuted.
  //  b) media-blocked tabs (tab1, tab2) will get playing. (media not blocked anymore)
  // However tab4 (muted) which is not multiselected should not be affected.
  let tab2MuteAudioBtn = document.getAnonymousElementByAttribute(tab2, "anonid", "soundplaying-icon");
  await test_mute_tab(tab2, tab2MuteAudioBtn, false);

  ok(!muted(tab0) && !activeMediaBlocked(tab0), "Tab0 is unmuted and not activemedia-blocked");
  ok(!muted(tab1) && !activeMediaBlocked(tab1), "Tab1 is unmuted and not activemedia-blocked");
  ok(!muted(tab2) && !activeMediaBlocked(tab2), "Tab2 is unmuted and not activemedia-blocked");
  ok(!muted(tab3) && !activeMediaBlocked(tab3), "Tab3 is unmuted and not activemedia-blocked");
  ok(muted(tab4), "Tab4 is muted");
  is(gBrowser.selectedTab, tab0, "Tab0 is active");


  for (let tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }
});

add_task(async function checkTabContextMenu() {
  let tab0 = await addMediaTab();
  let tab1 = await addMediaTab();
  let tab2 = await addMediaTab();
  let tab3 = await addMediaTab();
  let tabs = [tab0, tab1, tab2, tab3];

  let menuItemToggleMuteTab = document.getElementById("context_toggleMuteTab");
  let menuItemToggleMuteSelectedTabs = document.getElementById("context_toggleMuteSelectedTabs");

  await play(tab0);
  tab0.toggleMuteAudio();
  await play(tab1);
  tab2.toggleMuteAudio();

  // Mutliselect tab0, tab1, tab2.
  await triggerClickOn(tab0, { ctrlKey: true });
  await triggerClickOn(tab1, { ctrlKey: true });
  await triggerClickOn(tab2, { ctrlKey: true });

  // Check mutliselected tabs
  for (let i = 0; i <= 2; i++) {
    ok(tabs[i].multiselected, "Tab" + i + " is multi-selected");
  }
  ok(!tab3.multiselected, "Tab3 is not multiselected");

  // Check mute state for tabs
  ok(!muted(tab0) && !activeMediaBlocked(tab0), "Tab0 is not muted and not activemedia-blocked");
  ok(activeMediaBlocked(tab1), "Tab1 is media-blocked");
  ok(muted(tab2), "Tab2 is muted");
  ok(!muted(tab3, "Tab3 is not muted"));

  let labels = ["Mute Tabs", "Play Tabs", "Unmute Tabs"];

  for (let i = 0; i <= 2; i++) {
    updateTabContextMenu(tabs[i]);
    ok(menuItemToggleMuteTab.hidden,
      "toggleMuteAudio menu for one tab is hidden - contextTab" + i);
    ok(!menuItemToggleMuteSelectedTabs.hidden,
      "toggleMuteAudio menu for selected tab is not hidden - contextTab" + i);
    is(menuItemToggleMuteSelectedTabs.label, labels[i], labels[i] + " should be shown");
  }

  updateTabContextMenu(tab3);
  ok(!menuItemToggleMuteTab.hidden, "toggleMuteAudio menu for one tab is not hidden");
  ok(menuItemToggleMuteSelectedTabs.hidden, "toggleMuteAudio menu for selected tab is hidden");

  for (let tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }
});
