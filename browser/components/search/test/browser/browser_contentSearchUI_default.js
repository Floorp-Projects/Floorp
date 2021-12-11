/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_ENGINE_NAME = "searchSuggestionEngine";
const HANDOFF_PREF =
  "browser.newtabpage.activity-stream.improvesearch.handoffToAwesomebar";

let extension;
let defaultEngine;
let addedEngine;

add_task(async function setup() {
  // Disable window occlusion. Bug 1733955
  if (navigator.platform.indexOf("Win") == 0) {
    await SpecialPowers.pushPrefEnv({
      set: [["widget.windows.window_occlusion_tracking.enabled", false]],
    });
  }

  defaultEngine = await Services.search.getDefault();

  extension = await SearchTestUtils.installSearchExtension({
    id: TEST_ENGINE_NAME,
    name: TEST_ENGINE_NAME,
    suggest_url:
      "https://example.com/browser/browser/components/search/test/browser/searchSuggestionEngine.sjs",
    suggest_url_get_params: "query={searchTerms}",
  });

  addedEngine = await Services.search.getEngineByName(TEST_ENGINE_NAME);

  // Enable suggestions in this test. Otherwise, the string in the content
  // search box changes.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.suggest.searches", true]],
  });

  registerCleanupFunction(async () => {
    await Services.search.setDefault(defaultEngine);
  });
});

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

async function ensurePlaceholder(tab, expectedId, expectedEngine) {
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [expectedId, expectedEngine],
    async function(id, engine) {
      await ContentTaskUtils.waitForCondition(() => !content.document.hidden);

      await ContentTaskUtils.waitForCondition(
        () => content.document.querySelector(".search-handoff-button"),
        "l10n ID not set."
      );
      let buttonNode = content.document.querySelector(".search-handoff-button");
      let expectedAttributes = { id, args: engine ? { engine } : null };
      Assert.deepEqual(
        content.document.l10n.getAttributes(buttonNode),
        expectedAttributes,
        "Expected updated l10n ID and args."
      );
    }
  );
}

async function runNewTabTest(isHandoff) {
  let tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:newtab",
    gBrowser,
    waitForLoad: false,
  });

  let engineIcon = defaultEngine.getIconURLBySize(16, 16);

  await ensureIcon(tab, engineIcon);
  if (isHandoff) {
    await ensurePlaceholder(
      tab,
      "newtab-search-box-handoff-input",
      Services.search.defaultEngine.name
    );
  }

  await Services.search.setDefault(addedEngine);

  // We only show the engine's own icon for app provided engines, otherwise show
  // a default. xref https://bugzilla.mozilla.org/show_bug.cgi?id=1449338#c19
  await ensureIcon(tab, "chrome://global/skin/icons/search-glass.svg");
  if (isHandoff) {
    await ensurePlaceholder(tab, "newtab-search-box-handoff-input-no-engine");
  }

  // Disable suggestions in the Urlbar. This should update the placeholder
  // string since handoff will now enter search mode.
  if (isHandoff) {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.urlbar.suggest.searches", false]],
    });
    await ensurePlaceholder(tab, "newtab-search-box-input");
    await SpecialPowers.popPrefEnv();
  }

  await Services.search.setDefault(defaultEngine);

  BrowserTestUtils.removeTab(tab);
}

add_task(async function test_content_search_attributes() {
  await SpecialPowers.pushPrefEnv({
    set: [[HANDOFF_PREF, true]],
  });

  await runNewTabTest(true);
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_content_search_attributes_no_handoff() {
  await SpecialPowers.pushPrefEnv({
    set: [[HANDOFF_PREF, false]],
  });

  await runNewTabTest(false);
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_content_search_attributes_in_private_window() {
  let win = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
    waitForTabURL: "about:privatebrowsing",
  });
  let tab = win.gBrowser.selectedTab;

  let engineIcon = defaultEngine.getIconURLBySize(16, 16);

  await ensureIcon(tab, engineIcon);
  await ensurePlaceholder(
    tab,
    "about-private-browsing-handoff",
    Services.search.defaultEngine.name
  );

  await Services.search.setDefault(addedEngine);

  // We only show the engine's own icon for app provided engines, otherwise show
  // a default. xref https://bugzilla.mozilla.org/show_bug.cgi?id=1449338#c19
  await ensureIcon(tab, "chrome://global/skin/icons/search-glass.svg");
  await ensurePlaceholder(tab, "about-private-browsing-handoff-no-engine");

  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.suggest.searches", false]],
  });
  await ensurePlaceholder(tab, "about-private-browsing-search-btn");
  await SpecialPowers.popPrefEnv();

  await Services.search.setDefault(defaultEngine);

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_content_search_permanent_private_browsing() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [HANDOFF_PREF, true],
      ["browser.privatebrowsing.autostart", true],
    ],
  });

  let win = await BrowserTestUtils.openNewBrowserWindow();
  await runNewTabTest(true);
  await BrowserTestUtils.closeWindow(win);
  await SpecialPowers.popPrefEnv();
});
