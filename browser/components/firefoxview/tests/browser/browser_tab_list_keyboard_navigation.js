/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_open_tab_row_navigation() {
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    let win = browser.ownerGlobal;

    await navigateToViewAndWait(document, "opentabs");
    const openTabs = document.querySelector("view-opentabs[name=opentabs]");

    await TestUtils.waitForCondition(
      () => openTabs.viewCards[0].tabList?.rowEls.length === 1,
      "The tab list hasn't rendered"
    );

    // Focus tab row
    let tabRow = openTabs.viewCards[0].tabList.rowEls[0];
    let tabRowFocused = BrowserTestUtils.waitForEvent(tabRow, "focus", win);
    tabRow.focus();
    await tabRowFocused;
    info("The tab row main element has focus.");

    // Navigate right to context menu button
    let secondaryButtonFocused = BrowserTestUtils.waitForEvent(
      tabRow.secondaryButtonEl,
      "focus",
      win
    );
    EventUtils.synthesizeKey("KEY_ArrowRight", {}, win);
    await secondaryButtonFocused;
    info("The context menu button has focus.");

    // Navigate right to close button
    let tertiaryButtonFocused = BrowserTestUtils.waitForEvent(
      tabRow.tertiaryButtonEl,
      "focus",
      win
    );
    EventUtils.synthesizeKey("KEY_ArrowRight", {}, win);
    await tertiaryButtonFocused;
    info("The close button has focus");

    // Navigate left to context menu button
    secondaryButtonFocused = BrowserTestUtils.waitForEvent(
      tabRow.secondaryButtonEl,
      "focus",
      win
    );
    EventUtils.synthesizeKey("KEY_ArrowLeft", {}, win);
    await secondaryButtonFocused;
    info("The context menu button has focus.");

    // Navigate left to tab row main element
    tabRowFocused = BrowserTestUtils.waitForEvent(tabRow.mainEl, "focus", win);
    EventUtils.synthesizeKey("KEY_ArrowLeft", {}, win);
    await tabRowFocused;
    info("The tab row main element has focus.");
  });

  cleanupTabs();
});

add_task(async function test_focus_moves_after_unmute() {
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    let win = browser.ownerGlobal;
    await navigateToViewAndWait(document, "opentabs");
    let openTabs = document.querySelector("view-opentabs[name=opentabs]");
    await openTabs.updateComplete;
    await TestUtils.waitForCondition(
      () => openTabs.viewCards[0].tabList.rowEls.length,
      "The tab list has rendered."
    );
    await openTabs.openTabsTarget.readyWindowsPromise;
    let card = openTabs.viewCards[0];
    let openTabEl = card.tabList.rowEls[0];
    let tabChangeRaised = BrowserTestUtils.waitForEvent(
      NonPrivateTabs,
      "TabChange"
    );

    // Mute tab
    openTabEl.muteOrUnmuteTab();

    await tabChangeRaised;
    await openTabs.updateComplete;

    let mutedTab = card.tabList.rowEls[0];
    await TestUtils.waitForCondition(
      () => mutedTab.indicators.includes("muted"),
      "The tab has been muted."
    );

    // Unmute using keyboard
    mutedTab.focusMediaButton();
    isActiveElement(mutedTab.mediaButtonEl);
    info("The media button has focus.");

    tabChangeRaised = BrowserTestUtils.waitForEvent(
      NonPrivateTabs,
      "TabChange"
    );
    EventUtils.synthesizeKey("KEY_Enter", {}, win);

    await tabChangeRaised;
    await openTabs.updateComplete;

    let unmutedTab = card.tabList.rowEls[0];
    await TestUtils.waitForCondition(
      () => !unmutedTab.indicators.includes("muted"),
      "The tab is no longer muted."
    );
    isActiveElement(unmutedTab.secondaryButtonEl);
    info(
      "Focus should be on the tab's secondary button element after unmuting."
    );

    // Mute tab again and check that only Enter keys will toggle it
    unmutedTab.muteOrUnmuteTab();
    await TestUtils.waitForCondition(
      () => mutedTab.indicators.includes("muted"),
      "The tab has been muted."
    );
    mutedTab = card.tabList.rowEls[0];

    mutedTab.focusLink();
    isActiveElement(mutedTab.mainEl);
    info("The 'main' element has focus.");

    EventUtils.synthesizeKey("KEY_ArrowRight", {}, win);
    isActiveElement(mutedTab.mediaButtonEl);
    info("The media button has focus.");

    EventUtils.synthesizeKey("KEY_ArrowRight", {}, win);
    isActiveElement(mutedTab.secondaryButtonEl);
    info("The secondary/more button has focus.");

    ok(
      mutedTab.indicators.includes("muted"),
      "The muted tab is still muted after arrowing past it."
    );

    EventUtils.synthesizeKey("KEY_ArrowLeft", {}, win);
    isActiveElement(mutedTab.mediaButtonEl);
    info("The media button has focus.");

    EventUtils.synthesizeKey("KEY_Enter", {}, win);
    await tabChangeRaised;
    await openTabs.updateComplete;

    unmutedTab = card.tabList.rowEls[0];
    await TestUtils.waitForCondition(
      () => !unmutedTab.indicators.includes("muted"),
      "The tab is no longer muted."
    );
    isActiveElement(unmutedTab.secondaryButtonEl);
    info(
      "Focus should be on the tab's secondary button element after unmuting."
    );
  });

  cleanupTabs();
});

add_task(async function test_open_tab_row_with_sound_navigation() {
  const tabWithSound = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://mochi.test:8888/browser/dom/base/test/file_audioLoop.html",
    true
  );
  const tabsUpdated = await BrowserTestUtils.waitForEvent(
    NonPrivateTabs,
    "TabChange"
  );
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    let win = browser.ownerGlobal;

    await navigateToViewAndWait(document, "opentabs");
    const openTabs = document.querySelector("view-opentabs[name=opentabs]");
    await TestUtils.waitForCondition(
      () => tabWithSound.hasAttribute("soundplaying"),
      "Tab is playing sound"
    );
    await tabsUpdated;
    await openTabs.updateComplete;

    await TestUtils.waitForCondition(
      () => openTabs.viewCards[0].tabList?.rowEls.length === 2,
      "The tab list has rendered."
    );

    // Focus tab row with sound playing
    let tabRow;
    for (const rowEl of openTabs.viewCards[0].tabList.rowEls) {
      if (rowEl.indicators.includes("soundplaying")) {
        tabRow = rowEl;
        break;
      }
    }
    ok(tabRow, "Found a tab row with sound playing.");
    let tabRowFocused = BrowserTestUtils.waitForEvent(tabRow, "focus", win);
    tabRow.focus();
    await tabRowFocused;
    info("The tab row main element has focus.");

    // Navigate right to media button
    let mediaButtonFocused = BrowserTestUtils.waitForEvent(
      tabRow.mediaButtonEl,
      "focus",
      win
    );
    EventUtils.synthesizeKey("KEY_ArrowRight", {}, win);
    await mediaButtonFocused;
    info("The media button has focus.");

    // Navigate right to context menu button
    let secondaryButtonFocused = BrowserTestUtils.waitForEvent(
      tabRow.secondaryButtonEl,
      "focus",
      win
    );
    EventUtils.synthesizeKey("KEY_ArrowRight", {}, win);
    await secondaryButtonFocused;
    info("The context menu button has focus.");

    // Navigate right to close button
    let tertiaryButtonFocused = BrowserTestUtils.waitForEvent(
      tabRow.tertiaryButtonEl,
      "focus",
      win
    );
    EventUtils.synthesizeKey("KEY_ArrowRight", {}, win);
    await tertiaryButtonFocused;
    info("The close button has focus");

    // Navigate left to context menuo button
    secondaryButtonFocused = BrowserTestUtils.waitForEvent(
      tabRow.secondaryButtonEl,
      "focus",
      win
    );
    EventUtils.synthesizeKey("KEY_ArrowLeft", {}, win);
    await secondaryButtonFocused;
    info("The context menu button has focus.");

    // Navigate left to media button
    mediaButtonFocused = BrowserTestUtils.waitForEvent(
      tabRow.mediaButtonEl,
      "focus",
      win
    );
    EventUtils.synthesizeKey("KEY_ArrowLeft", {}, win);
    await mediaButtonFocused;
    info("The media button has focus.");

    // Navigate left to main element of tab row
    tabRowFocused = BrowserTestUtils.waitForEvent(tabRow.mainEl, "focus", win);
    EventUtils.synthesizeKey("KEY_ArrowLeft", {}, win);
    await tabRowFocused;
    info("The tab row main element has focus.");
  });

  cleanupTabs();
});

add_task(async function test_open_tab_row_with_sound_mute_and_unmute() {
  const tabWithSound = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://mochi.test:8888/browser/dom/base/test/file_audioLoop.html",
    true
  );
  const tabsUpdated = await BrowserTestUtils.waitForEvent(
    NonPrivateTabs,
    "TabChange"
  );
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    let win = browser.ownerGlobal;

    await navigateToViewAndWait(document, "opentabs");
    const openTabs = document.querySelector("view-opentabs[name=opentabs]");
    await TestUtils.waitForCondition(
      () => tabWithSound.hasAttribute("soundplaying"),
      "Tab is playing sound"
    );
    await tabsUpdated;
    await openTabs.updateComplete;

    await TestUtils.waitForCondition(
      () => openTabs.viewCards[0].tabList?.rowEls.length === 2,
      "The tab list has rendered."
    );

    // Focus tab row with sound playing
    let tabRow;
    for (const rowEl of openTabs.viewCards[0].tabList.rowEls) {
      if (rowEl.indicators.includes("soundplaying")) {
        tabRow = rowEl;
        break;
      }
    }
    ok(tabRow, "Found a tab row with sound playing.");
    let tabRowFocused = BrowserTestUtils.waitForEvent(tabRow, "focus", win);
    tabRow.focus();
    await tabRowFocused;
    info("The tab row main element has focus.");

    // Navigate right to media button
    let mediaButtonFocused = BrowserTestUtils.waitForEvent(
      tabRow.mediaButtonEl,
      "focus",
      win
    );
    EventUtils.synthesizeKey("KEY_ArrowRight", {}, win);
    await mediaButtonFocused;
    info("The media button has focus.");

    EventUtils.synthesizeKey("KEY_Enter", {}, win);
    await TestUtils.waitForCondition(
      () => tabRow.indicators.includes("muted"),
      "Tab has been muted"
    );

    EventUtils.synthesizeKey("KEY_Enter", {}, win);
    await TestUtils.waitForCondition(
      () => tabRow.indicators.includes("soundplaying"),
      "Tab has been unmuted"
    );
  });

  cleanupTabs();
});
