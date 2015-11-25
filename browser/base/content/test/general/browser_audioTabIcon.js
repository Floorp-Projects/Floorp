const PAGE = "https://example.com/browser/browser/base/content/test/general/file_mediaPlayback.html";

function* wait_for_tab_playing_event(tab, expectPlaying) {
  yield BrowserTestUtils.waitForEvent(tab, "TabAttrModified", false, (event) => {
    if (event.detail.changed.indexOf("soundplaying") >= 0) {
      is(tab.hasAttribute("soundplaying"), expectPlaying, "The tab should " + (expectPlaying ? "" : "not ") + "be playing");
      return true;
    }
    return false;
  });
}

function* play(tab) {
  let browser = tab.linkedBrowser;
  yield ContentTask.spawn(browser, {}, function* () {
    let audio = content.document.querySelector("audio");
    audio.play();
  });

  yield wait_for_tab_playing_event(tab, true);
}

function* pause(tab) {
  let browser = tab.linkedBrowser;
  yield ContentTask.spawn(browser, {}, function* () {
    let audio = content.document.querySelector("audio");
    audio.pause();
  });

  yield wait_for_tab_playing_event(tab, false);
}

function disable_non_test_mouse(disable) {
  let utils = window.QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIDOMWindowUtils);
  utils.disableNonTestMouseEvents(disable);
}

function* hover_icon(icon, tooltip) {
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

function* test_tooltip(icon, expectedTooltip, isActiveTab) {
  let tooltip = document.getElementById("tabbrowser-tab-tooltip");

  yield hover_icon(icon, tooltip);
  if (isActiveTab) {
    // The active tab should have the keybinding shortcut in the tooltip.
    // We check this by ensuring that the strings are not equal but the expected
    // message appears in the beginning.
    isnot(tooltip.getAttribute("label"), expectedTooltip, "Tooltips should not be equal");
    is(tooltip.getAttribute("label").indexOf(expectedTooltip), 0, "Correct tooltip expected");
  } else {
    is(tooltip.getAttribute("label"), expectedTooltip, "Tooltips should not be equal");
  }
  leave_icon(icon);
}

function get_wait_for_mute_promise(tab, expectMuted) {
  return BrowserTestUtils.waitForEvent(tab, "TabAttrModified", false, event => {
    if (event.detail.changed.indexOf("muted") >= 0) {
      is(tab.hasAttribute("muted"), expectMuted, "The tab should " + (expectMuted ? "" : "not ") + "be muted");
      return true;
    }
    return false;
  });
}

function* test_mute_tab(tab, icon, expectMuted) {
  let mutedPromise = test_mute_keybinding(tab, expectMuted);

  let activeTab = gBrowser.selectedTab;

  let tooltip = document.getElementById("tabbrowser-tab-tooltip");

  yield hover_icon(icon, tooltip);
  EventUtils.synthesizeMouseAtCenter(icon, {button: 0});
  leave_icon(icon);

  is(gBrowser.selectedTab, activeTab, "Clicking on mute should not change the currently selected tab");

  return mutedPromise;
}

function get_tab_state(tab) {
  const ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);
  return JSON.parse(ss.getTabState(tab));
}

function* test_muting_using_menu(tab, expectMuted) {
  // Show the popup menu
  let contextMenu = document.getElementById("tabContextMenu");
  let popupShownPromise = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(tab, {type: "contextmenu", button: 2});
  yield popupShownPromise;

  // Check the menu
  let expectedLabel = expectMuted ? "Unmute Tab" : "Mute Tab";
  let toggleMute = document.getElementById("context_toggleMuteTab");
  is(toggleMute.label, expectedLabel, "Correct label expected");
  is(toggleMute.accessKey, "M", "Correct accessKey expected");

  is(toggleMute.hasAttribute("muted"), expectMuted, "Should have the correct state for the muted attribute");
  ok(!toggleMute.hasAttribute("soundplaying"), "Should not have the soundplaying attribute");

  yield play(tab);

  is(toggleMute.hasAttribute("muted"), expectMuted, "Should have the correct state for the muted attribute");
  ok(toggleMute.hasAttribute("soundplaying"), "Should have the soundplaying attribute");

  yield pause(tab);

  is(toggleMute.hasAttribute("muted"), expectMuted, "Should have the correct state for the muted attribute");
  ok(!toggleMute.hasAttribute("soundplaying"), "Should not have the soundplaying attribute");

  // Click on the menu and wait for the tab to be muted.
  let mutedPromise = get_wait_for_mute_promise(tab, !expectMuted);
  let popupHiddenPromise = BrowserTestUtils.waitForEvent(contextMenu, "popuphidden");
  EventUtils.synthesizeMouseAtCenter(toggleMute, {});
  yield popupHiddenPromise;
  yield mutedPromise;
}

function* test_playing_icon_on_tab(tab, browser, isPinned) {
  let icon = document.getAnonymousElementByAttribute(tab, "anonid",
                                                     isPinned ? "overlay-icon" : "soundplaying-icon");
  let isActiveTab = tab === gBrowser.selectedTab;

  yield play(tab);

  yield test_tooltip(icon, "Mute tab", isActiveTab);

  ok(!("muted" in get_tab_state(tab)), "No muted attribute should be persisted");

  yield test_mute_tab(tab, icon, true);

  ok("muted" in get_tab_state(tab), "Muted attribute should be persisted");

  yield test_tooltip(icon, "Unmute tab", isActiveTab);

  yield test_mute_tab(tab, icon, false);

  ok(!("muted" in get_tab_state(tab)), "No muted attribute should be persisted");

  yield test_tooltip(icon, "Mute tab", isActiveTab);

  yield test_mute_tab(tab, icon, true);

  yield pause(tab);

  ok(tab.hasAttribute("muted") &&
     !tab.hasAttribute("soundplaying"), "Tab should still be muted but not playing");

  yield test_tooltip(icon, "Unmute tab", isActiveTab);

  yield test_mute_tab(tab, icon, false);

  ok(!tab.hasAttribute("muted") &&
     !tab.hasAttribute("soundplaying"), "Tab should not be be muted or playing");

  // Make sure it's possible to mute using the context menu.
  yield test_muting_using_menu(tab, false);

  // Make sure it's possible to unmute using the context menu.
  yield test_muting_using_menu(tab, true);
}

function* test_swapped_browser(oldTab, newBrowser, isPlaying) {
  ok(oldTab.hasAttribute("muted"), "Expected the correct muted attribute on the old tab");
  is(oldTab.hasAttribute("soundplaying"), isPlaying, "Expected the correct soundplaying attribute on the old tab");

  let newTab = gBrowser.getTabForBrowser(newBrowser);
  let AttrChangePromise = BrowserTestUtils.waitForEvent(newTab, "TabAttrModified", false, event => {
    return (event.detail.changed.indexOf("soundplaying") >= 0 || !isPlaying) &&
           event.detail.changed.indexOf("muted") >= 0;
  });
  let AudioPlaybackPromise = new Promise(resolve => {
    let observer = (subject, topic, data) => {
      ok(true, "Should see an audio-playback notification");
    };
    Services.obs.addObserver(observer, "audio-playback", false);
    setTimeout(() => {
      Services.obs.removeObserver(observer, "audio-playback");
      resolve();
    }, 100);
  });

  gBrowser.swapBrowsersAndCloseOther(newTab, oldTab);
  yield AttrChangePromise;

  ok(newTab.hasAttribute("muted"), "Expected the correct muted attribute on the new tab");
  is(newTab.hasAttribute("soundplaying"), isPlaying, "Expected the correct soundplaying attribute on the new tab");

  // Wait to see if an audio-playback event is dispatched.
  yield AudioPlaybackPromise;

  ok(newTab.hasAttribute("muted"), "Expected the correct muted attribute on the new tab");
  is(newTab.hasAttribute("soundplaying"), isPlaying, "Expected the correct soundplaying attribute on the new tab");

  let icon = document.getAnonymousElementByAttribute(newTab, "anonid",
                                                     "soundplaying-icon");
  yield test_tooltip(icon, "Unmute tab", true);
}

function* test_browser_swapping(tab, browser) {
  // First, test swapping with a playing but muted tab.
  yield play(tab);

  let icon = document.getAnonymousElementByAttribute(tab, "anonid",
                                                     "soundplaying-icon");
  yield test_mute_tab(tab, icon, true);

  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: "about:blank",
  }, function*(newBrowser) {
    yield test_swapped_browser(tab, newBrowser, true)

    // Now, test swapping with a muted but not playing tab.
    // Note that the tab remains muted, so we only need to pause playback.
    tab = gBrowser.getTabForBrowser(newBrowser);
    yield pause(tab);

    yield BrowserTestUtils.withNewTab({
      gBrowser,
      url: "about:blank",
    }, newBrowser => test_swapped_browser(tab, newBrowser, false));
  });
}

function* test_click_on_pinned_tab_after_mute() {
  function* test_on_browser(browser) {
    let tab = gBrowser.getTabForBrowser(browser);

    gBrowser.selectedTab = originallySelectedTab;
    isnot(tab, gBrowser.selectedTab, "Sanity check, the tab should not be selected!");

    // Steps to reproduce the bug:
    //   Pin the tab.
    gBrowser.pinTab(tab);

    //   Start playback and wait for it to finish.
    yield play(tab);

    //   Mute the tab.
    let icon = document.getAnonymousElementByAttribute(tab, "anonid", "overlay-icon");
    yield test_mute_tab(tab, icon, true);

    // Pause playback and wait for it to finish.
    yield pause(tab);

    // Unmute tab.
    yield test_mute_tab(tab, icon, false);

    // Now click on the tab.
    let image = document.getAnonymousElementByAttribute(tab, "anonid", "tab-icon-image");
    EventUtils.synthesizeMouseAtCenter(image, {button: 0});

    is(tab, gBrowser.selectedTab, "Tab switch should be successful");

    // Cleanup.
    gBrowser.unpinTab(tab);
    gBrowser.selectedTab = originallySelectedTab;
  }

  let originallySelectedTab = gBrowser.selectedTab;

  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: PAGE
  }, test_on_browser);
}

// This test only does something useful in e10s!
function* test_cross_process_load() {
  function* test_on_browser(browser) {
    let tab = gBrowser.getTabForBrowser(browser);

    //   Start playback and wait for it to finish.
    yield play(tab);

    let soundPlayingStoppedPromise = BrowserTestUtils.waitForEvent(tab, "TabAttrModified", false,
      event => event.detail.changed.indexOf("soundplaying") >= 0
    );

    // Go to a different process.
    browser.loadURI("about:");
    yield BrowserTestUtils.browserLoaded(browser);

    yield soundPlayingStoppedPromise;

    ok(!tab.hasAttribute("soundplaying"), "Tab should not be playing sound any more");
  }

  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: PAGE
  }, test_on_browser);
}

function* test_mute_keybinding() {
  function* test_muting_using_keyboard(tab) {
    let mutedPromise = get_wait_for_mute_promise(tab, true);
    EventUtils.synthesizeKey("m", {ctrlKey: true});
    yield mutedPromise;
    mutedPromise = get_wait_for_mute_promise(tab, false);
    EventUtils.synthesizeKey("m", {ctrlKey: true});
    yield mutedPromise;
  }
  function* test_on_browser(browser) {
    let tab = gBrowser.getTabForBrowser(browser);

    // Make sure it's possible to mute before the tab is playing.
    yield test_muting_using_keyboard(tab);

    //   Start playback and wait for it to finish.
    yield play(tab);

    // Make sure it's possible to mute after the tab is playing.
    yield test_muting_using_keyboard(tab);

    // Pause playback and wait for it to finish.
    yield pause(tab);

    // Make sure things work if the tab is pinned.
    gBrowser.pinTab(tab);

    // Make sure it's possible to mute before the tab is playing.
    yield test_muting_using_keyboard(tab);

    //   Start playback and wait for it to finish.
    yield play(tab);

    // Make sure it's possible to mute after the tab is playing.
    yield test_muting_using_keyboard(tab);

    gBrowser.unpinTab(tab);
  }

  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: PAGE
  }, test_on_browser);
}

function* test_on_browser(browser) {
  let tab = gBrowser.getTabForBrowser(browser);

  // Test the icon in a normal tab.
  yield test_playing_icon_on_tab(tab, browser, false);

  gBrowser.pinTab(tab);

  // Test the icon in a pinned tab.
  yield test_playing_icon_on_tab(tab, browser, true);

  gBrowser.unpinTab(tab);

  // Retest with another browser in the foreground tab
  if (gBrowser.selectedBrowser.currentURI.spec == PAGE) {
    yield BrowserTestUtils.withNewTab({
      gBrowser,
      url: "data:text/html,test"
    }, () => test_on_browser(browser));
  } else {
    yield test_browser_swapping(tab, browser);
  }
}

add_task(function*() {
  yield new Promise((resolve) => {
    SpecialPowers.pushPrefEnv({"set": [
                                ["browser.tabs.showAudioPlayingIcon", true],
                              ]}, resolve);
  });
});

requestLongerTimeout(2);
add_task(function* test_page() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: PAGE
  }, test_on_browser);
});

add_task(test_click_on_pinned_tab_after_mute);

add_task(test_cross_process_load);

add_task(test_mute_keybinding);
