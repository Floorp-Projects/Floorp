/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_ENGINE_NAME = "searchSuggestionEngine";
const HANDOFF_PREF =
  "browser.newtabpage.activity-stream.improvesearch.handoffToAwesomebar";

let extension;
let defaultEngine;
let addedEngine;

add_task(async function setup() {
  defaultEngine = await Services.search.getDefault();

  extension = await SearchTestUtils.installSearchExtension({
    id: TEST_ENGINE_NAME,
    name: TEST_ENGINE_NAME,
    suggest_url:
      "https://example.com/browser/browser/components/search/test/browser/searchSuggestionEngine.sjs",
    suggest_url_get_params: "query={searchTerms}",
  });

  addedEngine = await Services.search.getEngineByName(TEST_ENGINE_NAME);

  registerCleanupFunction(async () => {
    await Services.search.setDefault(defaultEngine);
  });
});

// async function runAboutNewTabTest(expectedIcon)

async function ensureIcon(tab, expectedIcon) {
  await SpecialPowers.spawn(tab.linkedBrowser, [expectedIcon], async function(
    icon
  ) {
    await ContentTaskUtils.waitForCondition(() => !content.document.hidden);

    let computedStyle = content.window.getComputedStyle(content.document.body);
    await ContentTaskUtils.waitForCondition(
      () => computedStyle.getPropertyValue("--newtab-search-icon") != "null",
      "Search Icon not set."
    );

    Assert.equal(
      computedStyle.getPropertyValue("--newtab-search-icon"),
      `url(${icon})`,
      "Should have the expected icon"
    );
  });
}

async function runNewTabTest() {
  let tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:newtab",
    gBrowser,
    waitForLoad: false,
  });

  let engineIcon = defaultEngine.getIconURLBySize(16, 16);

  await ensureIcon(tab, engineIcon);

  await Services.search.setDefault(addedEngine);

  // We only show the engine's own icon for app provided engines, otherwise show
  // a default. xref https://bugzilla.mozilla.org/show_bug.cgi?id=1449338#c19
  await ensureIcon(tab, "chrome://global/skin/icons/search-glass.svg");

  await Services.search.setDefault(defaultEngine);

  BrowserTestUtils.removeTab(tab);
}

add_task(async function test_content_search_icon() {
  SpecialPowers.pushPrefEnv({
    set: [[HANDOFF_PREF, true]],
  });

  await runNewTabTest();
});

add_task(async function test_content_search_icon_no_handoff() {
  SpecialPowers.pushPrefEnv({
    set: [[HANDOFF_PREF, false]],
  });

  await runNewTabTest();
});

add_task(async function test_content_search_icon_in_private_window() {
  let win = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
    waitForTabURL: "about:privatebrowsing",
  });
  let tab = win.gBrowser.selectedTab;

  let engineIcon = defaultEngine.getIconURLBySize(16, 16);

  await ensureIcon(tab, engineIcon);

  await Services.search.setDefault(addedEngine);

  // We only show the engine's own icon for app provided engines, otherwise show
  // a default. xref https://bugzilla.mozilla.org/show_bug.cgi?id=1449338#c19
  await ensureIcon(tab, "chrome://global/skin/icons/search-glass.svg");

  await Services.search.setDefault(defaultEngine);

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function cleanup() {
  // Extensions must be unloaded before registerCleanupFunction runs, so
  // unload them here.
  await extension.unload();
});
