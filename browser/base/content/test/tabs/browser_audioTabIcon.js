/* eslint-disable mozilla/no-arbitrary-setTimeout */
const PAGE =
  "https://example.com/browser/browser/base/content/test/tabs/file_mediaPlayback.html";
const TABATTR_REMOVAL_PREFNAME = "browser.tabs.delayHidingAudioPlayingIconMS";
const INITIAL_TABATTR_REMOVAL_DELAY_MS = Services.prefs.getIntPref(
  TABATTR_REMOVAL_PREFNAME
);

async function pause(tab, options) {
  let extendedDelay = options && options.extendedDelay;
  if (extendedDelay) {
    // Use 10s to remove possibility of race condition with attr removal.
    Services.prefs.setIntPref(TABATTR_REMOVAL_PREFNAME, 10000);
  }

  try {
    let browser = tab.linkedBrowser;
    let awaitDOMAudioPlaybackStopped = BrowserTestUtils.waitForEvent(
      browser,
      "DOMAudioPlaybackStopped",
      "DOMAudioPlaybackStopped event should get fired after pause"
    );
    await SpecialPowers.spawn(browser, [], async function() {
      let audio = content.document.querySelector("audio");
      audio.pause();
    });

    // If the tab has already be muted, it means the tab won't have soundplaying,
    // so we don't need to check this attribute.
    if (browser.audioMuted) {
      return;
    }

    if (extendedDelay) {
      ok(
        tab.hasAttribute("soundplaying"),
        "The tab should still have the soundplaying attribute immediately after pausing"
      );

      await awaitDOMAudioPlaybackStopped;
      ok(
        tab.hasAttribute("soundplaying"),
        "The tab should still have the soundplaying attribute immediately after DOMAudioPlaybackStopped"
      );
    }

    await wait_for_tab_playing_event(tab, false);
    ok(
      !tab.hasAttribute("soundplaying"),
      "The tab should not have the soundplaying attribute after the timeout has resolved"
    );
  } finally {
    // Make sure other tests don't timeout if an exception gets thrown above.
    // Need to use setIntPref instead of clearUserPref because
    // testing/profiles/common/user.js overrides the default value to help this and
    // other tests run faster.
    Services.prefs.setIntPref(
      TABATTR_REMOVAL_PREFNAME,
      INITIAL_TABATTR_REMOVAL_DELAY_MS
    );
  }
}

async function hide_tab(tab) {
  let tabHidden = BrowserTestUtils.waitForEvent(tab, "TabHide");
  gBrowser.hideTab(tab);
  return tabHidden;
}

async function show_tab(tab) {
  let tabShown = BrowserTestUtils.waitForEvent(tab, "TabShow");
  gBrowser.showTab(tab);
  return tabShown;
}

async function test_tooltip(icon, expectedTooltip, isActiveTab) {
  let tooltip = document.getElementById("tabbrowser-tab-tooltip");

  await hover_icon(icon, tooltip);
  if (isActiveTab) {
    // The active tab should have the keybinding shortcut in the tooltip.
    // We check this by ensuring that the strings are not equal but the expected
    // message appears in the beginning.
    isnot(
      tooltip.getAttribute("label"),
      expectedTooltip,
      "Tooltips should not be equal"
    );
    is(
      tooltip.getAttribute("label").indexOf(expectedTooltip),
      0,
      "Correct tooltip expected"
    );
  } else {
    is(
      tooltip.getAttribute("label"),
      expectedTooltip,
      "Tooltips should not be equal"
    );
  }
  leave_icon(icon);
}

function get_tab_state(tab) {
  return JSON.parse(SessionStore.getTabState(tab));
}

async function test_muting_using_menu(tab, expectMuted) {
  // Show the popup menu
  let contextMenu = document.getElementById("tabContextMenu");
  let popupShownPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );
  EventUtils.synthesizeMouseAtCenter(tab, { type: "contextmenu", button: 2 });
  await popupShownPromise;

  // Check the menu
  let expectedLabel = expectMuted ? "Unmute Tab" : "Mute Tab";
  let expectedAccessKey = expectMuted ? "m" : "M";
  let toggleMute = document.getElementById("context_toggleMuteTab");
  is(toggleMute.label, expectedLabel, "Correct label expected");
  is(toggleMute.accessKey, expectedAccessKey, "Correct accessKey expected");

  is(
    toggleMute.hasAttribute("muted"),
    expectMuted,
    "Should have the correct state for the muted attribute"
  );
  ok(
    !toggleMute.hasAttribute("soundplaying"),
    "Should not have the soundplaying attribute"
  );

  await play(tab);

  is(
    toggleMute.hasAttribute("muted"),
    expectMuted,
    "Should have the correct state for the muted attribute"
  );
  is(
    !toggleMute.hasAttribute("soundplaying"),
    expectMuted,
    "The value of soundplaying attribute is incorrect"
  );

  await pause(tab);

  is(
    toggleMute.hasAttribute("muted"),
    expectMuted,
    "Should have the correct state for the muted attribute"
  );
  ok(
    !toggleMute.hasAttribute("soundplaying"),
    "Should not have the soundplaying attribute"
  );

  // Click on the menu and wait for the tab to be muted.
  let mutedPromise = get_wait_for_mute_promise(tab, !expectMuted);
  let popupHiddenPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popuphidden"
  );
  EventUtils.synthesizeMouseAtCenter(toggleMute, {});
  await popupHiddenPromise;
  await mutedPromise;
}

async function test_playing_icon_on_tab(tab, browser, isPinned) {
  let icon = isPinned ? tab.overlayIcon : tab.soundPlayingIcon;
  let isActiveTab = tab === gBrowser.selectedTab;

  await play(tab);

  await test_tooltip(icon, "Mute tab", isActiveTab);

  ok(
    !("muted" in get_tab_state(tab)),
    "No muted attribute should be persisted"
  );
  ok(
    !("muteReason" in get_tab_state(tab)),
    "No muteReason property should be persisted"
  );

  await test_mute_tab(tab, icon, true);

  ok("muted" in get_tab_state(tab), "Muted attribute should be persisted");
  ok(
    "muteReason" in get_tab_state(tab),
    "muteReason property should be persisted"
  );

  await test_tooltip(icon, "Unmute tab", isActiveTab);

  await test_mute_tab(tab, icon, false);

  ok(
    !("muted" in get_tab_state(tab)),
    "No muted attribute should be persisted"
  );
  ok(
    !("muteReason" in get_tab_state(tab)),
    "No muteReason property should be persisted"
  );

  await test_tooltip(icon, "Mute tab", isActiveTab);

  await test_mute_tab(tab, icon, true);

  await pause(tab);

  ok(
    tab.hasAttribute("muted") && !tab.hasAttribute("soundplaying"),
    "Tab should still be muted but not playing"
  );
  ok(
    tab.muted && !tab.soundPlaying,
    "Tab should still be muted but not playing"
  );

  await test_tooltip(icon, "Unmute tab", isActiveTab);

  await test_mute_tab(tab, icon, false);

  ok(
    !tab.hasAttribute("muted") && !tab.hasAttribute("soundplaying"),
    "Tab should not be be muted or playing"
  );
  ok(!tab.muted && !tab.soundPlaying, "Tab should not be be muted or playing");

  // Make sure it's possible to mute using the context menu.
  await test_muting_using_menu(tab, false);

  // Make sure it's possible to unmute using the context menu.
  await test_muting_using_menu(tab, true);
}

async function test_playing_icon_on_hidden_tab(tab) {
  let oldSelectedTab = gBrowser.selectedTab;
  let otherTabs = [
    await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE, true, true),
    await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE, true, true),
  ];
  let tabContainer = tab.container;
  let alltabsButton = document.getElementById("alltabs-button");
  let alltabsBadge = alltabsButton.badgeLabel;

  function assertIconShowing() {
    is(
      getComputedStyle(alltabsBadge).backgroundImage,
      'url("chrome://browser/skin/tabbrowser/badge-audio-playing.svg")',
      "The audio playing icon is shown"
    );
    is(
      tabContainer.getAttribute("hiddensoundplaying"),
      "true",
      "There are hidden audio tabs"
    );
  }

  function assertIconHidden() {
    is(
      getComputedStyle(alltabsBadge).backgroundImage,
      "none",
      "The audio playing icon is hidden"
    );
    ok(
      !tabContainer.hasAttribute("hiddensoundplaying"),
      "There are no hidden audio tabs"
    );
  }

  // Keep the passed in tab selected.
  gBrowser.selectedTab = tab;

  // Play sound in the other two (visible) tabs.
  await play(otherTabs[0]);
  await play(otherTabs[1]);
  assertIconHidden();

  // Hide one of the noisy tabs, we see the icon.
  await hide_tab(otherTabs[0]);
  assertIconShowing();

  // Hiding the other tab keeps the icon.
  await hide_tab(otherTabs[1]);
  assertIconShowing();

  // Pausing both tabs will hide the icon.
  await pause(otherTabs[0]);
  assertIconShowing();
  await pause(otherTabs[1]);
  assertIconHidden();

  // The icon returns when audio starts again.
  await play(otherTabs[0]);
  await play(otherTabs[1]);
  assertIconShowing();

  // There is still an icon after hiding one tab.
  await show_tab(otherTabs[0]);
  assertIconShowing();

  // The icon is hidden when both of the tabs are shown.
  await show_tab(otherTabs[1]);
  assertIconHidden();

  await BrowserTestUtils.removeTab(otherTabs[0]);
  await BrowserTestUtils.removeTab(otherTabs[1]);

  // Make sure we didn't change the selected tab.
  gBrowser.selectedTab = oldSelectedTab;
}

async function test_swapped_browser_while_playing(oldTab, newBrowser) {
  // The tab was muted so it won't have soundplaying attribute even it's playing.
  ok(
    oldTab.hasAttribute("muted"),
    "Expected the correct muted attribute on the old tab"
  );
  is(
    oldTab.muteReason,
    null,
    "Expected the correct muteReason attribute on the old tab"
  );
  ok(
    !oldTab.hasAttribute("soundplaying"),
    "Expected the correct soundplaying attribute on the old tab"
  );

  let newTab = gBrowser.getTabForBrowser(newBrowser);
  let AttrChangePromise = BrowserTestUtils.waitForEvent(
    newTab,
    "TabAttrModified",
    false,
    event => {
      return event.detail.changed.includes("muted");
    }
  );

  gBrowser.swapBrowsersAndCloseOther(newTab, oldTab);
  await AttrChangePromise;

  ok(
    newTab.hasAttribute("muted"),
    "Expected the correct muted attribute on the new tab"
  );
  is(
    newTab.muteReason,
    null,
    "Expected the correct muteReason property on the new tab"
  );
  ok(
    !newTab.hasAttribute("soundplaying"),
    "Expected the correct soundplaying attribute on the new tab"
  );

  await test_tooltip(newTab.soundPlayingIcon, "Unmute tab", true);
}

async function test_swapped_browser_while_not_playing(oldTab, newBrowser) {
  ok(
    oldTab.hasAttribute("muted"),
    "Expected the correct muted attribute on the old tab"
  );
  is(
    oldTab.muteReason,
    null,
    "Expected the correct muteReason property on the old tab"
  );
  ok(
    !oldTab.hasAttribute("soundplaying"),
    "Expected the correct soundplaying attribute on the old tab"
  );

  let newTab = gBrowser.getTabForBrowser(newBrowser);
  let AttrChangePromise = BrowserTestUtils.waitForEvent(
    newTab,
    "TabAttrModified",
    false,
    event => {
      return event.detail.changed.includes("muted");
    }
  );

  let AudioPlaybackPromise = new Promise(resolve => {
    let observer = (subject, topic, data) => {
      ok(false, "Should not see an audio-playback notification");
    };
    Services.obs.addObserver(observer, "audio-playback");
    setTimeout(() => {
      Services.obs.removeObserver(observer, "audio-playback");
      resolve();
    }, 100);
  });

  gBrowser.swapBrowsersAndCloseOther(newTab, oldTab);
  await AttrChangePromise;

  ok(
    newTab.hasAttribute("muted"),
    "Expected the correct muted attribute on the new tab"
  );
  is(
    newTab.muteReason,
    null,
    "Expected the correct muteReason property on the new tab"
  );
  ok(
    !newTab.hasAttribute("soundplaying"),
    "Expected the correct soundplaying attribute on the new tab"
  );

  // Wait to see if an audio-playback event is dispatched.
  await AudioPlaybackPromise;

  ok(
    newTab.hasAttribute("muted"),
    "Expected the correct muted attribute on the new tab"
  );
  is(
    newTab.muteReason,
    null,
    "Expected the correct muteReason property on the new tab"
  );
  ok(
    !newTab.hasAttribute("soundplaying"),
    "Expected the correct soundplaying attribute on the new tab"
  );

  await test_tooltip(newTab.soundPlayingIcon, "Unmute tab", true);
}

async function test_browser_swapping(tab, browser) {
  // First, test swapping with a playing but muted tab.
  await play(tab);

  await test_mute_tab(tab, tab.soundPlayingIcon, true);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:blank",
    },
    async function(newBrowser) {
      await test_swapped_browser_while_playing(tab, newBrowser);

      // Now, test swapping with a muted but not playing tab.
      // Note that the tab remains muted, so we only need to pause playback.
      tab = gBrowser.getTabForBrowser(newBrowser);
      await pause(tab);

      await BrowserTestUtils.withNewTab(
        {
          gBrowser,
          url: "about:blank",
        },
        secondAboutBlankBrowser =>
          test_swapped_browser_while_not_playing(tab, secondAboutBlankBrowser)
      );
    }
  );
}

async function test_click_on_pinned_tab_after_mute() {
  async function taskFn(browser) {
    let tab = gBrowser.getTabForBrowser(browser);

    gBrowser.selectedTab = originallySelectedTab;
    isnot(
      tab,
      gBrowser.selectedTab,
      "Sanity check, the tab should not be selected!"
    );

    // Steps to reproduce the bug:
    //   Pin the tab.
    gBrowser.pinTab(tab);

    //   Start playback and wait for it to finish.
    await play(tab);

    //   Mute the tab.
    let icon = tab.overlayIcon;
    await test_mute_tab(tab, icon, true);

    // Pause playback and wait for it to finish.
    await pause(tab);

    // Unmute tab.
    await test_mute_tab(tab, icon, false);

    // Now click on the tab.
    EventUtils.synthesizeMouseAtCenter(tab.iconImage, { button: 0 });

    is(tab, gBrowser.selectedTab, "Tab switch should be successful");

    // Cleanup.
    gBrowser.unpinTab(tab);
    gBrowser.selectedTab = originallySelectedTab;
  }

  let originallySelectedTab = gBrowser.selectedTab;

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: PAGE,
    },
    taskFn
  );
}

// This test only does something useful in e10s!
async function test_cross_process_load() {
  async function taskFn(browser) {
    let tab = gBrowser.getTabForBrowser(browser);

    //   Start playback and wait for it to finish.
    await play(tab);

    let soundPlayingStoppedPromise = BrowserTestUtils.waitForEvent(
      tab,
      "TabAttrModified",
      false,
      event => event.detail.changed.includes("soundplaying")
    );

    // Go to a different process.
    BrowserTestUtils.loadURI(browser, "about:mozilla");
    await BrowserTestUtils.browserLoaded(browser);

    await soundPlayingStoppedPromise;

    ok(
      !tab.hasAttribute("soundplaying"),
      "Tab should not be playing sound any more"
    );
    ok(!tab.soundPlaying, "Tab should not be playing sound any more");
  }

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: PAGE,
    },
    taskFn
  );
}

async function test_mute_keybinding() {
  async function test_muting_using_keyboard(tab) {
    let mutedPromise = get_wait_for_mute_promise(tab, true);
    EventUtils.synthesizeKey("m", { ctrlKey: true });
    await mutedPromise;
    mutedPromise = get_wait_for_mute_promise(tab, false);
    EventUtils.synthesizeKey("m", { ctrlKey: true });
    await mutedPromise;
  }
  async function taskFn(browser) {
    let tab = gBrowser.getTabForBrowser(browser);

    // Make sure it's possible to mute before the tab is playing.
    await test_muting_using_keyboard(tab);

    //   Start playback and wait for it to finish.
    await play(tab);

    // Make sure it's possible to mute after the tab is playing.
    await test_muting_using_keyboard(tab);

    // Pause playback and wait for it to finish.
    await pause(tab);

    // Make sure things work if the tab is pinned.
    gBrowser.pinTab(tab);

    // Make sure it's possible to mute before the tab is playing.
    await test_muting_using_keyboard(tab);

    //   Start playback and wait for it to finish.
    await play(tab);

    // Make sure it's possible to mute after the tab is playing.
    await test_muting_using_keyboard(tab);

    gBrowser.unpinTab(tab);
  }

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: PAGE,
    },
    taskFn
  );
}

async function test_on_browser(browser) {
  let tab = gBrowser.getTabForBrowser(browser);

  // Test the icon in a normal tab.
  await test_playing_icon_on_tab(tab, browser, false);

  gBrowser.pinTab(tab);

  // Test the icon in a pinned tab.
  await test_playing_icon_on_tab(tab, browser, true);

  gBrowser.unpinTab(tab);

  // Test the sound playing icon for hidden tabs.
  await test_playing_icon_on_hidden_tab(tab);

  // Retest with another browser in the foreground tab
  if (gBrowser.selectedBrowser.currentURI.spec == PAGE) {
    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "data:text/html,test",
      },
      () => test_on_browser(browser)
    );
  } else {
    await test_browser_swapping(tab, browser);
  }
}

async function test_delayed_tabattr_removal() {
  async function taskFn(browser) {
    let tab = gBrowser.getTabForBrowser(browser);
    await play(tab);

    // Extend the delay to guarantee the soundplaying attribute
    // is not removed from the tab when audio is stopped. Without
    // the extended delay the attribute could be removed in the
    // same tick and the test wouldn't catch that this broke.
    await pause(tab, { extendedDelay: true });
  }

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: PAGE,
    },
    taskFn
  );
}

requestLongerTimeout(2);
add_task(async function test_page() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: PAGE,
    },
    test_on_browser
  );
});

add_task(test_click_on_pinned_tab_after_mute);

add_task(test_cross_process_load);

add_task(test_mute_keybinding);

add_task(test_delayed_tabattr_removal);
