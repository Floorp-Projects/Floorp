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

function* test_tooltip(icon, expectedTooltip) {
  let tooltip = document.getElementById("tabbrowser-tab-tooltip");

  yield hover_icon(icon, tooltip);
  is(tooltip.getAttribute("label").indexOf(expectedTooltip), 0, "Correct tooltip expected");
  leave_icon(icon);
}

function* test_mute_tab(tab, icon, expectMuted) {
  let mutedPromise = BrowserTestUtils.waitForEvent(tab, "TabAttrModified", false, (event) => {
    if (event.detail.changed.indexOf("muted") >= 0) {
      is(tab.hasAttribute("muted"), expectMuted, "The tab should " + (expectMuted ? "" : "not ") + "be muted");
      return true;
    }
    return false;
  });

  let activeTab = gBrowser.selectedTab;

  let tooltip = document.getElementById("tabbrowser-tab-tooltip");

  yield hover_icon(icon, tooltip);
  EventUtils.synthesizeMouseAtCenter(icon, {button: 0});
  leave_icon(icon);

  is(gBrowser.selectedTab, activeTab, "Clicking on mute should not change the currently selected tab");

  return mutedPromise;
}

function* test_playing_icon_on_tab(tab, browser, isPinned) {
  let icon = document.getAnonymousElementByAttribute(tab, "anonid",
                                                     isPinned ? "overlay-icon" : "soundplaying-icon");

  yield ContentTask.spawn(browser, {}, function* () {
    let audio = content.document.querySelector("audio");
    audio.play();
  });

  yield wait_for_tab_playing_event(tab, true);

  yield test_tooltip(icon, "Mute tab");

  yield test_mute_tab(tab, icon, true);

  yield test_tooltip(icon, "Unmute tab");

  yield test_mute_tab(tab, icon, false);

  yield test_tooltip(icon, "Mute tab");

  yield test_mute_tab(tab, icon, true);

  yield ContentTask.spawn(browser, {}, function* () {
    let audio = content.document.querySelector("audio");
    audio.pause();
  });
  yield wait_for_tab_playing_event(tab, false);

  ok(tab.hasAttribute("muted") &&
     !tab.hasAttribute("soundplaying"), "Tab should still be muted but not playing");

  yield test_tooltip(icon, "Unmute tab");

  yield test_mute_tab(tab, icon, false);

  ok(!tab.hasAttribute("muted") &&
     !tab.hasAttribute("soundplaying"), "Tab should not be be muted or playing");
}

function* test_swapped_browser(oldTab, newBrowser, isPlaying) {
  ok(oldTab.hasAttribute("muted"), "Expected the correct muted attribute on the old tab");
  is(oldTab.hasAttribute("soundplaying"), isPlaying, "Expected the correct soundplaying attribute on the old tab");

  let newTab = gBrowser.getTabForBrowser(newBrowser);
  let AttrChangePromise = BrowserTestUtils.waitForEvent(newTab, "TabAttrModified", false, event => {
    return (event.detail.changed.indexOf("soundplaying") >= 0 || !isPlaying) &&
           event.detail.changed.indexOf("muted") >= 0;
  });

  gBrowser.swapBrowsersAndCloseOther(newTab, oldTab);
  yield AttrChangePromise;

  ok(newTab.hasAttribute("muted"), "Expected the correct muted attribute on the new tab");
  is(newTab.hasAttribute("soundplaying"), isPlaying, "Expected the correct soundplaying attribute on the new tab");
}

function* test_browser_swapping(tab, browser) {
  // First, test swapping with a playing but muted tab.
  yield ContentTask.spawn(browser, {}, function* () {
    let audio = content.document.querySelector("audio");
    audio.play();
  });
  yield wait_for_tab_playing_event(tab, true);

  let icon = document.getAnonymousElementByAttribute(tab, "anonid",
                                                     "soundplaying-icon");
  yield test_mute_tab(tab, icon, true);

  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: "about:blank",
  }, function*(newBrowser) {
    yield test_swapped_browser(tab, newBrowser, true)

    // FIXME: this is needed to work around bug 1190903.
    yield new Promise(resolve => setTimeout(resolve, 1000));

    // Now, test swapping with a muted but not playing tab.
    // Note that the tab remains muted, so we only need to pause playback.
    tab = gBrowser.getTabForBrowser(newBrowser);
    yield ContentTask.spawn(newBrowser, {}, function* () {
      let audio = content.document.querySelector("audio");
      audio.pause();
    });

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

    //   Start playbak.
    yield ContentTask.spawn(browser, {}, function* () {
      let audio = content.document.querySelector("audio");
      audio.play();
    });

    //   Wait for playback to start.
    yield wait_for_tab_playing_event(tab, true);

    //   Mute the tab.
    let icon = document.getAnonymousElementByAttribute(tab, "anonid", "overlay-icon");
    yield test_mute_tab(tab, icon, true);

    //   Stop playback
    yield ContentTask.spawn(browser, {}, function* () {
      let audio = content.document.querySelector("audio");
      audio.pause();
    });

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

    yield test_click_on_pinned_tab_after_mute();
  }
}

add_task(function*() {
  yield new Promise((resolve) => {
    SpecialPowers.pushPrefEnv({"set": [
                                ["media.useAudioChannelService", true],
                                ["browser.tabs.showAudioPlayingIcon", true],
                              ]}, resolve);
  });
});

add_task(function* test_page() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: PAGE
  }, test_on_browser);
});
