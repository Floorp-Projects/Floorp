const { NonPrivateTabs } = ChromeUtils.importESModule(
  "resource:///modules/OpenTabs.sys.mjs"
);

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

  while (gBrowser.tabs.length > 1) {
    BrowserTestUtils.removeTab(gBrowser.tabs[0]);
  }
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
      "The tab list hasn't rendered"
    );

    // Focus tab row with sound playing
    let tabRow = openTabs.viewCards[0].tabList.rowEls.find(rowEl =>
      rowEl.indicators.includes("soundplaying")
    );
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

  while (gBrowser.tabs.length > 1) {
    BrowserTestUtils.removeTab(gBrowser.tabs[0]);
  }
});
