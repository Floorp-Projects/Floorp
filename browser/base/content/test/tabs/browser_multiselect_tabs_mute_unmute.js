const PREF_DELAY_AUTOPLAY = "media.block-autoplay-until-in-foreground";

add_task(async function setPref() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_DELAY_AUTOPLAY, true]],
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
  await play(tab1, false);
  await play(tab2, false);

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
  let tab0MuteAudioBtn = tab0.overlayIcon;
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
  ok(!muted(tab1), "Tab1 is not muted");
  ok(!activeMediaBlocked(tab1), "Tab1 is not activemedia-blocked");
  ok(activeMediaBlocked(tab2), "Tab2 is media-blocked");
  ok(!muted(tab3), "Tab3 is not muted");
  ok(!activeMediaBlocked(tab3), "Tab3 is not activemedia-blocked");
  ok(!muted(tab4), "Tab4 is not muted");
  ok(!activeMediaBlocked(tab4), "Tab4 is not activemedia-blocked");

  // Mute tab1 which is multiselected, thus other multiselected tabs should be affected too
  // in the following way:
  //  a) muted tabs (tab0) will remain muted.
  //  b) unmuted tabs (tab1, tab3) will become muted.
  //  b) media-blocked tabs (tab2) will remain media-blocked.
  // However tab4 (unmuted) which is not multiselected should not be affected.
  let tab1MuteAudioBtn = tab1.overlayIcon;
  await test_mute_tab(tab1, tab1MuteAudioBtn, true);

  // Check mute state
  ok(muted(tab0), "Tab0 is still muted");
  ok(muted(tab1), "Tab1 is muted");
  ok(activeMediaBlocked(tab2), "Tab2 is still media-blocked");
  ok(muted(tab3), "Tab3 is now muted");
  ok(!muted(tab4), "Tab4 is not muted");
  ok(!activeMediaBlocked(tab4), "Tab4 is not activemedia-blocked");

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
  await play(tab1, false);
  await play(tab2, false);

  // Mute tab3 and tab4
  await toggleMuteAudio(tab3, true);
  await toggleMuteAudio(tab4, true);

  // Multiselecting tab0, tab1, tab2 and tab3
  await triggerClickOn(tab3, { shiftKey: true });

  // Check multiselection
  for (let i = 0; i <= 3; i++) {
    ok(tabs[i].multiselected, "tab" + i + " is multiselected");
  }
  ok(!tab4.multiselected, "tab4 is not multiselected");

  // Check tabs mute state
  ok(!muted(tab0), "Tab0 is not muted");
  ok(!activeMediaBlocked(tab0), "Tab0 is not activemedia-blocked");
  ok(activeMediaBlocked(tab1), "Tab1 is media-blocked");
  ok(activeMediaBlocked(tab2), "Tab2 is media-blocked");
  ok(muted(tab3), "Tab3 is muted");
  ok(muted(tab4), "Tab4 is muted");
  is(gBrowser.selectedTab, tab0, "Tab0 is active");

  // unmute tab0 which is multiselected, thus other multiselected tabs should be affected too
  // in the following way:
  //  a) muted tabs (tab3) will become unmuted.
  //  b) unmuted tabs (tab0) will remain unmuted.
  //  c) media-blocked tabs (tab1, tab2) will remain blocked.
  // However tab4 (muted) which is not multiselected should not be affected.
  let tab3MuteAudioBtn = tab3.overlayIcon;
  await test_mute_tab(tab3, tab3MuteAudioBtn, false);

  ok(!muted(tab0), "Tab0 is not muted");
  ok(!activeMediaBlocked(tab0), "Tab0 is not activemedia-blocked");
  ok(!muted(tab1), "Tab1 is not muted");
  ok(activeMediaBlocked(tab1), "Tab1 is activemedia-blocked");
  ok(!muted(tab2), "Tab2 is not muted");
  ok(activeMediaBlocked(tab2), "Tab2 is activemedia-blocked");
  ok(!muted(tab3), "Tab3 is not muted");
  ok(!activeMediaBlocked(tab3), "Tab3 is not activemedia-blocked");
  ok(muted(tab4), "Tab4 is muted");
  is(gBrowser.selectedTab, tab0, "Tab0 is active");

  for (let tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }
});

add_task(async function muteAndUnmuteTabs_usingKeyboard() {
  let tab0 = await addMediaTab();
  let tab1 = await addMediaTab();
  let tab2 = await addMediaTab();
  let tab3 = await addMediaTab();
  let tab4 = await addMediaTab();

  let tabs = [tab0, tab1, tab2, tab3, tab4];

  await BrowserTestUtils.switchTab(gBrowser, tab0);

  let mutedPromise = get_wait_for_mute_promise(tab0, true);
  EventUtils.synthesizeKey("M", { ctrlKey: true });
  await mutedPromise;
  ok(muted(tab0), "Tab0 should be muted");
  ok(!muted(tab1), "Tab1 should not be muted");
  ok(!muted(tab2), "Tab2 should not be muted");
  ok(!muted(tab3), "Tab3 should not be muted");
  ok(!muted(tab4), "Tab4 should not be muted");

  // Multiselecting tab0, tab1, tab2 and tab3
  await triggerClickOn(tab3, { shiftKey: true });

  // Check multiselection
  for (let i = 0; i <= 3; i++) {
    ok(tabs[i].multiselected, "tab" + i + " is multiselected");
  }
  ok(!tab4.multiselected, "tab4 is not multiselected");

  mutedPromise = get_wait_for_mute_promise(tab0, false);
  EventUtils.synthesizeKey("M", { ctrlKey: true });
  await mutedPromise;
  ok(!muted(tab0), "Tab0 should not be muted");
  ok(!muted(tab1), "Tab1 should not be muted");
  ok(!muted(tab2), "Tab2 should not be muted");
  ok(!muted(tab3), "Tab3 should not be muted");
  ok(!muted(tab4), "Tab4 should not be muted");

  mutedPromise = get_wait_for_mute_promise(tab0, true);
  EventUtils.synthesizeKey("M", { ctrlKey: true });
  await mutedPromise;
  ok(muted(tab0), "Tab0 should be muted");
  ok(muted(tab1), "Tab1 should be muted");
  ok(muted(tab2), "Tab2 should be muted");
  ok(muted(tab3), "Tab3 should be muted");
  ok(!muted(tab4), "Tab4 should not be muted");

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
  await play(tab1, false);
  await play(tab2, false);

  // Multiselecting tab0, tab1, tab2 and tab3.
  await triggerClickOn(tab3, { shiftKey: true });

  // Mute tab0 and tab4
  await toggleMuteAudio(tab0, true);
  await toggleMuteAudio(tab4, true);

  // Check multiselection
  for (let i = 0; i <= 3; i++) {
    ok(tabs[i].multiselected, "tab" + i + " is multiselected");
  }
  ok(!tab4.multiselected, "tab4 is not multiselected");

  // Check mute state
  ok(muted(tab0), "Tab0 is muted");
  ok(activeMediaBlocked(tab1), "Tab1 is media-blocked");
  ok(activeMediaBlocked(tab2), "Tab2 is media-blocked");
  ok(!muted(tab3), "Tab3 is not muted");
  ok(!activeMediaBlocked(tab3), "Tab3 is not activemedia-blocked");
  ok(muted(tab4), "Tab4 is muted");
  is(gBrowser.selectedTab, tab0, "Tab0 is active");

  // play tab2 which is multiselected, thus other multiselected tabs should be affected too
  // in the following way:
  //  a) muted tabs (tab0) will remain muted.
  //  b) unmuted tabs (tab3) will remain unmuted.
  //  c) media-blocked tabs (tab1, tab2) will become unblocked.
  // However tab4 (muted) which is not multiselected should not be affected.
  let tab2MuteAudioBtn = tab2.overlayIcon;
  await test_mute_tab(tab2, tab2MuteAudioBtn, false);

  ok(muted(tab0), "Tab0 is muted");
  ok(!activeMediaBlocked(tab0), "Tab0 is not activemedia-blocked");
  ok(!muted(tab1), "Tab1 is not muted");
  ok(!activeMediaBlocked(tab1), "Tab1 is not activemedia-blocked");
  ok(!muted(tab2), "Tab2 is not muted");
  ok(!activeMediaBlocked(tab2), "Tab2 is not activemedia-blocked");
  ok(!muted(tab3), "Tab3 is not muted");
  ok(!activeMediaBlocked(tab3), "Tab3 is not activemedia-blocked");
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
  let menuItemToggleMuteSelectedTabs = document.getElementById(
    "context_toggleMuteSelectedTabs"
  );

  await play(tab0, false);
  await toggleMuteAudio(tab0, true);
  await play(tab1, false);
  await toggleMuteAudio(tab2, true);

  // multiselect tab0, tab1, tab2.
  await triggerClickOn(tab0, { ctrlKey: true });
  await triggerClickOn(tab1, { ctrlKey: true });
  await triggerClickOn(tab2, { ctrlKey: true });

  // Check multiselected tabs
  for (let i = 0; i <= 2; i++) {
    ok(tabs[i].multiselected, "Tab" + i + " is multi-selected");
  }
  ok(!tab3.multiselected, "Tab3 is not multiselected");

  // Check mute state for tabs
  ok(muted(tab0), "Tab0 is muted");
  ok(activeMediaBlocked(tab0), "Tab0 is activemedia-blocked");
  ok(activeMediaBlocked(tab1), "Tab1 is activemedia-blocked");
  ok(muted(tab2), "Tab2 is muted");
  ok(!muted(tab3, "Tab3 is not muted"));

  const l10nIds = [
    "tabbrowser-context-unmute-selected-tabs",
    "tabbrowser-context-mute-selected-tabs",
    "tabbrowser-context-unmute-selected-tabs",
  ];

  for (let i = 0; i <= 2; i++) {
    updateTabContextMenu(tabs[i]);
    ok(
      menuItemToggleMuteTab.hidden,
      "toggleMuteAudio menu for one tab is hidden - contextTab" + i
    );
    ok(
      !menuItemToggleMuteSelectedTabs.hidden,
      "toggleMuteAudio menu for selected tab is not hidden - contextTab" + i
    );
    is(
      menuItemToggleMuteSelectedTabs.dataset.l10nId,
      l10nIds[i],
      l10nIds[i] + " should be shown"
    );
  }

  updateTabContextMenu(tab3);
  ok(
    !menuItemToggleMuteTab.hidden,
    "toggleMuteAudio menu for one tab is not hidden"
  );
  ok(
    menuItemToggleMuteSelectedTabs.hidden,
    "toggleMuteAudio menu for selected tab is hidden"
  );

  for (let tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }
});
