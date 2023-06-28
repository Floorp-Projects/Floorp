/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/translations/tests/browser/shared-head.js",
  this
);

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

  const gearIcon = getByL10nId("translations-panel-settings-button");
  click(gearIcon, "Open the settings menu");
}

/**
 * A convenience function to open the translations panel settings
 * menu through the app menu.
 *
 * Fails the test if the menu cannot be opened.
 */
async function openTranslationsSettingsMenuViaAppMenu() {
  await openTranslationsPanelViaAppMenu();
  const gearIcon = getByL10nId("translations-panel-settings-button");
  click(gearIcon, "Open the settings menu");
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
}

/**
 * Asserts that the always-translate-language checkbox matches the expected checked state.
 *
 * @param {string} langTag - A BCP-47 language tag
 * @param {boolean} expectChecked - Whether the checkbox should be checked
 */
async function assertIsAlwaysTranslateLanguage(langTag, expectChecked) {
  await assertCheckboxState(
    "translations-panel-settings-always-translate-language",
    expectChecked
  );
}

/**
 * Asserts that the never-translate-language checkbox matches the expected checked state.
 *
 * @param {string} langTag - A BCP-47 language tag
 * @param {boolean} expectChecked - Whether the checkbox should be checked
 */
async function assertIsNeverTranslateLanguage(langTag, expectChecked) {
  await assertCheckboxState(
    "translations-panel-settings-never-translate-language",
    expectChecked
  );
}

/**
 * Asserts that the never-translate-site checkbox matches the expected checked state.
 *
 * @param {string} url - The url of a website
 * @param {boolean} expectChecked - Whether the checkbox should be checked
 */
async function assertIsNeverTranslateSite(url, expectChecked) {
  await assertCheckboxState(
    "translations-panel-settings-never-translate-site",
    expectChecked
  );
}

/**
 * Asserts that the state of a checkbox with a given dataL10nId is
 * checked or not, based on the value of expected being true or false.
 *
 * @param {string} dataL10nId - The data-l10n-id of the checkbox.
 * @param {boolean} expectChecked - Whether the checkbox should be checked.
 */
async function assertCheckboxState(dataL10nId, expectChecked) {
  const menuItems = getAllByL10nId(dataL10nId);
  for (const menuItem of menuItems) {
    await waitForCondition(
      () =>
        menuItem.getAttribute("checked") === (expectChecked ? "true" : "false"),
      "Waiting for checkbox state"
    );
    is(
      menuItem.getAttribute("checked"),
      expectChecked ? "true" : "false",
      `Should match expected checkbox state for ${dataL10nId}`
    );
  }
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
 * @returns {Promise<void>}
 */
async function waitForTranslationsPopupEvent(eventName, callback) {
  const panel = document.getElementById("translations-panel");
  if (!panel) {
    throw new Error("Unable to find the translations panel element.");
  }
  const promise = BrowserTestUtils.waitForEvent(panel, eventName);
  callback();
  info("Waiting for the translations panel popup to be shown");
  await promise;
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

const ENGLISH_PAGE_URL = TRANSLATIONS_TESTER_EN;
const SPANISH_PAGE_URL = TRANSLATIONS_TESTER_ES;
const SPANISH_PAGE_URL_2 = TRANSLATIONS_TESTER_ES_2;
const SPANISH_PAGE_URL_DOT_ORG = TRANSLATIONS_TESTER_ES_DOT_ORG;
const LANGUAGE_PAIRS = [
  { fromLang: "es", toLang: "en", isBeta: false },
  { fromLang: "en", toLang: "es", isBeta: false },
  { fromLang: "fr", toLang: "en", isBeta: false },
  { fromLang: "en", toLang: "fr", isBeta: false },
  { fromLang: "en", toLang: "uk", isBeta: true },
  { fromLang: "uk", toLang: "en", isBeta: true },
];
