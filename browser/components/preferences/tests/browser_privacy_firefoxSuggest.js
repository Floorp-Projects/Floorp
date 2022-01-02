/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This tests the Privacy pane's Firefox Suggest UI.

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarProviderQuickSuggest:
    "resource:///modules/UrlbarProviderQuickSuggest.jsm",
});

XPCOMUtils.defineLazyGetter(this, "QuickSuggestTestUtils", () => {
  const { QuickSuggestTestUtils: module } = ChromeUtils.import(
    "resource://testing-common/QuickSuggestTestUtils.jsm"
  );
  module.init(this);
  registerCleanupFunction(() => module.uninit());
  return module;
});

const CONTAINER_ID = "firefoxSuggestContainer";
const NONSPONSORED_CHECKBOX_ID = "firefoxSuggestNonsponsoredToggle";
const SPONSORED_CHECKBOX_ID = "firefoxSuggestSponsoredToggle";
const DATA_COLLECTION_CHECKBOX_ID = "firefoxSuggestDataCollectionToggle";
const INFO_BOX_ID = "firefoxSuggestInfoBox";
const INFO_TEXT_ID = "firefoxSuggestInfoText";
const LEARN_MORE_CLASS = "firefoxSuggestLearnMore";

// Maps text element IDs to `{ enabled, disabled }`, where `enabled` is the
// expected l10n ID when the Firefox Suggest feature is enabled, and `disabled`
// is when disabled.
const EXPECTED_L10N_IDS = {
  locationBarGroupHeader: {
    enabled: "addressbar-header-firefox-suggest",
    disabled: "addressbar-header",
  },
  locationBarSuggestionLabel: {
    enabled: "addressbar-suggest-firefox-suggest",
    disabled: "addressbar-suggest",
  },
};

// Allow more time for Mac machines so they don't time out in verify mode.
if (AppConstants.platform == "macosx") {
  requestLongerTimeout(3);
}

// The following tasks check the visibility of the Firefox Suggest UI based on
// the value of the feature pref. See doVisibilityTest().

add_task(async function historyToOffline() {
  await doVisibilityTest({
    initialScenario: "history",
    initialExpectedVisibility: false,
    newScenario: "offline",
    newExpectedVisibility: true,
  });
});

add_task(async function historyToOnline() {
  await doVisibilityTest({
    initialScenario: "history",
    initialExpectedVisibility: false,
    newScenario: "online",
    newExpectedVisibility: true,
  });
});

add_task(async function offlineToHistory() {
  await doVisibilityTest({
    initialScenario: "offline",
    initialExpectedVisibility: true,
    newScenario: "history",
    newExpectedVisibility: false,
  });
});

add_task(async function offlineToOnline() {
  await doVisibilityTest({
    initialScenario: "offline",
    initialExpectedVisibility: true,
    newScenario: "online",
    newExpectedVisibility: true,
  });
});

add_task(async function onlineToHistory() {
  await doVisibilityTest({
    initialScenario: "online",
    initialExpectedVisibility: true,
    newScenario: "history",
    newExpectedVisibility: false,
  });
});

add_task(async function onlineToOffline() {
  await doVisibilityTest({
    initialScenario: "online",
    initialExpectedVisibility: true,
    newScenario: "offline",
    newExpectedVisibility: true,
  });
});

/**
 * Runs a test that checks the visibility of the Firefox Suggest preferences UI
 * based on scenario pref.
 *
 * @param {string} initialScenario
 *   The initial scenario.
 * @param {boolean} initialExpectedVisibility
 *   Whether the UI should be visible with the initial scenario.
 * @param {string} newScenario
 *   The updated scenario.
 * @param {boolean} newExpectedVisibility
 *   Whether the UI should be visible after setting the new scenario.
 */
async function doVisibilityTest({
  initialScenario,
  initialExpectedVisibility,
  newScenario,
  newExpectedVisibility,
}) {
  info(
    "Running visibility test: " +
      JSON.stringify({
        initialScenario,
        initialExpectedVisibility,
        newScenario,
        newExpectedVisibility,
      })
  );

  // Set the initial scenario.
  await QuickSuggestTestUtils.setScenario(initialScenario);

  Assert.equal(
    Services.prefs.getBoolPref("browser.urlbar.quicksuggest.enabled"),
    initialExpectedVisibility,
    `quicksuggest.enabled is correct after setting initial scenario, initialExpectedVisibility=${initialExpectedVisibility}`
  );

  // Open prefs and check the initial visibility.
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });

  let doc = gBrowser.selectedBrowser.contentDocument;
  let container = doc.getElementById(CONTAINER_ID);
  Assert.equal(
    BrowserTestUtils.is_visible(container),
    initialExpectedVisibility,
    `The container has the expected initial visibility, initialExpectedVisibility=${initialExpectedVisibility}`
  );

  // Check the text elements' l10n IDs.
  for (let [id, { enabled, disabled }] of Object.entries(EXPECTED_L10N_IDS)) {
    Assert.equal(
      doc.getElementById(id).dataset.l10nId,
      initialExpectedVisibility ? enabled : disabled,
      `Initial l10n ID for element with ID ${id}, initialExpectedVisibility=${initialExpectedVisibility}`
    );
  }

  // Set the new scenario.
  await QuickSuggestTestUtils.setScenario(newScenario);

  Assert.equal(
    Services.prefs.getBoolPref("browser.urlbar.quicksuggest.enabled"),
    newExpectedVisibility,
    `quicksuggest.enabled is correct after setting new scenario, newExpectedVisibility=${newExpectedVisibility}`
  );

  // Check visibility again.
  Assert.equal(
    BrowserTestUtils.is_visible(container),
    newExpectedVisibility,
    `The container has the expected visibility after setting new scenario, newExpectedVisibility=${newExpectedVisibility}`
  );

  // Check the text elements' l10n IDs again.
  for (let [id, { enabled, disabled }] of Object.entries(EXPECTED_L10N_IDS)) {
    Assert.equal(
      doc.getElementById(id).dataset.l10nId,
      newExpectedVisibility ? enabled : disabled,
      `New l10n ID for element with ID ${id}, newExpectedVisibility=${newExpectedVisibility}`
    );
  }

  // Clean up.
  gBrowser.removeCurrentTab();
  await QuickSuggestTestUtils.setScenario(null);
}

// Verifies all 8 states of the 3 toggles and their related info box states.
add_task(async function togglesAndInfoBox() {
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });

  // suggest.quicksuggest.nonsponsored = true
  // suggest.quicksuggest.sponsored = true
  // quicksuggest.dataCollection.enabled = true
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.quicksuggest.nonsponsored", true],
      ["browser.urlbar.suggest.quicksuggest.sponsored", true],
      ["browser.urlbar.quicksuggest.dataCollection.enabled", true],
    ],
  });
  assertCheckboxes({
    [NONSPONSORED_CHECKBOX_ID]: true,
    [SPONSORED_CHECKBOX_ID]: true,
    [DATA_COLLECTION_CHECKBOX_ID]: true,
  });
  await assertInfoBox("addressbar-firefox-suggest-info-all");
  await SpecialPowers.popPrefEnv();

  // suggest.quicksuggest.nonsponsored = true
  // suggest.quicksuggest.sponsored = true
  // quicksuggest.dataCollection.enabled = false
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.quicksuggest.nonsponsored", true],
      ["browser.urlbar.suggest.quicksuggest.sponsored", true],
      ["browser.urlbar.quicksuggest.dataCollection.enabled", false],
    ],
  });
  assertCheckboxes({
    [NONSPONSORED_CHECKBOX_ID]: true,
    [SPONSORED_CHECKBOX_ID]: true,
    [DATA_COLLECTION_CHECKBOX_ID]: false,
  });
  await assertInfoBox("addressbar-firefox-suggest-info-nonsponsored-sponsored");
  await SpecialPowers.popPrefEnv();

  // suggest.quicksuggest.nonsponsored = true
  // suggest.quicksuggest.sponsored = false
  // quicksuggest.dataCollection.enabled = true
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.quicksuggest.nonsponsored", true],
      ["browser.urlbar.suggest.quicksuggest.sponsored", false],
      ["browser.urlbar.quicksuggest.dataCollection.enabled", true],
    ],
  });
  assertCheckboxes({
    [NONSPONSORED_CHECKBOX_ID]: true,
    [SPONSORED_CHECKBOX_ID]: false,
    [DATA_COLLECTION_CHECKBOX_ID]: true,
  });
  await assertInfoBox("addressbar-firefox-suggest-info-nonsponsored-data");
  await SpecialPowers.popPrefEnv();

  // suggest.quicksuggest.nonsponsored = true
  // suggest.quicksuggest.sponsored = false
  // quicksuggest.dataCollection.enabled = false
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.quicksuggest.nonsponsored", true],
      ["browser.urlbar.suggest.quicksuggest.sponsored", false],
      ["browser.urlbar.quicksuggest.dataCollection.enabled", false],
    ],
  });
  assertCheckboxes({
    [NONSPONSORED_CHECKBOX_ID]: true,
    [SPONSORED_CHECKBOX_ID]: false,
    [DATA_COLLECTION_CHECKBOX_ID]: false,
  });
  await assertInfoBox("addressbar-firefox-suggest-info-nonsponsored");
  await SpecialPowers.popPrefEnv();

  // suggest.quicksuggest.nonsponsored = false
  // suggest.quicksuggest.sponsored = true
  // quicksuggest.dataCollection.enabled = true
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.quicksuggest.nonsponsored", false],
      ["browser.urlbar.suggest.quicksuggest.sponsored", true],
      ["browser.urlbar.quicksuggest.dataCollection.enabled", true],
    ],
  });
  assertCheckboxes({
    [NONSPONSORED_CHECKBOX_ID]: false,
    [SPONSORED_CHECKBOX_ID]: true,
    [DATA_COLLECTION_CHECKBOX_ID]: true,
  });
  await assertInfoBox("addressbar-firefox-suggest-info-sponsored-data");
  await SpecialPowers.popPrefEnv();

  // suggest.quicksuggest.nonsponsored = false
  // suggest.quicksuggest.sponsored = true
  // quicksuggest.dataCollection.enabled = false
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.quicksuggest.nonsponsored", false],
      ["browser.urlbar.suggest.quicksuggest.sponsored", true],
      ["browser.urlbar.quicksuggest.dataCollection.enabled", false],
    ],
  });
  assertCheckboxes({
    [NONSPONSORED_CHECKBOX_ID]: false,
    [SPONSORED_CHECKBOX_ID]: true,
    [DATA_COLLECTION_CHECKBOX_ID]: false,
  });
  await assertInfoBox("addressbar-firefox-suggest-info-sponsored");
  await SpecialPowers.popPrefEnv();

  // suggest.quicksuggest.nonsponsored = false
  // suggest.quicksuggest.sponsored = false
  // quicksuggest.dataCollection.enabled = true
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.quicksuggest.nonsponsored", false],
      ["browser.urlbar.suggest.quicksuggest.sponsored", false],
      ["browser.urlbar.quicksuggest.dataCollection.enabled", true],
    ],
  });
  assertCheckboxes({
    [NONSPONSORED_CHECKBOX_ID]: false,
    [SPONSORED_CHECKBOX_ID]: false,
    [DATA_COLLECTION_CHECKBOX_ID]: true,
  });
  await assertInfoBox("addressbar-firefox-suggest-info-data");
  await SpecialPowers.popPrefEnv();

  // suggest.quicksuggest.nonsponsored = false
  // suggest.quicksuggest.sponsored = false
  // quicksuggest.dataCollection.enabled = false
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.quicksuggest.nonsponsored", false],
      ["browser.urlbar.suggest.quicksuggest.sponsored", false],
      ["browser.urlbar.quicksuggest.dataCollection.enabled", false],
    ],
  });
  assertCheckboxes({
    [NONSPONSORED_CHECKBOX_ID]: false,
    [SPONSORED_CHECKBOX_ID]: false,
    [DATA_COLLECTION_CHECKBOX_ID]: false,
  });
  await assertInfoBox(null);
  await SpecialPowers.popPrefEnv();

  gBrowser.removeCurrentTab();
});

// Clicks each of the toggles and makes sure the prefs and info box are updated.
add_task(async function clickToggles() {
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });

  let doc = gBrowser.selectedBrowser.contentDocument;
  let addressBarSection = doc.getElementById("locationBarGroup");
  addressBarSection.scrollIntoView();

  // Set initial state.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.quicksuggest.nonsponsored", true],
      ["browser.urlbar.suggest.quicksuggest.sponsored", true],
      ["browser.urlbar.quicksuggest.dataCollection.enabled", true],
    ],
  });
  assertCheckboxes({
    [NONSPONSORED_CHECKBOX_ID]: true,
    [SPONSORED_CHECKBOX_ID]: true,
    [DATA_COLLECTION_CHECKBOX_ID]: true,
  });
  await assertInfoBox("addressbar-firefox-suggest-info-all");

  // non-sponsored toggle
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#" + NONSPONSORED_CHECKBOX_ID,
    {},
    gBrowser.selectedBrowser
  );
  Assert.ok(
    !Services.prefs.getBoolPref(
      "browser.urlbar.suggest.quicksuggest.nonsponsored"
    ),
    "suggest.quicksuggest.nonsponsored is false after clicking non-sponsored toggle"
  );
  assertCheckboxes({
    [NONSPONSORED_CHECKBOX_ID]: false,
    [SPONSORED_CHECKBOX_ID]: true,
    [DATA_COLLECTION_CHECKBOX_ID]: true,
  });
  await assertInfoBox("addressbar-firefox-suggest-info-sponsored-data");

  // sponsored toggle
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#" + SPONSORED_CHECKBOX_ID,
    {},
    gBrowser.selectedBrowser
  );
  Assert.ok(
    !Services.prefs.getBoolPref(
      "browser.urlbar.suggest.quicksuggest.nonsponsored"
    ),
    "suggest.quicksuggest.nonsponsored remains false after clicking sponsored toggle"
  );
  Assert.ok(
    !Services.prefs.getBoolPref(
      "browser.urlbar.suggest.quicksuggest.sponsored"
    ),
    "suggest.quicksuggest.sponsored is false after clicking sponsored toggle"
  );
  assertCheckboxes({
    [NONSPONSORED_CHECKBOX_ID]: false,
    [SPONSORED_CHECKBOX_ID]: false,
    [DATA_COLLECTION_CHECKBOX_ID]: true,
  });
  await assertInfoBox("addressbar-firefox-suggest-info-data");

  // data collection toggle
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#" + DATA_COLLECTION_CHECKBOX_ID,
    {},
    gBrowser.selectedBrowser
  );
  Assert.ok(
    !Services.prefs.getBoolPref(
      "browser.urlbar.suggest.quicksuggest.nonsponsored"
    ),
    "suggest.quicksuggest.nonsponsored remains false after clicking sponsored toggle"
  );
  Assert.ok(
    !Services.prefs.getBoolPref(
      "browser.urlbar.suggest.quicksuggest.sponsored"
    ),
    "suggest.quicksuggest.sponsored remains false after clicking data collection toggle"
  );
  Assert.ok(
    !Services.prefs.getBoolPref(
      "browser.urlbar.quicksuggest.dataCollection.enabled"
    ),
    "quicksuggest.dataCollection.enabled is false after clicking data collection toggle"
  );
  assertCheckboxes({
    [NONSPONSORED_CHECKBOX_ID]: false,
    [SPONSORED_CHECKBOX_ID]: false,
    [DATA_COLLECTION_CHECKBOX_ID]: false,
  });
  await assertInfoBox(null);

  gBrowser.removeCurrentTab();
  await SpecialPowers.popPrefEnv();
});

// Clicks the learn-more links and checks the help page is opened in a new tab.
add_task(async function clickLearnMore() {
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });

  let doc = gBrowser.selectedBrowser.contentDocument;
  let addressBarSection = doc.getElementById("locationBarGroup");
  addressBarSection.scrollIntoView();

  // Set initial state so that the info box and learn more link are shown.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.quicksuggest.nonsponsored", true],
      ["browser.urlbar.suggest.quicksuggest.sponsored", true],
      ["browser.urlbar.quicksuggest.dataCollection.enabled", true],
    ],
  });
  await assertInfoBox("addressbar-firefox-suggest-info-all");

  let learnMoreLinks = doc.querySelectorAll("." + LEARN_MORE_CLASS);
  Assert.equal(
    learnMoreLinks.length,
    2,
    "Expected number of learn-more links are present"
  );
  for (let link of learnMoreLinks) {
    Assert.ok(
      BrowserTestUtils.is_visible(link),
      "Learn-more link is visible: " + link.id
    );
  }

  let prefsTab = gBrowser.selectedTab;
  for (let link of learnMoreLinks) {
    let tabPromise = BrowserTestUtils.waitForNewTab(
      gBrowser,
      UrlbarProviderQuickSuggest.helpUrl
    );
    info("Clicking learn-more link: " + link.id);
    Assert.ok(link.id, "Sanity check: Learn-more link has an ID");
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#" + link.id,
      {},
      gBrowser.selectedBrowser
    );
    info("Waiting for help page to load in a new tab");
    await tabPromise;
    gBrowser.removeCurrentTab();
    gBrowser.selectedTab = prefsTab;
  }

  gBrowser.removeCurrentTab();
  await SpecialPowers.popPrefEnv();
});

/**
 * Verifies the state of the checkboxes (which are styled as toggle switches).
 *
 * @param {object} checkedByElementID
 *   Maps checkbox element IDs to booleans. Each boolean is the expected checked
 *   state of the corresponding ID.
 */
function assertCheckboxes(checkedByElementID) {
  let doc = gBrowser.selectedBrowser.contentDocument;
  let container = doc.getElementById(CONTAINER_ID);
  Assert.ok(BrowserTestUtils.is_visible(container), "The container is visible");
  for (let [id, checked] of Object.entries(checkedByElementID)) {
    let checkbox = doc.getElementById(id);
    Assert.equal(
      checkbox.checked,
      checked,
      "Checkbox checked state for ID: " + id
    );
  }
}

/**
 * Verifies the state of the info box.
 *
 * @param {string} expectedL10nID
 *   The l10n ID of the string that should be visible in the info box, null if
 *   the info box should be hidden.
 */
async function assertInfoBox(expectedL10nID) {
  info("Checking info box with expected l10n ID: " + expectedL10nID);
  let doc = gBrowser.selectedBrowser.contentDocument;
  let infoBox = doc.getElementById(INFO_BOX_ID);
  await TestUtils.waitForCondition(
    () => BrowserTestUtils.is_visible(infoBox) == !!expectedL10nID,
    "Waiting for expected info box visibility: " + !!expectedL10nID
  );

  let infoIcon = infoBox.querySelector(".info-icon");
  Assert.equal(
    BrowserTestUtils.is_visible(infoIcon),
    !!expectedL10nID,
    "The info icon is visible iff a description should be shown"
  );

  let learnMore = infoBox.querySelector("." + LEARN_MORE_CLASS);
  Assert.ok(learnMore, "Found the info box learn more link");
  Assert.equal(
    BrowserTestUtils.is_visible(learnMore),
    !!expectedL10nID,
    "The info box learn more link is visible iff a description should be shown"
  );

  if (expectedL10nID) {
    let infoText = doc.getElementById(INFO_TEXT_ID);
    Assert.equal(
      infoText.dataset.l10nId,
      expectedL10nID,
      "Info text has expected l10n ID"
    );
  }
}
