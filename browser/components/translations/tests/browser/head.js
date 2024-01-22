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
 * @param {string} langTag - A BCP-47 language tag.
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
 * Asserts that the Spanish test page is untranslated by checking
 * that the H1 element is still in its original Spanish form.
 *
 * @param {Function} runInPage - Allows running a closure in the content page.
 * @param {string} message - An optional message to log to info.
 */
async function assertPageIsUntranslated(runInPage, message = null) {
  if (message) {
    info(message);
  }
  info("Checking that the page is untranslated");
  await runInPage(async TranslationsTest => {
    const { getH1 } = TranslationsTest.getSelectors();
    await TranslationsTest.assertTranslationResult(
      "The page's H1 is untranslated and in the original Spanish.",
      getH1,
      "Don Quijote de La Mancha"
    );
  });
}

/**
 * Asserts that the Spanish test page has been translated by checking
 * that the H1 element has been modified from its original form.
 *
 * @param {string} fromLanguage - The BCP-47 language tag being translated from.
 * @param {string} toLanguage - The BCP-47 language tag being translated into.
 * @param {Function} runInPage - Allows running a closure in the content page.
 * @param {string} message - An optional message to log to info.
 */
async function assertPageIsTranslated(
  fromLanguage,
  toLanguage,
  runInPage,
  message = null
) {
  if (message) {
    info(message);
  }
  info("Checking that the page is translated");
  const callback = async (TranslationsTest, { fromLang, toLang }) => {
    const { getH1 } = TranslationsTest.getSelectors();
    await TranslationsTest.assertTranslationResult(
      "The page's H1 is translated.",
      getH1,
      `DON QUIJOTE DE LA MANCHA [${fromLang} to ${toLang}, html]`
    );
  };
  await runInPage(callback, { fromLang: fromLanguage, toLang: toLanguage });
  await assertLangTagIsShownOnTranslationsButton(fromLanguage, toLanguage);
}

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
 * Opens the translations panel.
 *
 * @param {object} config
 * @param {Function} config.onOpenPanel
 *  - A function to run as soon as the panel opens.
 * @param {boolean} config.openFromAppMenu
 *  - Open the panel from the app menu. If false, uses the translations button.
 * @param {boolean} config.openWithKeyboard
 *  - Open the panel by synthesizing the keyboard. If false, synthesizes the mouse.
 */
async function openTranslationsPanel({
  onOpenPanel = null,
  openFromAppMenu = false,
  openWithKeyboard = false,
}) {
  logAction();
  await closeTranslationsPanelIfOpen();
  if (openFromAppMenu) {
    await openTranslationsPanelViaAppMenu({ onOpenPanel, openWithKeyboard });
  } else {
    await openTranslationsPanelViaTranslationsButton({
      onOpenPanel,
      openWithKeyboard,
    });
  }
}

/**
 * Opens the translations panel via the translations button.
 *
 * @param {object} config
 * @param {Function} config.onOpenPanel
 *  - A function to run as soon as the panel opens.
 * @param {boolean} config.openWithKeyboard
 *  - Open the panel by synthesizing the keyboard. If false, synthesizes the mouse.
 */
async function openTranslationsPanelViaTranslationsButton({
  onOpenPanel = null,
  openWithKeyboard = false,
}) {
  logAction();
  const { button } = await assertTranslationsButton(
    { button: true },
    "The translations button is visible."
  );
  await waitForTranslationsPopupEvent(
    "popupshown",
    () => {
      if (openWithKeyboard) {
        hitEnterKey(button, "Opening the popup with keyboard");
      } else {
        click(button, "Opening the popup");
      }
    },
    onOpenPanel
  );
}

/**
 * Provide a uniform way to log actions. This abuses the Error stack to get the callers
 * of the action. This should help in test debugging.
 */
function logAction(...params) {
  const error = new Error();
  const stackLines = error.stack.split("\n");
  const actionName = stackLines[1]?.split("@")[0] ?? "";
  const taskFileLocation = stackLines[2]?.split("@")[1] ?? "";
  if (taskFileLocation.includes("head.js")) {
    // Only log actions that were done at the test level.
    return;
  }

  info(`Action: ${actionName}(${params.join(", ")})`);
  info(
    `Source: ${taskFileLocation.replace(
      "chrome://mochitests/content/browser/",
      ""
    )}`
  );
}

/**
 * Opens the translations panel settings menu.
 * Requires that the translations panel is already open.
 */
async function openTranslationsSettingsMenu() {
  logAction();
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
 * Opens the translations panel via the app menu.
 *
 * @param {object} config
 * @param {Function} config.onOpenPanel
 *  - A function to run as soon as the panel opens.
 * @param {boolean} config.openWithKeyboard
 *  - Open the panel by synthesizing the keyboard. If false, synthesizes the mouse.
 */
async function openTranslationsPanelViaAppMenu({
  onOpenPanel = null,
  openWithKeyboard = false,
}) {
  logAction();
  const appMenuButton = getById("PanelUI-menu-button");
  if (openWithKeyboard) {
    hitEnterKey(appMenuButton, "Opening the app-menu button with keyboard");
  } else {
    click(appMenuButton, "Opening the app-menu button");
  }
  await BrowserTestUtils.waitForEvent(window.PanelUI.mainView, "ViewShown");

  const translateSiteButton = getById("appMenu-translate-button");

  is(
    translateSiteButton.disabled,
    false,
    "The app-menu translate button should be enabled"
  );

  await waitForTranslationsPopupEvent(
    "popupshown",
    () => {
      if (openWithKeyboard) {
        hitEnterKey(translateSiteButton, "Opening the popup with keyboard");
      } else {
        click(translateSiteButton, "Opening the popup");
      }
    },
    onOpenPanel
  );
}

/**
 * Switches the selected from-language to the provided language tag.
 *
 * @param {string} langTag - A BCP-47 language tag.
 */
function switchSelectedFromLanguage(langTag) {
  logAction(langTag);
  const { fromMenuList } = TranslationsPanel.elements;
  fromMenuList.value = langTag;
  fromMenuList.dispatchEvent(new Event("command"));
}

/**
 * Asserts that the selected from-language matches the provided language tag.
 *
 * @param {string} langTag - A BCP-47 language tag.
 */
function assertSelectedFromLanguage(langTag) {
  info(`Checking that the selected from-language matches ${langTag}`);
  const { fromMenuList } = TranslationsPanel.elements;
  is(
    fromMenuList.value,
    langTag,
    "Expected selected from-language to match the given language tag"
  );
}

/**
 * Switches the selected to-language to the provided language tag.
 *
 * @param {string} langTag - A BCP-47 language tag.
 */
function switchSelectedToLanguage(langTag) {
  logAction(langTag);
  const { toMenuList } = TranslationsPanel.elements;
  toMenuList.value = langTag;
  toMenuList.dispatchEvent(new Event("command"));
}

/**
 * Asserts that the selected to-language matches the provided language tag.
 *
 * @param {string} langTag - A BCP-47 language tag.
 */
function assertSelectedToLanguage(langTag) {
  info(`Checking that the selected to-language matches ${langTag}`);
  const { toMenuList } = TranslationsPanel.elements;
  is(
    toMenuList.value,
    langTag,
    "Expected selected to-language to match the given language tag"
  );
}

/*
 * Simulates the effect of toggling a menu item in the translations panel
 * settings menu. Requires that the settings menu is currently open,
 * otherwise the test will fail.
 */
async function clickSettingsMenuItemByL10nId(l10nId) {
  logAction(l10nId);
  info(`Toggling the "${l10nId}" settings menu item.`);
  click(getByL10nId(l10nId), `Clicking the "${l10nId}" settings menu item.`);
  await closeSettingsMenuIfOpen();
}

/**
 * Simulates the effect of clicking the always-offer-translations menuitem.
 * Requires that the settings menu of the translations panel is open,
 * otherwise the test will fail.
 */
async function clickAlwaysOfferTranslations() {
  logAction();
  await clickSettingsMenuItemByL10nId(
    "translations-panel-settings-always-offer-translation"
  );
}

/**
 * Simulates the effect of clicking the always-translate-language menuitem.
 * Requires that the settings menu of the translations panel is open,
 * otherwise the test will fail.
 */
async function clickAlwaysTranslateLanguage({
  downloadHandler = null,
  pivotTranslation = false,
} = {}) {
  logAction();
  await clickSettingsMenuItemByL10nId(
    "translations-panel-settings-always-translate-language"
  );
  if (downloadHandler) {
    await assertTranslationsButton(
      { button: true, circleArrows: true, locale: false, icon: true },
      "The icon presents the loading indicator."
    );
    await downloadHandler(pivotTranslation ? 2 : 1);
  }
}

/**
 * Simulates the effect of clicking the never-translate-language menuitem.
 * Requires that the settings menu of the translations panel is open,
 * otherwise the test will fail.
 */
async function clickNeverTranslateLanguage() {
  logAction();
  await clickSettingsMenuItemByL10nId(
    "translations-panel-settings-never-translate-language"
  );
}

/**
 * Simulates the effect of clicking the never-translate-site menuitem.
 * Requires that the settings menu of the translations panel is open,
 * otherwise the test will fail.
 */
async function clickNeverTranslateSite() {
  logAction();
  await clickSettingsMenuItemByL10nId(
    "translations-panel-settings-never-translate-site"
  );
}

/**
 * Simulates the effect of clicking the manage-languages menuitem.
 * Requires that the settings menu of the translations panel is open,
 * otherwise the test will fail.
 */
async function clickManageLanguages() {
  logAction();
  await clickSettingsMenuItemByL10nId(
    "translations-panel-settings-manage-languages"
  );
}

/**
 * Asserts that the always-offer-translations checkbox matches the expected checked state.
 *
 * @param {boolean} checked
 */
async function assertIsAlwaysOfferTranslationsEnabled(checked) {
  info(
    `Checking that always-offer-translations is ${
      checked ? "enabled" : "disabled"
    }`
  );
  await assertCheckboxState(
    "translations-panel-settings-always-offer-translation",
    { checked }
  );
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
  info(
    `Checking that always-translate is ${
      checked ? "enabled" : "disabled"
    } for "${langTag}"`
  );
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
  info(
    `Checking that never-translate is ${
      checked ? "enabled" : "disabled"
    } for "${langTag}"`
  );
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
  info(
    `Checking that never-translate is ${
      checked ? "enabled" : "disabled"
    } for "${url}"`
  );
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
 * @param {string} expectations.langTag - A BCP-47 language tag.
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
 * Asserts that the proper language tags are shown on the translations button.
 *
 * @param {string} fromLanguage - The BCP-47 language tag being translated from.
 * @param {string} toLanguage - The BCP-47 language tag being translated into.
 */
async function assertLangTagIsShownOnTranslationsButton(
  fromLanguage,
  toLanguage
) {
  info(
    `Ensuring that the translations button displays the language tag "${toLanguage}"`
  );
  const { button, locale } = await assertTranslationsButton(
    { button: true, circleArrows: false, locale: true, icon: true },
    "The icon presents the locale."
  );
  is(
    locale.innerText,
    toLanguage,
    `The expected language tag "${toLanguage}" is shown.`
  );
  is(
    button.getAttribute("data-l10n-id"),
    "urlbar-translations-button-translated"
  );
  const fromLangDisplay = getIntlDisplayName(fromLanguage);
  const toLangDisplay = getIntlDisplayName(toLanguage);
  is(
    button.getAttribute("data-l10n-args"),
    `{"fromLanguage":"${fromLangDisplay}","toLanguage":"${toLangDisplay}"}`
  );
}

/**
 * Simulates clicking the cancel button.
 */
async function clickCancelButton() {
  logAction();
  const { cancelButton } = TranslationsPanel.elements;
  assertIsVisible(true, { element: cancelButton });
  await waitForTranslationsPopupEvent("popuphidden", () => {
    click(cancelButton, "Clicking the cancel button");
  });
}

/**
 * Simulates clicking the restore-page button.
 */
async function clickRestoreButton() {
  logAction();
  const { restoreButton } = TranslationsPanel.elements;
  assertIsVisible(true, { element: restoreButton });
  await waitForTranslationsPopupEvent("popuphidden", () => {
    click(restoreButton, "Click the restore-page button");
  });
}

/**
 * Simulates clicking the dismiss-error button.
 */
async function clickDismissErrorButton() {
  logAction();
  const { dismissErrorButton } = TranslationsPanel.elements;
  assertIsVisible(true, { element: dismissErrorButton });
  await waitForTranslationsPopupEvent("popuphidden", () => {
    click(dismissErrorButton, "Click the dismiss-error button");
  });
}

/**
 * Simulates clicking the translate button.
 *
 * @param {object} config
 * @param {Function} config.downloadHandler
 *  - The function handle expected downloads, resolveDownloads() or rejectDownloads()
 *    Leave as null to test more granularly, such as testing opening the loading view,
 *    or allowing for the automatic downloading of files.
 * @param {boolean} config.pivotTranslation
 *  - True if the expected translation is a pivot translation, otherwise false.
 *    Affects the number of expected downloads.
 */
async function clickTranslateButton({
  downloadHandler = null,
  pivotTranslation = false,
} = {}) {
  logAction();
  const { translateButton } = TranslationsPanel.elements;
  assertIsVisible(true, { element: translateButton });
  await waitForTranslationsPopupEvent("popuphidden", () => {
    click(translateButton);
  });

  if (downloadHandler) {
    await assertTranslationsButton(
      { button: true, circleArrows: true, locale: false, icon: true },
      "The icon presents the loading indicator."
    );
    await downloadHandler(pivotTranslation ? 2 : 1);
  }
}

/**
 * Simulates clicking the change-source-language button.
 *
 * @param {object} config
 * @param {boolean} config.firstShow
 *  - True if the first-show view should be expected
 *    False if the default view should be expected
 */
async function clickChangeSourceLanguageButton({ firstShow = false } = {}) {
  logAction();
  const { changeSourceLanguageButton } = TranslationsPanel.elements;
  assertIsVisible(true, { element: changeSourceLanguageButton });
  await waitForTranslationsPopupEvent(
    "popupshown",
    () => {
      click(
        changeSourceLanguageButton,
        "Click the change-source-language button"
      );
    },
    firstShow ? assertPanelFirstShowView : assertPanelDefaultView
  );
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
    introLearnMoreLink: false,
    langSelection: false,
    restoreButton: false,
    toLabel: false,
    toMenuList: false,
    translateButton: false,
    unsupportedHeader: false,
    unsupportedHint: false,
    unsupportedLearnMoreLink: false,
    ...expectations,
  };
  const elements = TranslationsPanel.elements;
  for (const propertyName in finalExpectations) {
    ok(
      elements.hasOwnProperty(propertyName),
      `Expected translations panel elements to have property ${propertyName}`
    );
    if (finalExpectations.hasOwnProperty(propertyName)) {
      assertIsVisible(finalExpectations[propertyName], {
        element: elements[propertyName],
      });
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
 * Asserts that the TranslationsPanel header has the expected l10nId.
 *
 * @param {string} l10nId - The expected data-l10n-id of the header.
 */
function assertDefaultHeaderL10nId(l10nId) {
  const { header } = TranslationsPanel.elements;
  is(
    header.getAttribute("data-l10n-id"),
    l10nId,
    "The translations panel header should match the expected data-l10n-id"
  );
}

/**
 * Asserts that panel element visibility matches the default panel view.
 */
function assertPanelDefaultView() {
  info("Checking that the panel shows the default view");
  assertPanelMainViewId("translations-panel-view-default");
  assertPanelElementVisibility({
    ...defaultViewVisibilityExpectations,
  });
  assertDefaultHeaderL10nId("translations-panel-header");
}

/**
 * Asserts that the panel element visibility matches the panel loading view.
 */
function assertPanelLoadingView() {
  info("Checking that the panel shows the loading view");
  assertPanelDefaultView();
  const loadingButton = getByL10nId(
    "translations-panel-translate-button-loading"
  );
  ok(loadingButton, "The loading button is present");
  ok(loadingButton.disabled, "The loading button is disabled");
}

/**
 * Asserts that panel element visibility matches the panel error view.
 */
function assertPanelErrorView() {
  info("Checking that the panel shows the error view");
  assertPanelMainViewId("translations-panel-view-default");
  assertPanelElementVisibility({
    error: true,
    ...defaultViewVisibilityExpectations,
  });
  assertDefaultHeaderL10nId("translations-panel-header");
}

/**
 * Asserts that panel element visibility matches the panel first-show view.
 */
function assertPanelFirstShowView() {
  info("Checking that the panel shows the first-show view");
  assertPanelMainViewId("translations-panel-view-default");
  assertPanelElementVisibility({
    intro: true,
    introLearnMoreLink: true,
    ...defaultViewVisibilityExpectations,
  });
  assertDefaultHeaderL10nId("translations-panel-intro-header");
}

/**
 * Asserts that panel element visibility matches the panel first-show error view.
 */
function assertPanelFirstShowErrorView() {
  info("Checking that the panel shows the first-show error view");
  assertPanelMainViewId("translations-panel-view-default");
  assertPanelElementVisibility({
    error: true,
    intro: true,
    introLearnMoreLink: true,
    ...defaultViewVisibilityExpectations,
  });
  assertDefaultHeaderL10nId("translations-panel-intro-header");
}

/**
 * Asserts that panel element visibility matches the panel revisit view.
 */
function assertPanelRevisitView() {
  info("Checking that the panel shows the revisit view");
  assertPanelMainViewId("translations-panel-view-default");
  assertPanelElementVisibility({
    header: true,
    langSelection: true,
    restoreButton: true,
    toLabel: true,
    toMenuList: true,
    translateButton: true,
  });
  assertDefaultHeaderL10nId("translations-panel-revisit-header");
}

/**
 * Asserts that panel element visibility matches the panel unsupported language view.
 */
function assertPanelUnsupportedLanguageView() {
  info("Checking that the panel shows the unsupported-language view");
  assertPanelMainViewId("translations-panel-view-unsupported-language");
  assertPanelElementVisibility({
    changeSourceLanguageButton: true,
    dismissErrorButton: true,
    unsupportedHeader: true,
    unsupportedHint: true,
    unsupportedLearnMoreLink: true,
  });
}

/**
 * Navigate to a URL and indicate a message as to why.
 */
async function navigate(
  message,
  { url, onOpenPanel = null, downloadHandler = null, pivotTranslation = false }
) {
  logAction();
  // When the translations panel is open from the app menu,
  // it doesn't close on navigate the way that it does when it's
  // open from the translations button, so ensure that we always
  // close it when we navigate to a new page.
  await closeTranslationsPanelIfOpen();

  info(message);

  // Load a blank page first to ensure that tests don't hang.
  // I don't know why this is needed, but it appears to be necessary.
  BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, BLANK_PAGE);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  const loadTargetPage = async () => {
    BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, url);
    await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

    if (downloadHandler) {
      await assertTranslationsButton(
        { button: true, circleArrows: true, locale: false, icon: true },
        "The icon presents the loading indicator."
      );
      await downloadHandler(pivotTranslation ? 2 : 1);
    }
  };

  info(`Loading url: "${url}"`);
  if (onOpenPanel) {
    await waitForTranslationsPopupEvent(
      "popupshown",
      loadTargetPage,
      onOpenPanel
    );
  } else {
    await loadTargetPage();
  }
}

/**
 * Click the reader-mode button if the reader-mode button is available.
 * Fails if the reader-mode button is hidden.
 */
async function toggleReaderMode() {
  logAction();
  const readerButton = document.getElementById("reader-mode-button");
  await waitForCondition(() => readerButton.hidden === false);

  readerButton.getAttribute("readeractive")
    ? info("Exiting reader mode")
    : info("Entering reader mode");

  const readyPromise = readerButton.getAttribute("readeractive")
    ? waitForCondition(() => !readerButton.getAttribute("readeractive"))
    : BrowserTestUtils.waitForContentEvent(
        gBrowser.selectedBrowser,
        "AboutReaderContentReady"
      );

  click(readerButton, "Clicking the reader-mode button");
  await readyPromise;
}

/**
 * Opens a new tab in the foreground.
 *
 * @param {string} url
 */
async function addTab(url) {
  logAction(url);
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

/**
 * Switches to a given tab.
 *
 * @param {object} tab - The tab to switch to
 * @param {string} name
 */
async function switchTab(tab, name) {
  logAction("tab", name);
  gBrowser.selectedTab = tab;
  await new Promise(resolve => setTimeout(resolve, 0));
}

/**
 * Simulates clicking an element with the mouse.
 *
 * @param {element} element - The element to click.
 * @param {string} [message] - A message to log to info.
 */
function click(element, message) {
  logAction(message);
  return new Promise(resolve => {
    element.addEventListener(
      "click",
      function () {
        resolve();
      },
      { once: true }
    );

    EventUtils.synthesizeMouseAtCenter(element, {
      type: "mousedown",
      isSynthesized: false,
    });
    EventUtils.synthesizeMouseAtCenter(element, {
      type: "mouseup",
      isSynthesized: false,
    });
  });
}

/**
 * Returns whether an element's computed style is visible.
 *
 * @param {Element} element - The element to check.
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
 * Asserts the visibility state of an element retrieved by one of the three available options.
 *
 * @param {boolean} expected - The expected visibility state (true for visible, false for invisible).
 * @param {object} options - The element retrieval options
 * @param {Element} options.element - The HTML element to check visibility for.
 * @param {string} options.id - The Id of the element to retrieve and check visibility for.
 * @throws Throws if the visibility does not match the expected visibility state.
 */
function assertIsVisible(expected, { element, id }) {
  if (id && element) {
    throw new Error(
      "assertIsVisible() expects either an element or an id, but both were specified."
    );
  }

  if (element) {
    return is(
      isVisible(element),
      expected,
      `Expected element with id '${element.id}' to be ${
        expected ? "visible" : "invisible"
      }.`
    );
  }

  if (id) {
    return is(
      maybeGetById(id) !== null,
      expected,
      `Expected element with id '${id}' to be ${
        expected ? "visible" : "invisible"
      }.`
    );
  }

  throw new Error(
    "assertIsVisible() was called with no specified element or id."
  );
}

/**
 * Opens the context menu at a specified element on the page, based on the provided options.
 *
 * @param {Function} runInPage - A content-exposed function to run within the context of the page.
 * @param {object} options - Options for opening the context menu.
 * @param {boolean} options.selectFirstParagraph - Selects the first paragraph before opening the context menu.
 * @param {boolean} options.selectSpanishParagraph - Selects the Spanish paragraph before opening the context menu.
 *                                                   This is only available in SPANISH_TEST_PAGE.
 * @param {boolean} options.openAtFirstParagraph - Opens the context menu at the first paragraph in the test page.
 * @param {boolean} options.openAtSpanishParagraph - Opens the context menu at the Spanish paragraph in the test page.
 *                                                   This is only available in SPANISH_TEST_PAGE.
 * @param {boolean} options.openAtEnglishHyperlink - Opens the context menu at the English hyperlink in the test page.
 *                                                   This is only available in SPANISH_TEST_PAGE.
 * @param {boolean} options.openAtSpanishHyperlink - Opens the context menu at the Spanish hyperlink in the test page.
 *                                                   This is only available in SPANISH_TEST_PAGE.
 * @throws Throws an error if no valid option was provided for opening the menu.
 */
async function openContextMenu(
  runInPage,
  {
    selectFirstParagraph,
    selectSpanishParagraph,
    openAtFirstParagraph,
    openAtSpanishParagraph,
    openAtEnglishHyperlink,
    openAtSpanishHyperlink,
  }
) {
  logAction();

  if (selectFirstParagraph === true) {
    await runInPage(async TranslationsTest => {
      const { getFirstParagraph } = TranslationsTest.getSelectors();
      const paragraph = getFirstParagraph();
      TranslationsTest.selectContentElement(paragraph);
    });
  }

  if (selectSpanishParagraph === true) {
    await runInPage(async TranslationsTest => {
      const { getSpanishParagraph } = TranslationsTest.getSelectors();
      const paragraph = getSpanishParagraph();
      TranslationsTest.selectContentElement(paragraph);
    });
  }

  if (openAtFirstParagraph === true) {
    await runInPage(async TranslationsTest => {
      const { getFirstParagraph } = TranslationsTest.getSelectors();
      const paragraph = getFirstParagraph();
      await TranslationsTest.rightClickContentElement(paragraph);
    });
    return;
  }

  if (openAtSpanishParagraph === true) {
    await runInPage(async TranslationsTest => {
      const { getSpanishParagraph } = TranslationsTest.getSelectors();
      const paragraph = getSpanishParagraph();
      await TranslationsTest.rightClickContentElement(paragraph);
    });
    return;
  }

  if (openAtEnglishHyperlink === true) {
    await runInPage(async TranslationsTest => {
      const { getEnglishHyperlink } = TranslationsTest.getSelectors();
      const hyperlink = getEnglishHyperlink();
      await TranslationsTest.rightClickContentElement(hyperlink);
    });
    return;
  }

  if (openAtSpanishHyperlink === true) {
    await runInPage(async TranslationsTest => {
      const { getSpanishHyperlink } = TranslationsTest.getSelectors();
      const hyperlink = getSpanishHyperlink();
      await TranslationsTest.rightClickContentElement(hyperlink);
    });
    return;
  }

  throw new Error(
    "openContextMenu() was not provided a declaration for which element to open the menu at."
  );
}

/**
 * Opens the context menu then asserts properties of the translate-selection item in the context menu.
 *
 * @param {Function} runInPage - A content-exposed function to run within the context of the page.
 * @param {object} options - Options for how to open the context menu and what properties to assert about the translate-selection item.
 * @param {boolean} options.selectFirstParagraph - Selects the first paragraph before opening the context menu.
 * @param {boolean} options.selectSpanishParagraph - Selects the Spanish paragraph before opening the context menu.
 *                                                   This is only available in SPANISH_TEST_PAGE.
 * @param {boolean} options.expectMenuItemIsVisible - Whether the translate-selection item is expected to be visible.
 *                                                  Does not assert visibility if left undefined.
 * @param {string} options.expectedTargetLanguage - The target language for translation.
 * @param {boolean} options.openAtFirstParagraph - Opens the context menu at the first paragraph in the test page.
 * @param {boolean} options.openAtSpanishParagraph - Opens the context menu at the Spanish paragraph in the test page.
 *                                                   This is only available in SPANISH_TEST_PAGE.
 * @param {boolean} options.openAtEnglishHyperlink - Opens the context menu at the English hyperlink in the test page.
 *                                                   This is only available in SPANISH_TEST_PAGE.
 * @param {boolean} options.openAtSpanishHyperlink - Opens the context menu at the Spanish hyperlink in the test page.
 *                                                   This is only available in SPANISH_TEST_PAGE.
 * @param {string} [message] - A message to log to info.
 * @throws Throws an error if the properties of the translate-selection item do not match the expected options.
 */
async function assertContextMenuTranslateSelectionItem(
  runInPage,
  {
    selectFirstParagraph,
    selectSpanishParagraph,
    expectMenuItemIsVisible,
    expectedTargetLanguage,
    openAtFirstParagraph,
    openAtSpanishParagraph,
    openAtEnglishHyperlink,
    openAtSpanishHyperlink,
  },
  message
) {
  logAction();

  if (message) {
    info(message);
  }

  await closeTranslationsPanelIfOpen();
  await closeContextMenuIfOpen();

  await openContextMenu(runInPage, {
    selectFirstParagraph,
    selectSpanishParagraph,
    openAtFirstParagraph,
    openAtSpanishParagraph,
    openAtEnglishHyperlink,
    openAtSpanishHyperlink,
  });

  const menuItem = maybeGetById(
    "context-translate-selection",
    /* ensureIsVisible */ false
  );

  if (expectMenuItemIsVisible !== undefined) {
    assertIsVisible(expectMenuItemIsVisible, { element: menuItem });
  }

  if (expectMenuItemIsVisible === true) {
    if (expectedTargetLanguage) {
      // Target language expected, check for the data-l10n-id with a `{$language}` argument.
      const expectedL10nId =
        selectFirstParagraph === true || selectSpanishParagraph === true
          ? "main-context-menu-translate-selection-to-language"
          : "main-context-menu-translate-link-text-to-language";
      await waitForCondition(
        () => menuItem.getAttribute("data-l10n-id") === expectedL10nId,
        `Waiting for translate-selection context menu item to localize with target language ${expectedTargetLanguage}`
      );

      is(
        menuItem.getAttribute("data-l10n-id"),
        expectedL10nId,
        "Expected the translate-selection context menu item to be localized with a target language."
      );

      const l10nArgs = JSON.parse(menuItem.getAttribute("data-l10n-args"));
      is(
        l10nArgs.language,
        getIntlDisplayName(expectedTargetLanguage),
        `Expected the translate-selection context menu item to have the target language '${expectedTargetLanguage}'.`
      );
    } else {
      // No target language expected, check for the data-l10n-id that has no `{$language}` argument.
      const expectedL10nId =
        selectFirstParagraph === true || selectSpanishParagraph === true
          ? "main-context-menu-translate-selection"
          : "main-context-menu-translate-link-text";
      await waitForCondition(
        () => menuItem.getAttribute("data-l10n-id") === expectedL10nId,
        "Waiting for translate-selection context menu item to localize without target language."
      );

      is(
        menuItem.getAttribute("data-l10n-id"),
        expectedL10nId,
        "Expected the translate-selection context menu item to be localized without a target language."
      );
    }
  }

  await closeContextMenuIfOpen();
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
 * Retrieves an element by its Id.
 *
 * @param {string} id
 * @param {Document} [doc]
 * @returns {Element}
 * @throws Throws if the element is not visible in the DOM.
 */
function getById(id, doc = document) {
  const element = maybeGetById(id, /* ensureIsVisible */ true, doc);
  if (!element) {
    throw new Error("The element is not visible in the DOM: #" + id);
  }
  return element;
}

/**
 * Attempts to retrieve an element by its Id.
 *
 * @param {string} id - The Id of the element to retrieve.
 * @param {boolean} [ensureIsVisible=true] - If set to true, the function will return null when the element is not visible.
 * @param {Document} [doc=document] - The document from which to retrieve the element.
 * @returns {Element | null} - The retrieved element.
 * @throws Throws if no element was found by the given Id.
 */
function maybeGetById(id, ensureIsVisible = true, doc = document) {
  const element = doc.getElementById(id);
  if (!element) {
    throw new Error("Could not find the element by id: #" + id);
  }

  if (!ensureIsVisible) {
    return element;
  }

  if (isVisible(element)) {
    return element;
  }

  return null;
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

/**
 * Opens the Translation Settings page by clicking the settings button sent in the argument.
 *
 * @param  {HTMLElement} settingsButton
 * @returns {Element}
 */
async function openAboutPreferencesTranslationsSettingsPane(settingsButton) {
  const document = gBrowser.selectedBrowser.contentDocument;

  const promise = BrowserTestUtils.waitForEvent(
    document,
    "paneshown",
    false,
    event => event.detail.category === "paneTranslations"
  );

  click(settingsButton, "Click settings button");
  await promise;

  const elements = {
    backButton: document.getElementById("translations-settings-back-button"),
    header: document.getElementById("translations-settings-header"),
  };

  return elements;
}
