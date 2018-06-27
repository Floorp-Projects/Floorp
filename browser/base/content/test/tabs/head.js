function updateTabContextMenu(tab) {
  let menu = document.getElementById("tabContextMenu");
  if (!tab)
    tab = gBrowser.selectedTab;
  var evt = new Event("");
  tab.dispatchEvent(evt);
  menu.openPopup(tab, "end_after", 0, 0, true, false, evt);
  is(TabContextMenu.contextTab, tab, "TabContextMenu context is the expected tab");
  menu.hidePopup();
}

function triggerClickOn(target, options) {
  let promise = BrowserTestUtils.waitForEvent(target, "click");
  if (AppConstants.platform == "macosx") {
      options = {
          metaKey: options.ctrlKey,
          shiftKey: options.shiftKey
      };
  }
  EventUtils.synthesizeMouseAtCenter(target, options);
  return promise;
}

async function addTab() {
  const tab = BrowserTestUtils.addTab(gBrowser,
      "http://mochi.test:8888/", { skipAnimation: true });
  const browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);
  return tab;
}

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

async function wait_for_tab_media_blocked_event(tab, expectMediaBlocked) {
  if (tab.activeMediaBlocked == expectMediaBlocked) {
    ok(true, "The tab should " + (expectMediaBlocked ? "" : "not ") + "be activemedia-blocked");
    return true;
  }
  return BrowserTestUtils.waitForEvent(tab, "TabAttrModified", false, (event) => {
    if (event.detail.changed.includes("activemedia-blocked")) {
      is(tab.hasAttribute("activemedia-blocked"), expectMediaBlocked, "The tab should " + (expectMediaBlocked ? "" : "not ") + "be activemedia-blocked");
      is(tab.activeMediaBlocked, expectMediaBlocked, "The tab should " + (expectMediaBlocked ? "" : "not ") + "be activemedia-blocked");
      return true;
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

async function play(tab, expectPlaying = true) {
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

  if (expectPlaying) {
    await wait_for_tab_playing_event(tab, true);
  } else {
    await wait_for_tab_media_blocked_event(tab, true);
  }
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
  let mutedPromise = get_wait_for_mute_promise(tab, expectMuted);

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
