/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Ensure multiselected tabs that are active media blocked act correctly
 * when we try to unblock them using the "Play Tabs" icon or by calling
 * resumeDelayedMediaOnMultiSelectedTabs()
 */

"use strict";

const PREF_DELAY_AUTOPLAY = "media.block-autoplay-until-in-foreground";

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

  info("Play both tabs");
  await play(tab0, false);
  await play(tab1, false);

  info("Multiselect tabs");
  await triggerClickOn(tab1, { shiftKey: true });

  // Check multiselection
  ok(tab0.multiselected, "tab0 is multiselected");
  ok(tab1.multiselected, "tab1 is multiselected");

  // Check tabs are unmuted
  ok(!muted(tab0), "Tab0 is unmuted");
  ok(!muted(tab1), "Tab1 is unmuted");

  let tab0BlockPromise = wait_for_tab_media_blocked_event(tab0, false);
  let tab1BlockPromise = wait_for_tab_media_blocked_event(tab1, false);

  // Play media on selected tabs
  gBrowser.resumeDelayedMediaOnMultiSelectedTabs();

  info("Wait for media to play");
  await tab0BlockPromise;
  await tab1BlockPromise;

  // Check tabs are still unmuted
  ok(!muted(tab0), "Tab0 is unmuted");
  ok(!muted(tab1), "Tab1 is unmuted");

  BrowserTestUtils.removeTab(tab0);
  BrowserTestUtils.removeTab(tab1);
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

  info("Multiselect tabs");
  await triggerClickOn(tab1, { shiftKey: true });

  // Check multiselection
  ok(tab0.multiselected, "tab0 is multiselected");
  ok(tab1.multiselected, "tab1 is multiselected");

  // Check tabs are muted
  ok(muted(tab0), "Tab0 is muted");
  ok(muted(tab1), "Tab1 is muted");

  let tab0BlockPromise = wait_for_tab_media_blocked_event(tab0, false);
  let tab1BlockPromise = wait_for_tab_media_blocked_event(tab1, false);

  // Play media on selected tabs
  gBrowser.resumeDelayedMediaOnMultiSelectedTabs();

  info("Wait for media to play");
  await tab0BlockPromise;
  await tab1BlockPromise;

  // Check tabs are still muted
  ok(muted(tab0), "Tab0 is muted");
  ok(muted(tab1), "Tab1 is muted");

  BrowserTestUtils.removeTab(tab0);
  BrowserTestUtils.removeTab(tab1);
});

/*
 * The "Play Tabs" icon unblocks media
 */
add_task(async function testDelayPlayWhenUsingButton() {
  info("Add media tabs");
  let tab0 = await addMediaTab();
  let tab1 = await addMediaTab();
  let tab2 = await addMediaTab();
  let tab3 = await addMediaTab();
  let tab4 = await addMediaTab();

  let tabs = [tab0, tab1, tab2, tab3, tab4];

  // All tabs are initially unblocked due to not being played yet
  ok(!activeMediaBlocked(tab0), "Tab0 is not activemedia-blocked");
  ok(!activeMediaBlocked(tab1), "Tab1 is not activemedia-blocked");
  ok(!activeMediaBlocked(tab2), "Tab2 is not activemedia-blocked");
  ok(!activeMediaBlocked(tab3), "Tab3 is not activemedia-blocked");
  ok(!activeMediaBlocked(tab4), "Tab4 is not activemedia-blocked");

  // Playing tabs 0, 1, and 2 will block them
  info("Play tabs 0, 1, and 2");
  await play(tab0, false);
  await play(tab1, false);
  await play(tab2, false);
  ok(activeMediaBlocked(tab0), "Tab0 is activemedia-blocked");
  ok(activeMediaBlocked(tab1), "Tab1 is activemedia-blocked");
  ok(activeMediaBlocked(tab2), "Tab2 is activemedia-blocked");

  // tab3 and tab4 are still unblocked
  ok(!activeMediaBlocked(tab3), "Tab3 is not activemedia-blocked");
  ok(!activeMediaBlocked(tab4), "Tab4 is not activemedia-blocked");

  // Multiselect tab0, tab1, tab2, and tab3.
  info("Multiselect tabs");
  await triggerClickOn(tab3, { shiftKey: true });

  // Check multiselection
  for (let i = 0; i <= 3; i++) {
    ok(tabs[i].multiselected, `tab${i} is multiselected`);
  }
  ok(!tab4.multiselected, "tab4 is not multiselected");

  let tab0BlockPromise = wait_for_tab_media_blocked_event(tab0, false);
  let tab1BlockPromise = wait_for_tab_media_blocked_event(tab1, false);
  let tab2BlockPromise = wait_for_tab_media_blocked_event(tab2, false);

  // Use the overlay icon on tab2 to play media on the selected tabs
  info("Press play tab2 icon");
  await pressIcon(tab2.overlayIcon);

  // tab0, tab1, and tab2 were played and multiselected
  // They will now be unblocked and playing media
  info("Wait for tabs to play");
  await tab0BlockPromise;
  await tab1BlockPromise;
  await tab2BlockPromise;
  ok(!activeMediaBlocked(tab0), "Tab0 is not activemedia-blocked");
  ok(!activeMediaBlocked(tab1), "Tab1 is not activemedia-blocked");
  ok(!activeMediaBlocked(tab2), "Tab2 is not activemedia-blocked");
  // tab3 was also multiselected but never played
  // It will be unblocked but not playing media
  ok(!activeMediaBlocked(tab3), "Tab3 is not activemedia-blocked");
  // tab4 was not multiselected and was never played
  // It remains in its original state
  ok(!activeMediaBlocked(tab4), "Tab4 is not activemedia-blocked");

  for (let tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }
});

/*
 * Tab context menus have to show the menu icons "Play Tab" or "Play Tabs"
 * depending on the number of tabs selected, and whether blocked media is present
 */
add_task(async function testTabContextMenu() {
  info("Add media tabs");
  let tab0 = await addMediaTab();
  let tab1 = await addMediaTab();
  let tab2 = await addMediaTab();
  let tab3 = await addMediaTab();
  let tabs = [tab0, tab1, tab2, tab3];

  let menuItemPlayTab = document.getElementById("context_playTab");
  let menuItemPlaySelectedTabs = document.getElementById(
    "context_playSelectedTabs"
  );

  // Multiselect tab0, tab1, and tab2.
  info("Multiselect tabs");
  await triggerClickOn(tab0, { ctrlKey: true });
  await triggerClickOn(tab1, { ctrlKey: true });
  await triggerClickOn(tab2, { ctrlKey: true });

  // Check multiselected tabs
  for (let i = 0; i <= 2; i++) {
    ok(tabs[i].multiselected, `tab${i} is multi-selected`);
  }
  ok(!tab3.multiselected, "tab3 is not multiselected");

  // No active media yet:
  // - "Play Tab" is hidden
  // - "Play Tabs" is hidden
  for (let i = 0; i <= 2; i++) {
    updateTabContextMenu(tabs[i]);
    ok(menuItemPlayTab.hidden, `tab${i} "Play Tab" is hidden`);
    ok(menuItemPlaySelectedTabs.hidden, `tab${i} "Play Tabs" is hidden`);
    ok(!activeMediaBlocked(tabs[i]), `tab${i} is not active media blocked`);
  }

  info("Play tabs 0, 1, and 2");
  await play(tab0, false);
  await play(tab1, false);
  await play(tab2, false);

  // Active media blocked:
  // - "Play Tab" is hidden
  // - "Play Tabs" is visible
  for (let i = 0; i <= 2; i++) {
    updateTabContextMenu(tabs[i]);
    ok(menuItemPlayTab.hidden, `tab${i} "Play Tab" is hidden`);
    ok(!menuItemPlaySelectedTabs.hidden, `tab${i} "Play Tabs" is visible`);
    ok(activeMediaBlocked(tabs[i]), `tab${i} is active media blocked`);
  }

  info("Play Media on tabs 0, 1, and 2");
  gBrowser.resumeDelayedMediaOnMultiSelectedTabs();

  // Active media is unblocked:
  // - "Play Tab" is hidden
  // - "Play Tabs" is hidden
  for (let i = 0; i <= 2; i++) {
    updateTabContextMenu(tabs[i]);
    ok(menuItemPlayTab.hidden, `tab${i} "Play Tab" is hidden`);
    ok(menuItemPlaySelectedTabs.hidden, `tab${i} "Play Tabs" is hidden`);
    ok(!activeMediaBlocked(tabs[i]), `tab${i} is not active media blocked`);
  }

  // tab3 is untouched
  updateTabContextMenu(tab3);
  ok(menuItemPlayTab.hidden, 'tab3 "Play Tab" is hidden');
  ok(menuItemPlaySelectedTabs.hidden, 'tab3 "Play Tabs" is hidden');
  ok(!activeMediaBlocked(tab3), "tab3 is not active media blocked");

  for (let tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }
});
