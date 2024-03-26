/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

requestLongerTimeout(2);

let pageWithAlert =
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.com/browser/browser/base/content/test/tabPrompts/openPromptOffTimeout.html";
let pageWithSound =
  "http://mochi.test:8888/browser/dom/base/test/file_audioLoop.html";

add_task(async function test_notification_dot_indicator() {
  clearHistory();
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    let win = browser.ownerGlobal;
    await navigateToViewAndWait(document, "opentabs");
    // load page that opens prompt when page is hidden
    let openedTab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      pageWithAlert,
      true
    );
    let openedTabGotAttentionPromise = BrowserTestUtils.waitForAttribute(
      "attention",
      openedTab
    );

    let tabChangeRaised = BrowserTestUtils.waitForEvent(
      NonPrivateTabs,
      "TabChange"
    );

    await switchToFxViewTab();

    let openTabs = document.querySelector("view-opentabs[name=opentabs]");

    await openedTabGotAttentionPromise;
    await tabChangeRaised;
    await openTabs.updateComplete;

    await TestUtils.waitForCondition(() =>
      Array.from(openTabs.viewCards[0].tabList.rowEls).some(rowEl => {
        return rowEl.indicators.includes("attention");
      })
    );

    info("The newly opened tab has a notification dot.");

    // Switch back to other tab to close prompt before cleanup
    await BrowserTestUtils.switchTab(gBrowser, openedTab);
    EventUtils.synthesizeKey("KEY_Enter", {}, win);

    cleanupTabs();
  });
});

add_task(async function test_container_indicator() {
  clearHistory();
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    let win = browser.ownerGlobal;

    // Load a page in a container tab
    let userContextId = 1;
    let containerTab = BrowserTestUtils.addTab(win.gBrowser, URLs[0], {
      userContextId,
    });

    await BrowserTestUtils.browserLoaded(
      containerTab.linkedBrowser,
      false,
      URLs[0]
    );

    await navigateToViewAndWait(document, "opentabs");

    let openTabs = document.querySelector("view-opentabs[name=opentabs]");

    await TestUtils.waitForCondition(
      () =>
        containerTab.getAttribute("usercontextid") === userContextId.toString(),
      "The container tab doesn't have the usercontextid attribute."
    );
    await openTabs.updateComplete;
    await TestUtils.waitForCondition(
      () => openTabs.viewCards[0].tabList?.rowEls.length,
      "The tab list hasn't rendered."
    );
    info("openTabs component has finished updating.");

    let containerTabElem;

    await TestUtils.waitForCondition(
      () =>
        Array.from(openTabs.viewCards[0].tabList.rowEls).some(rowEl => {
          let hasContainerObj;
          if (rowEl.containerObj?.icon) {
            containerTabElem = rowEl;
            hasContainerObj = rowEl.containerObj;
          }

          return hasContainerObj;
        }),
      "The container tab element isn't marked in Fx View."
    );
    ok(
      containerTabElem.shadowRoot
        .querySelector(".fxview-tab-row-container-indicator")
        .classList.contains("identity-color-blue"),
      "The container color is blue."
    );

    info("The newly opened tab is marked as a container tab.");

    cleanupTabs();
  });
});

add_task(async function test_sound_playing_muted_indicator() {
  clearHistory();
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    await navigateToViewAndWait(document, "opentabs");

    // Load a page in a container tab
    let soundTab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      pageWithSound,
      true
    );

    let tabChangeRaised = BrowserTestUtils.waitForEvent(
      NonPrivateTabs,
      "TabChange"
    );

    await switchToFxViewTab();

    let openTabs = document.querySelector("view-opentabs[name=opentabs]");

    await TestUtils.waitForCondition(() =>
      soundTab.hasAttribute("soundplaying")
    );
    await tabChangeRaised;
    await openTabs.updateComplete;
    await TestUtils.waitForCondition(
      () => openTabs.viewCards[0].tabList?.rowEls.length,
      "The tab list hasn't rendered."
    );

    let soundPlayingTabElem;
    await TestUtils.waitForCondition(() =>
      Array.from(openTabs.viewCards[0].tabList.rowEls).some(rowEl => {
        soundPlayingTabElem = rowEl;
        return rowEl.indicators.includes("soundplaying");
      })
    );

    ok(
      soundPlayingTabElem.mediaButtonEl,
      "The tab has the mute button showing."
    );

    tabChangeRaised = BrowserTestUtils.waitForEvent(
      NonPrivateTabs,
      "TabChange"
    );

    // Mute the tab
    EventUtils.synthesizeMouseAtCenter(
      soundPlayingTabElem.mediaButtonEl,
      {},
      content
    );

    await TestUtils.waitForCondition(
      () => soundTab.hasAttribute("muted"),
      "The tab doesn't have the muted attribute."
    );
    await tabChangeRaised;
    await openTabs.updateComplete;

    await TestUtils.waitForCondition(() =>
      soundPlayingTabElem.indicators.includes("muted")
    );

    ok(
      soundPlayingTabElem.mediaButtonEl,
      "The tab has the unmute button showing."
    );

    // Mute and unmute the tab and make sure the element in Fx View updates
    soundTab.toggleMuteAudio();
    await tabChangeRaised;
    await openTabs.updateComplete;
    await TestUtils.waitForCondition(() =>
      soundPlayingTabElem.indicators.includes("soundplaying")
    );

    ok(
      soundPlayingTabElem.mediaButtonEl,
      "The tab has the mute button showing."
    );

    soundTab.toggleMuteAudio();
    await tabChangeRaised;
    await openTabs.updateComplete;
    await TestUtils.waitForCondition(() =>
      soundPlayingTabElem.indicators.includes("muted")
    );

    ok(
      soundPlayingTabElem.mediaButtonEl,
      "The tab has the unmute button showing."
    );

    cleanupTabs();
  });
});

add_task(async function test_bookmark_indicator() {
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URLs[0]);
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    await navigateToViewAndWait(document, "opentabs");
    const openTabs = document.querySelector("view-opentabs[name=opentabs]");
    setSortOption(openTabs, "recency");
    let rowEl = await TestUtils.waitForCondition(
      () => openTabs.viewCards[0]?.tabList.rowEls[0]
    );

    info("Bookmark a tab while Firefox View is active.");
    let bookmark = await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      url: URLs[0],
    });
    await TestUtils.waitForCondition(
      () => rowEl.shadowRoot.querySelector(".bookmark"),
      "Tab shows the bookmark star."
    );
    await PlacesUtils.bookmarks.update({
      guid: bookmark.guid,
      url: URLs[1],
    });
    await TestUtils.waitForCondition(
      () => !rowEl.shadowRoot.querySelector(".bookmark"),
      "The bookmark star is removed."
    );
    bookmark = await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      url: URLs[0],
    });
    await TestUtils.waitForCondition(
      () => rowEl.shadowRoot.querySelector(".bookmark"),
      "The bookmark star is restored."
    );
    await PlacesUtils.bookmarks.remove(bookmark.guid);
    await TestUtils.waitForCondition(
      () => !rowEl.shadowRoot.querySelector(".bookmark"),
      "The bookmark star is removed again."
    );

    info("Bookmark a tab while Firefox View is inactive.");
    await BrowserTestUtils.switchTab(gBrowser, tab);
    bookmark = await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      url: URLs[0],
    });
    await switchToFxViewTab();
    await TestUtils.waitForCondition(
      () => rowEl.shadowRoot.querySelector(".bookmark"),
      "Tab shows the bookmark star."
    );
    await BrowserTestUtils.switchTab(gBrowser, tab);
    await PlacesUtils.bookmarks.remove(bookmark.guid);
    await switchToFxViewTab();
    await TestUtils.waitForCondition(
      () => !rowEl.shadowRoot.querySelector(".bookmark"),
      "The bookmark star is removed."
    );
  });
  await cleanupTabs();
  await PlacesUtils.bookmarks.eraseEverything();
});
