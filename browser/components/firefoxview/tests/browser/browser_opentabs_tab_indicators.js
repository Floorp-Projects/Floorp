/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { NonPrivateTabs } = ChromeUtils.importESModule(
  "resource:///modules/OpenTabs.sys.mjs"
);

let pageWithAlert =
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.com/browser/browser/base/content/test/tabPrompts/openPromptOffTimeout.html";
let pageWithSound =
  "http://mochi.test:8888/browser/dom/base/test/file_audioLoop.html";

function cleanup() {
  // Cleanup
  while (gBrowser.tabs.length > 1) {
    BrowserTestUtils.removeTab(gBrowser.tabs[0]);
  }
}

add_task(async function test_notification_dot_indicator() {
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

    await TestUtils.waitForCondition(
      () => openTabs.viewCards[0].tabList.rowEls[1].attention,
      "The opened tab doesn't have the attention property, so no notification dot is shown."
    );

    info("The newly opened tab has a notification dot.");

    // Switch back to other tab to close prompt before cleanup
    await BrowserTestUtils.switchTab(gBrowser, openedTab);
    EventUtils.synthesizeKey("KEY_Enter", {}, win);

    cleanup();
  });
});

add_task(async function test_container_indicator() {
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

    let containerTabElem = openTabs.viewCards[0].tabList.rowEls[1];

    await TestUtils.waitForCondition(
      () => containerTabElem.containerObj,
      "The container tab element isn't marked in Fx View."
    );

    ok(
      containerTabElem.shadowRoot
        .querySelector(".fxview-tab-row-container-indicator")
        .classList.contains("identity-color-blue"),
      "The container color is blue."
    );

    info("The newly opened tab is marked as a container tab.");

    cleanup();
  });
});

add_task(async function test_sound_playing_muted_indicator() {
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

    let soundPlayingTabElem = openTabs.viewCards[0].tabList.rowEls[1];

    await TestUtils.waitForCondition(() => soundPlayingTabElem.soundPlaying);

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

    await TestUtils.waitForCondition(() => soundPlayingTabElem.muted);

    ok(
      soundPlayingTabElem.mediaButtonEl,
      "The tab has the unmute button showing."
    );

    // Mute and unmute the tab and make sure the element in Fx View updates
    soundTab.toggleMuteAudio();
    await tabChangeRaised;
    await openTabs.updateComplete;
    await TestUtils.waitForCondition(() => soundPlayingTabElem.soundPlaying);

    ok(
      soundPlayingTabElem.mediaButtonEl,
      "The tab has the mute button showing."
    );

    soundTab.toggleMuteAudio();
    await tabChangeRaised;
    await openTabs.updateComplete;
    await TestUtils.waitForCondition(() => soundPlayingTabElem.muted);

    ok(
      soundPlayingTabElem.mediaButtonEl,
      "The tab has the unmute button showing."
    );

    cleanup();
  });
});
