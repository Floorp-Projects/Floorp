/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Ensure tabs that are active media blocked act correctly
 * when we try to unblock them using the "Play Tab" icon or by calling
 * resumeDelayedMedia()
 */

"use strict";

const PREF_DELAY_AUTOPLAY = "media.block-autoplay-until-in-foreground";

async function playMedia(tab, { expectBlocked }) {
  let blockedPromise = wait_for_tab_media_blocked_event(tab, expectBlocked);
  tab.resumeDelayedMedia();
  await blockedPromise;
  is(activeMediaBlocked(tab), expectBlocked, "tab has wrong media block state");
}

add_task(async function setPref() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_DELAY_AUTOPLAY, true]],
  });
});

/*
 * Playing blocked media will not mute the selected tabs
 */
add_task(async function testDelayPlayWontAffectUnmuteStatus() {
  info("Add media tabs");
  let tab0 = await addMediaTab();
  let tab1 = await addMediaTab();

  let tabs = [tab0, tab1];

  info("Play both tabs");
  await play(tab0, false);
  await play(tab1, false);

  // Check tabs are unmuted
  ok(!muted(tab0), "Tab0 is unmuted");
  ok(!muted(tab1), "Tab1 is unmuted");

  info("Play media on tab0");
  await playMedia(tab0, { expectBlocked: false });

  // Check tabs are still unmuted
  ok(!muted(tab0), "Tab0 is unmuted");
  ok(!muted(tab1), "Tab1 is unmuted");

  info("Play media on tab1");
  await playMedia(tab1, { expectBlocked: false });

  // Check tabs are still unmuted
  ok(!muted(tab0), "Tab0 is unmuted");
  ok(!muted(tab1), "Tab1 is unmuted");

  for (let tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }
});

/*
 * Playing blocked media will not unmute the selected tabs
 */
add_task(async function testDelayPlayWontAffectMuteStatus() {
  info("Add media tabs");
  let tab0 = await addMediaTab();
  let tab1 = await addMediaTab();

  info("Play both tabs");
  await play(tab0, false);
  await play(tab1, false);

  // Mute both tabs
  toggleMuteAudio(tab0, true);
  toggleMuteAudio(tab1, true);

  // Check tabs are muted
  ok(muted(tab0), "Tab0 is muted");
  ok(muted(tab1), "Tab1 is muted");

  info("Play media on tab0");
  await playMedia(tab0, { expectBlocked: false });

  // Check tabs are still muted
  ok(muted(tab0), "Tab0 is muted");
  ok(muted(tab1), "Tab1 is muted");

  info("Play media on tab1");
  await playMedia(tab1, { expectBlocked: false });

  // Check tabs are still muted
  ok(muted(tab0), "Tab0 is muted");
  ok(muted(tab1), "Tab1 is muted");

  BrowserTestUtils.removeTab(tab0);
  BrowserTestUtils.removeTab(tab1);
});

/*
 * Switching tabs will unblock media
 */
add_task(async function testDelayPlayWhenSwitchingTab() {
  info("Add media tabs");
  let tab0 = await addMediaTab();
  let tab1 = await addMediaTab();

  info("Play both tabs");
  await play(tab0, false);
  await play(tab1, false);

  // Both tabs are initially active media blocked after being played
  ok(activeMediaBlocked(tab0), "Tab0 is activemedia-blocked");
  ok(activeMediaBlocked(tab1), "Tab1 is activemedia-blocked");

  info("Switch to tab0");
  await BrowserTestUtils.switchTab(gBrowser, tab0);
  is(gBrowser.selectedTab, tab0, "Tab0 is active");

  // tab0 unblocked, tab1 blocked
  ok(!activeMediaBlocked(tab0), "Tab0 is not activemedia-blocked");
  ok(activeMediaBlocked(tab1), "Tab1 is activemedia-blocked");

  info("Switch to tab1");
  await BrowserTestUtils.switchTab(gBrowser, tab1);
  is(gBrowser.selectedTab, tab1, "Tab1 is active");

  // tab0 unblocked, tab1 unblocked
  ok(!activeMediaBlocked(tab0), "Tab0 is not activemedia-blocked");
  ok(!activeMediaBlocked(tab1), "Tab1 is not activemedia-blocked");

  BrowserTestUtils.removeTab(tab0);
  BrowserTestUtils.removeTab(tab1);
});

/*
 * The "Play Tab" icon unblocks media
 */
add_task(async function testDelayPlayWhenUsingButton() {
  info("Add media tabs");
  let tab0 = await addMediaTab();
  let tab1 = await addMediaTab();

  info("Play both tabs");
  await play(tab0, false);
  await play(tab1, false);

  // Both tabs are initially active media blocked after being played
  ok(activeMediaBlocked(tab0), "Tab0 is activemedia-blocked");
  ok(activeMediaBlocked(tab1), "Tab1 is activemedia-blocked");

  info("Press the Play Tab icon on tab0");
  await pressIcon(tab0.overlayIcon);

  // tab0 unblocked, tab1 blocked
  ok(!activeMediaBlocked(tab0), "Tab0 is not activemedia-blocked");
  ok(activeMediaBlocked(tab1), "Tab1 is activemedia-blocked");

  info("Press the Play Tab icon on tab1");
  await pressIcon(tab1.overlayIcon);

  // tab0 unblocked, tab1 unblocked
  ok(!activeMediaBlocked(tab0), "Tab0 is not activemedia-blocked");
  ok(!activeMediaBlocked(tab1), "Tab1 is not activemedia-blocked");

  BrowserTestUtils.removeTab(tab0);
  BrowserTestUtils.removeTab(tab1);
});

/*
 * Tab context menus have to show the menu icons "Play Tab" or "Play Tabs"
 * depending on the number of tabs selected, and whether blocked media is present
 */
add_task(async function testTabContextMenu() {
  info("Add media tab");
  let tab0 = await addMediaTab();

  let menuItemPlayTab = document.getElementById("context_playTab");
  let menuItemPlaySelectedTabs = document.getElementById(
    "context_playSelectedTabs"
  );

  // No active media yet:
  // - "Play Tab" is hidden
  // - "Play Tabs" is hidden
  updateTabContextMenu(tab0);
  ok(menuItemPlayTab.hidden, 'tab0 "Play Tab" is hidden');
  ok(menuItemPlaySelectedTabs.hidden, 'tab0 "Play Tabs" is hidden');
  ok(!activeMediaBlocked(tab0), "tab0 is not active media blocked");

  info("Play tab0");
  await play(tab0, false);

  // Active media blocked:
  // - "Play Tab" is visible
  // - "Play Tabs" is hidden
  updateTabContextMenu(tab0);
  ok(!menuItemPlayTab.hidden, 'tab0 "Play Tab" is visible');
  ok(menuItemPlaySelectedTabs.hidden, 'tab0 "Play Tabs" is hidden');
  ok(activeMediaBlocked(tab0), "tab0 is active media blocked");

  info("Play media on tab0");
  await playMedia(tab0, { expectBlocked: false });

  // Media is playing:
  // - "Play Tab" is hidden
  // - "Play Tabs" is hidden
  updateTabContextMenu(tab0);
  ok(menuItemPlayTab.hidden, 'tab0 "Play Tab" is hidden');
  ok(menuItemPlaySelectedTabs.hidden, 'tab0 "Play Tabs" is hidden');
  ok(!activeMediaBlocked(tab0), "tab0 is not active media blocked");

  BrowserTestUtils.removeTab(tab0);
});
