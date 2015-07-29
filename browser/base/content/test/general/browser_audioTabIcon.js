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
  is(tooltip.getAttribute("label"), expectedTooltip, "Correct tooltip expected");
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

  yield test_tooltip(icon, "This tab is playing audio");

  yield test_mute_tab(tab, icon, true);

  yield test_tooltip(icon, "This tab has been muted");

  yield test_mute_tab(tab, icon, false);

  yield test_tooltip(icon, "This tab is playing audio");

  yield ContentTask.spawn(browser, {}, function* () {
    let audio = content.document.querySelector("audio");
    audio.pause();
  });
  yield wait_for_tab_playing_event(tab, false);
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
