/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/translations/tests/browser/shared-head.js",
  this
);

/**
 * Returns the intl display name of a given language tag.
 *
 * @param {string} langTag - The BCP-47 language tag.
 */
const getIntlDisplayName = (() => {
  let displayNames = null;

  return langTag => {
    if (!displayNames) {
      displayNames = new Services.intl.DisplayNames(undefined, {
        type: "language",
        fallback: "none",
      });
    }
    return displayNames.of(langTag);
  };
})();

/**
 * Assert some property about the translations button.
 *
 * @param {Record<string, boolean>} visibleAssertions
 * @param {string} message The message for the assertion.
 * @returns {HTMLElement}
 */
async function assertTranslationsButton(visibleAssertions, message) {
  const elements = {
    button: document.getElementById("translations-button"),
    icon: document.getElementById("translations-button-icon"),
    circleArrows: document.getElementById("translations-button-circle-arrows"),
    locale: document.getElementById("translations-button-locale"),
  };

  for (const [name, element] of Object.entries(elements)) {
    if (!element) {
      throw new Error("Could not find the " + name);
    }
  }

  try {
    // Test that the visibilities match.
    await waitForCondition(() => {
      for (const [name, visible] of Object.entries(visibleAssertions)) {
        if (elements[name].hidden === visible) {
          return false;
        }
      }
      return true;
    }, message);
  } catch (error) {
    // On a mismatch, report it.
    for (const [name, expected] of Object.entries(visibleAssertions)) {
      is(!elements[name].hidden, expected, `Visibility for "${name}"`);
    }
  }

  ok(true, message);

  return elements;
}

/**
 * A convenience function to open the translations panel settings
 * menu by clicking on the translations button.
 *
 * Fails the test if the menu cannot be opened.
 */
async function openTranslationsSettingsMenuViaTranslationsButton() {
  await closeTranslationsPanelIfOpen();

  const { button } = await assertTranslationsButton(
    { button: true },
    "The button is available."
  );

  await waitForTranslationsPopupEvent("popupshown", () => {
    click(button, "Opening the popup");
  });

  const gearIcons = getAllByL10nId("translations-panel-settings-button");
  for (const gearIcon of gearIcons) {
    if (gearIcon.hidden) {
      continue;
    }
    click(gearIcon, "Open the settings menu");
    info("Waiting for settings menu to open.");
    const manageLanguages = await waitForCondition(() =>
      maybeGetByL10nId("translations-panel-settings-manage-languages")
    );
    ok(
      manageLanguages,
      "The manage languages item should be visible in the settings menu."
    );
    return;
  }
}

/**
 * A convenience function to open the translations panel settings
 * menu through the app menu.
 *
 * Fails the test if the menu cannot be opened.
 */
async function openTranslationsSettingsMenuViaAppMenu() {
  await openTranslationsPanelViaAppMenu();
  const gearIcons = getAllByL10nId("translations-panel-settings-button");
  for (const gearIcon of gearIcons) {
    if (gearIcon.hidden) {
      continue;
    }
    click(gearIcon, "Open the settings menu");
    info("Waiting for settings menu to open.");
    const manageLanguages = await waitForCondition(() =>
      maybeGetByL10nId("translations-panel-settings-manage-languages")
    );
    ok(
      manageLanguages,
      "The manage languages item should be visible in the settings menu."
    );
    return;
  }
}

/**
 * A convenience function to open the translations panel
 * through the app menu.
 *
 * Fails the test if the menu cannot be opened.
 */
async function openTranslationsPanelViaAppMenu() {
  await closeTranslationsPanelIfOpen();
  const appMenuButton = getById("PanelUI-menu-button");
  click(appMenuButton, "Opening the app-menu button");
  await BrowserTestUtils.waitForEvent(window.PanelUI.mainView, "ViewShown");

  const translateSiteButton = getById("appMenu-translate-button");

  is(
    translateSiteButton.disabled,
    false,
    "The app-menu translate button should be enabled"
  );

  await waitForTranslationsPopupEvent("popupshown", () => {
    click(translateSiteButton);
  });
}

/**
 * Simulates the effect of clicking the always-translate-language menuitem.
 * Requires that the settings menu of the translations panel is open,
 * otherwise the test will fail.
 */
async function toggleAlwaysTranslateLanguage() {
  const alwaysTranslateLanguage = getByL10nId(
    "translations-panel-settings-always-translate-language"
  );
  info("Toggle the always-translate-language menuitem");
  await alwaysTranslateLanguage.doCommand();
  await closeSettingsMenuIfOpen();
}

/**
 * Simulates the effect of clicking the never-translate-language menuitem.
 * Requires that the settings menu of the translations panel is open,
 * otherwise the test will fail.
 */
async function toggleNeverTranslateLanguage() {
  const neverTranslateLanguage = getByL10nId(
    "translations-panel-settings-never-translate-language"
  );
  info("Toggle the never-translate-language menuitem");
  await neverTranslateLanguage.doCommand();
  await closeSettingsMenuIfOpen();
}

/**
 * Simulates the effect of clicking the never-translate-site menuitem.
 * Requires that the settings menu of the translations panel is open,
 * otherwise the test will fail.
 */
async function toggleNeverTranslateSite() {
  const neverTranslateSite = getByL10nId(
    "translations-panel-settings-never-translate-site"
  );
  info("Toggle the never-translate-site menuitem");
  await neverTranslateSite.doCommand();
  await closeSettingsMenuIfOpen();
}

/**
 * Asserts that the always-translate-language checkbox matches the expected checked state.
 *
 * @param {string} langTag - A BCP-47 language tag
 * @param {object} expectations
 * @param {boolean} expectations.checked - Whether the checkbox is expected to be checked.
 * @param {boolean} expectations.disabled - Whether the menuitem is expected to be disabled.
 */
async function assertIsAlwaysTranslateLanguage(
  langTag,
  { checked = true, disabled = false }
) {
  await assertCheckboxState(
    "translations-panel-settings-always-translate-language",
    { langTag, checked, disabled }
  );
}

/**
 * Asserts that the never-translate-language checkbox matches the expected checked state.
 *
 * @param {string} langTag - A BCP-47 language tag
 * @param {object} expectations
 * @param {boolean} expectations.checked - Whether the checkbox is expected to be checked.
 * @param {boolean} expectations.disabled - Whether the menuitem is expected to be disabled.
 */
async function assertIsNeverTranslateLanguage(
  langTag,
  { checked = true, disabled = false }
) {
  await assertCheckboxState(
    "translations-panel-settings-never-translate-language",
    { langTag, checked, disabled }
  );
}

/**
 * Asserts that the never-translate-site checkbox matches the expected checked state.
 *
 * @param {string} url - The url of a website
 * @param {object} expectations
 * @param {boolean} expectations.checked - Whether the checkbox is expected to be checked.
 * @param {boolean} expectations.disabled - Whether the menuitem is expected to be disabled.
 */
async function assertIsNeverTranslateSite(
  url,
  { checked = true, disabled = false }
) {
  await assertCheckboxState(
    "translations-panel-settings-never-translate-site",
    { checked, disabled }
  );
}

/**
 * Asserts that the state of a checkbox with a given dataL10nId is
 * checked or not, based on the value of expected being true or false.
 *
 * @param {string} dataL10nId - The data-l10n-id of the checkbox.
 * @param {object} expectations
 * @param {string} expectations.langTag - The BCP-47 language tag.
 * @param {boolean} expectations.checked - Whether the checkbox is expected to be checked.
 * @param {boolean} expectations.disabled - Whether the menuitem is expected to be disabled.
 */
async function assertCheckboxState(
  dataL10nId,
  { langTag = null, checked = true, disabled = false }
) {
  const menuItems = getAllByL10nId(dataL10nId);
  for (const menuItem of menuItems) {
    if (langTag) {
      const {
        args: { language },
      } = document.l10n.getAttributes(menuItem);
      is(
        language,
        getIntlDisplayName(langTag),
        `Should match expected language display name for ${dataL10nId}`
      );
    }
    is(
      menuItem.disabled,
      disabled,
      `Should match expected disabled state for ${dataL10nId}`
    );
    await waitForCondition(
      () => menuItem.getAttribute("checked") === (checked ? "true" : "false"),
      "Waiting for checkbox state"
    );
    is(
      menuItem.getAttribute("checked"),
      checked ? "true" : "false",
      `Should match expected checkbox state for ${dataL10nId}`
    );
  }
}

/**
 * Asserts that for each provided expectation, the visible state of the corresponding
 * element in TranslationsPanel.elements both exists and matches the visibility expectation.
 *
 * @param {object} expectations
 *   A list of expectations for the visibility of any subset of TranslationsPanel.elements
 */
function assertPanelElementVisibility(expectations = {}) {
  // Assume nothing is visible by default, and overwrite them
  // with any specific expectations provided in the argument.
  const finalExpectations = {
    cancelButton: false,
    changeSourceLanguageButton: false,
    dismissErrorButton: false,
    error: false,
    fromMenuList: false,
    fromLabel: false,
    header: false,
    intro: false,
    langSelection: false,
    restoreButton: false,
    toLabel: false,
    toMenuList: false,
    translateButton: false,
    unsupportedHint: false,
    ...expectations,
  };
  const elements = TranslationsPanel.elements;
  for (const propertyName in finalExpectations) {
    ok(
      elements.hasOwnProperty(propertyName),
      `Expected translations panel elements to have property ${propertyName}`
    );
    if (finalExpectations.hasOwnProperty(propertyName)) {
      is(
        isVisible(elements[propertyName]),
        finalExpectations[propertyName],
        `The element "${propertyName}" visibility should match the expectation`
      );
    }
  }
}

/**
 * Asserts that the mainViewId of the panel matches the given string.
 *
 * @param {string} expectedId
 */
function assertPanelMainViewId(expectedId) {
  const mainViewId =
    TranslationsPanel.elements.multiview.getAttribute("mainViewId");
  is(
    mainViewId,
    expectedId,
    "The TranslationsPanel mainViewId should match its expected value"
  );
}

/**
 * A collection of element visibility expectations for the default panel view.
 */
const defaultViewVisibilityExpectations = {
  cancelButton: true,
  fromMenuList: true,
  fromLabel: true,
  header: true,
  langSelection: true,
  toMenuList: true,
  toLabel: true,
  translateButton: true,
};

/**
 * Asserts that panel element visibility matches the default panel view.
 */
function assertPanelDefaultView() {
  assertPanelMainViewId("translations-panel-view-default");
  assertPanelElementVisibility({
    ...defaultViewVisibilityExpectations,
  });
}

/**
 * Asserts that panel element visibility matches the panel error view.
 */
function assertPanelErrorView() {
  assertPanelMainViewId("translations-panel-view-default");
  assertPanelElementVisibility({
    error: true,
    ...defaultViewVisibilityExpectations,
  });
}

/**
 * Asserts that panel element visibility matches the panel first-show view.
 */
function assertPanelFirstShowView() {
  assertPanelMainViewId("translations-panel-view-default");
  assertPanelElementVisibility({
    intro: true,
    ...defaultViewVisibilityExpectations,
  });
}

/**
 * Asserts that panel element visibility matches the panel revisit view.
 */
function assertPanelRevisitView() {
  assertPanelMainViewId("translations-panel-view-default");
  assertPanelElementVisibility({
    header: true,
    langSelection: true,
    restoreButton: true,
    toLabel: true,
    toMenuList: true,
    translateButton: true,
  });
}

/**
 * Asserts that panel element visibility matches the panel unsupported language view.
 */
function assertPanelUnsupportedLanguageView() {
  assertPanelMainViewId("translations-panel-view-unsupported-language");
  assertPanelElementVisibility({
    changeSourceLanguageButton: true,
    dismissErrorButton: true,
    unsupportedHint: true,
  });
}

/**
 * Navigate to a URL and indicate a message as to why.
 */
async function navigate(url, message) {
  // When the translations panel is open from the app menu,
  // it doesn't close on navigate the way that it does when it's
  // open from the translations button, so ensure that we always
  // close it when we navigate to a new page.
  await closeTranslationsPanelIfOpen();

  info(message);

  // Load a blank page first to ensure that tests don't hang.
  // I don't know why this is needed, but it appears to be necessary.
  BrowserTestUtils.loadURIString(gBrowser.selectedBrowser, BLANK_PAGE);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  BrowserTestUtils.loadURIString(gBrowser.selectedBrowser, url);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
}

/**
 * Add a tab to the page
 *
 * @param {string} url
 */
async function addTab(url) {
  info(`Adding tab for ` + url);
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    url,
    true // Wait for laod
  );
  return {
    tab,
    removeTab() {
      BrowserTestUtils.removeTab(tab);
    },
  };
}

async function switchTab(tab) {
  info("Switching tabs");
  await BrowserTestUtils.switchTab(gBrowser, tab);
}

function click(button, message) {
  info(message);
  EventUtils.synthesizeMouseAtCenter(button, {});
}

/**
 * @param {Element} element
 * @returns {boolean}
 */
function isVisible(element) {
  if (element.offsetParent === null) {
    return false;
  }
  const win = element.ownerDocument.ownerGlobal;
  const { visibility, display } = win.getComputedStyle(element);
  return visibility === "visible" && display !== "none";
}

/**
 * Get an element by its l10n id, as this is a user-visible way to find an element.
 * The `l10nId` represents the text that a user would actually see.
 *
 * @param {string} l10nId
 * @param {Document} doc
 * @returns {Element}
 */
function getByL10nId(l10nId, doc = document) {
  const elements = doc.querySelectorAll(`[data-l10n-id="${l10nId}"]`);
  if (elements.length === 0) {
    throw new Error("Could not find the element by l10n id: " + l10nId);
  }
  for (const element of elements) {
    if (isVisible(element)) {
      return element;
    }
  }
  throw new Error("The element is not visible in the DOM: " + l10nId);
}

/**
 * Get all elements that match the l10n id.
 *
 * @param {string} l10nId
 * @param {Document} doc
 * @returns {Element}
 */
function getAllByL10nId(l10nId, doc = document) {
  const elements = doc.querySelectorAll(`[data-l10n-id="${l10nId}"]`);
  console.log(doc);
  if (elements.length === 0) {
    throw new Error("Could not find the element by l10n id: " + l10nId);
  }
  return elements;
}

/**
 * @param {string} id
 * @param {Document} [doc]
 * @returns {Element}
 */
function getById(id, doc = document) {
  const element = doc.getElementById(id);
  if (!element) {
    throw new Error("Could not find the element by id: #" + id);
  }
  if (isVisible(element)) {
    return element;
  }
  throw new Error("The element is not visible in the DOM: #" + id);
}

/**
 * A non-throwing version of `getByL10nId`.
 *
 * @param {string} l10nId
 * @returns {Element | null}
 */
function maybeGetByL10nId(l10nId, doc = document) {
  const selector = `[data-l10n-id="${l10nId}"]`;
  const elements = doc.querySelectorAll(selector);
  for (const element of elements) {
    if (isVisible(element)) {
      return element;
    }
  }
  return null;
}

/**
 * XUL popups will fire the popupshown and popuphidden events. These will fire for
 * any type of popup in the browser. This function waits for one of those events, and
 * checks that the viewId of the popup is PanelUI-profiler
 *
 * @param {"popupshown" | "popuphidden"} eventName
 * @param {Function} callback
 * @param {Function} postEventAssertion
 *   An optional assertion to be made immediately after the event occurs.
 * @returns {Promise<void>}
 */
async function waitForTranslationsPopupEvent(
  eventName,
  callback,
  postEventAssertion = null
) {
  // De-lazify the panel elements.
  TranslationsPanel.elements;
  const panel = document.getElementById("translations-panel");
  if (!panel) {
    throw new Error("Unable to find the translations panel element.");
  }
  const promise = BrowserTestUtils.waitForEvent(panel, eventName);
  await callback();
  info("Waiting for the translations panel popup to be shown");
  await promise;
  if (postEventAssertion) {
    postEventAssertion();
  }
  // Wait a single tick on the event loop.
  await new Promise(resolve => setTimeout(resolve, 0));
}

/**
 * When switching between between views in the popup panel, wait for the view to
 * be fully shown.
 *
 * @param {Function} callback
 */
async function waitForViewShown(callback) {
  const panel = document.getElementById("translations-panel");
  if (!panel) {
    throw new Error("Unable to find the translations panel element.");
  }
  const promise = BrowserTestUtils.waitForEvent(panel, "ViewShown");
  callback();
  info("Waiting for the translations panel view to be shown");
  await promise;
  await new Promise(resolve => setTimeout(resolve, 0));
}

const FRENCH_PAGE_URL = TRANSLATIONS_TESTER_FR;
const ENGLISH_PAGE_URL = TRANSLATIONS_TESTER_EN;
const SPANISH_PAGE_URL = TRANSLATIONS_TESTER_ES;
const SPANISH_PAGE_URL_2 = TRANSLATIONS_TESTER_ES_2;
const SPANISH_PAGE_URL_DOT_ORG = TRANSLATIONS_TESTER_ES_DOT_ORG;
const LANGUAGE_PAIRS = [
  { fromLang: "es", toLang: "en" },
  { fromLang: "en", toLang: "es" },
  { fromLang: "fr", toLang: "en" },
  { fromLang: "en", toLang: "fr" },
  { fromLang: "en", toLang: "uk" },
  { fromLang: "uk", toLang: "en" },
];
