/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/translations/tests/browser/shared-head.js",
  this
);

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
    true // Wait for load
  );
  return {
    tab,
    removeTab() {
      BrowserTestUtils.removeTab(tab);
    },
  };
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
 * Get all elements that match the l10n id.
 *
 * @param {string} l10nId
 * @param {Document} doc
 * @returns {Element}
 */
function getAllByL10nId(l10nId, doc = document) {
  const elements = doc.querySelectorAll(`[data-l10n-id="${l10nId}"]`);
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
    if (BrowserTestUtils.isVisible(element)) {
      return element;
    }
  }
  throw new Error("The element is not visible in the DOM: " + l10nId);
}

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

  if (BrowserTestUtils.isVisible(element)) {
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
    if (BrowserTestUtils.isVisible(element)) {
      return element;
    }
  }
  return null;
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
  await closeAllOpenPanelsAndMenus();

  info(message);

  // Load a blank page first to ensure that tests don't hang.
  // I don't know why this is needed, but it appears to be necessary.
  BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, BLANK_PAGE);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  const loadTargetPage = async () => {
    BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, url);
    await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

    if (downloadHandler) {
      await FullPageTranslationsTestUtils.assertTranslationsButton(
        { button: true, circleArrows: true, locale: false, icon: true },
        "The icon presents the loading indicator."
      );
      await downloadHandler(pivotTranslation ? 2 : 1);
    }
  };

  info(`Loading url: "${url}"`);
  if (onOpenPanel) {
    await FullPageTranslationsTestUtils.waitForPanelPopupEvent(
      "popupshown",
      loadTargetPage,
      onOpenPanel
    );
  } else {
    await loadTargetPage();
  }
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
 * A collection of shared functionality utilized by
 * FullPageTranslationsTestUtils and SelectTranslationsTestUtils.
 *
 * Using functions from the aforementioned classes is preferred over
 * using functions from this class directly.
 */
class SharedTranslationsTestUtils {
  /**
   * Asserts that the specified element currently has focus.
   *
   * @param {Element} element - The element to check for focus.
   */
  static _assertHasFocus(element) {
    is(
      document.activeElement,
      element,
      `The element '${element.id}' should have focus.`
    );
  }

  /**
   * Asserts that the mainViewId of the panel matches the given string.
   *
   * @param {FullPageTranslationsPanel | SelectTranslationsPanel} panel
   * @param {string} expectedId - The expected id that mainViewId is set to.
   */
  static _assertPanelMainViewId(panel, expectedId) {
    const mainViewId = panel.elements.multiview.getAttribute("mainViewId");
    is(
      mainViewId,
      expectedId,
      "The mainViewId should match its expected value"
    );
  }

  /**
   * Asserts that the selected language in the menu matches the langTag or l10nId.
   *
   * @param {Element} menuList - The menu list element to check.
   * @param {object} options - Options containing 'langTag' and 'l10nId' to assert against.
   * @param {string} [options.langTag] - The BCP-47 language tag to match.
   * @param {string} [options.l10nId] - The localization Id to match.
   */
  static _assertSelectedLanguage(menuList, { langTag, l10nId }) {
    ok(
      menuList.label,
      `The label for the menulist ${menuList.id} should not be empty.`
    );
    if (langTag) {
      is(
        menuList.value,
        langTag,
        `Expected ${menuList.id} selection to match '${langTag}'`
      );
    }
    if (l10nId) {
      is(
        menuList.getAttribute("data-l10n-id"),
        l10nId,
        `Expected ${menuList.id} l10nId to match '${l10nId}'`
      );
    }
  }

  /**
   * Asserts the visibility of the given elements based on the given expectations.
   *
   * @param {object} elements - An object containing the elements to be checked for visibility.
   * @param {object} expectations - An object where each property corresponds to a property in elements,
   *                                and its value is a boolean indicating whether the element should
   *                                be visible (true) or hidden (false).
   * @throws Throws if elements does not contain a property for each property in expectations.
   */
  static _assertPanelElementVisibility(elements, expectations) {
    const hidden = {};
    const visible = {};

    for (const propertyName in expectations) {
      ok(
        elements.hasOwnProperty(propertyName),
        `Expected panel elements to have property ${propertyName}`
      );
      if (expectations[propertyName]) {
        visible[propertyName] = elements[propertyName];
      } else {
        hidden[propertyName] = elements[propertyName];
      }
    }

    assertVisibility({ hidden, visible });
  }

  /**
   * Executes the provided callback before waiting for the event and then waits for the given event
   * to be fired for the element corresponding to the provided elementId.
   *
   * Optionally executes a postEventAssertion function once the event occurs.
   *
   * @param {string} elementId - The Id of the element to wait for the event on.
   * @param {string} eventName - The name of the event to wait for.
   * @param {Function} callback - A callback function to execute immediately before waiting for the event.
   *                              This is often used to trigger the event on the expected element.
   * @param {Function|null} [postEventAssertion=null] - An optional callback function to execute after
   *                                                    the event has occurred.
   * @throws Throws if the element with the specified `elementId` does not exist.
   * @returns {Promise<void>}
   */
  static async _waitForPopupEvent(
    elementId,
    eventName,
    callback,
    postEventAssertion = null
  ) {
    const element = document.getElementById(elementId);
    if (!element) {
      throw new Error(
        `Unable to find the ${elementId} element in the document.`
      );
    }
    const promise = BrowserTestUtils.waitForEvent(element, eventName);
    await callback();
    info(`Waiting for the ${elementId} ${eventName} event`);
    await promise;
    if (postEventAssertion) {
      await postEventAssertion();
    }
    // Wait a single tick on the event loop.
    await new Promise(resolve => setTimeout(resolve, 0));
  }
}

/**
 * A class containing test utility functions specific to testing full-page translations.
 */
class FullPageTranslationsTestUtils {
  /**
   * A collection of element visibility expectations for the default panel view.
   */
  static #defaultViewVisibilityExpectations = {
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
   * Asserts that the state of a checkbox with a given dataL10nId is
   * checked or not, based on the value of expected being true or false.
   *
   * @param {string} dataL10nId - The data-l10n-id of the checkbox.
   * @param {object} expectations
   * @param {string} expectations.langTag - A BCP-47 language tag.
   * @param {boolean} expectations.checked - Whether the checkbox is expected to be checked.
   * @param {boolean} expectations.disabled - Whether the menuitem is expected to be disabled.
   */
  static async #assertCheckboxState(
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
   * Asserts that the always-offer-translations checkbox matches the expected checked state.
   *
   * @param {boolean} checked
   */
  static async assertIsAlwaysOfferTranslationsEnabled(checked) {
    info(
      `Checking that always-offer-translations is ${
        checked ? "enabled" : "disabled"
      }`
    );
    await FullPageTranslationsTestUtils.#assertCheckboxState(
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
  static async assertIsAlwaysTranslateLanguage(
    langTag,
    { checked = true, disabled = false }
  ) {
    info(
      `Checking that always-translate is ${
        checked ? "enabled" : "disabled"
      } for "${langTag}"`
    );
    await FullPageTranslationsTestUtils.#assertCheckboxState(
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
  static async assertIsNeverTranslateLanguage(
    langTag,
    { checked = true, disabled = false }
  ) {
    info(
      `Checking that never-translate is ${
        checked ? "enabled" : "disabled"
      } for "${langTag}"`
    );
    await FullPageTranslationsTestUtils.#assertCheckboxState(
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
  static async assertIsNeverTranslateSite(
    url,
    { checked = true, disabled = false }
  ) {
    info(
      `Checking that never-translate is ${
        checked ? "enabled" : "disabled"
      } for "${url}"`
    );
    await FullPageTranslationsTestUtils.#assertCheckboxState(
      "translations-panel-settings-never-translate-site",
      { checked, disabled }
    );
  }

  /**
   * Asserts that the proper language tags are shown on the translations button.
   *
   * @param {string} fromLanguage - The BCP-47 language tag being translated from.
   * @param {string} toLanguage - The BCP-47 language tag being translated into.
   */
  static async #assertLangTagIsShownOnTranslationsButton(
    fromLanguage,
    toLanguage
  ) {
    info(
      `Ensuring that the translations button displays the language tag "${toLanguage}"`
    );
    const { button, locale } =
      await FullPageTranslationsTestUtils.assertTranslationsButton(
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
   * Asserts that the Spanish test page has been translated by checking
   * that the H1 element has been modified from its original form.
   *
   * @param {string} fromLanguage - The BCP-47 language tag being translated from.
   * @param {string} toLanguage - The BCP-47 language tag being translated into.
   * @param {Function} runInPage - Allows running a closure in the content page.
   * @param {string} message - An optional message to log to info.
   */
  static async assertPageIsTranslated(
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
    await FullPageTranslationsTestUtils.#assertLangTagIsShownOnTranslationsButton(
      fromLanguage,
      toLanguage
    );
  }

  /**
   * Asserts that the Spanish test page is untranslated by checking
   * that the H1 element is still in its original Spanish form.
   *
   * @param {Function} runInPage - Allows running a closure in the content page.
   * @param {string} message - An optional message to log to info.
   */
  static async assertPageIsUntranslated(runInPage, message = null) {
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
   * Asserts that for each provided expectation, the visible state of the corresponding
   * element in FullPageTranslationsPanel.elements both exists and matches the visibility expectation.
   *
   * @param {object} expectations
   *   A list of expectations for the visibility of any subset of FullPageTranslationsPanel.elements
   */
  static #assertPanelElementVisibility(expectations = {}) {
    SharedTranslationsTestUtils._assertPanelElementVisibility(
      FullPageTranslationsPanel.elements,
      {
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
        // Overwrite any of the above defaults with the passed in expectations.
        ...expectations,
      }
    );
  }

  /**
   * Asserts that the FullPageTranslationsPanel header has the expected l10nId.
   *
   * @param {string} l10nId - The expected data-l10n-id of the header.
   */
  static #assertPanelHeaderL10nId(l10nId) {
    const { header } = FullPageTranslationsPanel.elements;
    is(
      header.getAttribute("data-l10n-id"),
      l10nId,
      "The translations panel header should match the expected data-l10n-id"
    );
  }

  /**
   * Asserts that the FullPageTranslationsPanel error has the expected l10nId.
   *
   * @param {string} l10nId - The expected data-l10n-id of the error.
   */
  static #assertPanelErrorL10nId(l10nId) {
    const { errorMessage } = FullPageTranslationsPanel.elements;
    is(
      errorMessage.getAttribute("data-l10n-id"),
      l10nId,
      "The translations panel error message should match the expected data-l10n-id"
    );
  }

  /**
   * Asserts that the mainViewId of the panel matches the given string.
   *
   * @param {string} expectedId
   */
  static #assertPanelMainViewId(expectedId) {
    SharedTranslationsTestUtils._assertPanelMainViewId(
      FullPageTranslationsPanel,
      expectedId
    );
  }

  /**
   * Asserts that panel element visibility matches the default panel view.
   */
  static assertPanelViewDefault() {
    info("Checking that the panel shows the default view");
    FullPageTranslationsTestUtils.#assertPanelMainViewId(
      "full-page-translations-panel-view-default"
    );
    FullPageTranslationsTestUtils.#assertPanelElementVisibility({
      ...FullPageTranslationsTestUtils.#defaultViewVisibilityExpectations,
    });
    FullPageTranslationsTestUtils.#assertPanelHeaderL10nId(
      "translations-panel-header"
    );
  }

  /**
   * Asserts that panel element visibility matches the panel error view.
   */
  static assertPanelViewError() {
    info("Checking that the panel shows the error view");
    FullPageTranslationsTestUtils.#assertPanelMainViewId(
      "full-page-translations-panel-view-default"
    );
    FullPageTranslationsTestUtils.#assertPanelElementVisibility({
      error: true,
      ...FullPageTranslationsTestUtils.#defaultViewVisibilityExpectations,
    });
    FullPageTranslationsTestUtils.#assertPanelHeaderL10nId(
      "translations-panel-header"
    );
    FullPageTranslationsTestUtils.#assertPanelErrorL10nId(
      "translations-panel-error-translating"
    );
  }

  /**
   * Asserts that the panel element visibility matches the panel loading view.
   */
  static assertPanelViewLoading() {
    info("Checking that the panel shows the loading view");
    FullPageTranslationsTestUtils.assertPanelViewDefault();
    const loadingButton = getByL10nId(
      "translations-panel-translate-button-loading"
    );
    ok(loadingButton, "The loading button is present");
    ok(loadingButton.disabled, "The loading button is disabled");
  }

  /**
   * Asserts that panel element visibility matches the panel first-show view.
   */
  static assertPanelViewFirstShow() {
    info("Checking that the panel shows the first-show view");
    FullPageTranslationsTestUtils.#assertPanelMainViewId(
      "full-page-translations-panel-view-default"
    );
    FullPageTranslationsTestUtils.#assertPanelElementVisibility({
      intro: true,
      introLearnMoreLink: true,
      ...FullPageTranslationsTestUtils.#defaultViewVisibilityExpectations,
    });
    FullPageTranslationsTestUtils.#assertPanelHeaderL10nId(
      "translations-panel-intro-header"
    );
  }

  /**
   * Asserts that panel element visibility matches the panel first-show error view.
   */
  static assertPanelViewFirstShowError() {
    info("Checking that the panel shows the first-show error view");
    FullPageTranslationsTestUtils.#assertPanelMainViewId(
      "full-page-translations-panel-view-default"
    );
    FullPageTranslationsTestUtils.#assertPanelElementVisibility({
      error: true,
      intro: true,
      introLearnMoreLink: true,
      ...FullPageTranslationsTestUtils.#defaultViewVisibilityExpectations,
    });
    FullPageTranslationsTestUtils.#assertPanelHeaderL10nId(
      "translations-panel-intro-header"
    );
  }

  /**
   * Asserts that panel element visibility matches the panel revisit view.
   */
  static assertPanelViewRevisit() {
    info("Checking that the panel shows the revisit view");
    FullPageTranslationsTestUtils.#assertPanelMainViewId(
      "full-page-translations-panel-view-default"
    );
    FullPageTranslationsTestUtils.#assertPanelElementVisibility({
      header: true,
      langSelection: true,
      restoreButton: true,
      toLabel: true,
      toMenuList: true,
      translateButton: true,
    });
    FullPageTranslationsTestUtils.#assertPanelHeaderL10nId(
      "translations-panel-revisit-header"
    );
  }

  /**
   * Asserts that panel element visibility matches the panel unsupported language view.
   */
  static assertPanelViewUnsupportedLanguage() {
    info("Checking that the panel shows the unsupported-language view");
    FullPageTranslationsTestUtils.#assertPanelMainViewId(
      "full-page-translations-panel-view-unsupported-language"
    );
    FullPageTranslationsTestUtils.#assertPanelElementVisibility({
      changeSourceLanguageButton: true,
      dismissErrorButton: true,
      unsupportedHeader: true,
      unsupportedHint: true,
      unsupportedLearnMoreLink: true,
    });
  }

  /**
   * Asserts that the selected from-language matches the provided language tag.
   *
   * @param {object} options - Options containing 'langTag' and 'l10nId' to assert against.
   * @param {string} [options.langTag] - The BCP-47 language tag to match.
   * @param {string} [options.l10nId] - The localization Id to match.
   */
  static assertSelectedFromLanguage({ langTag, l10nId }) {
    const { fromMenuList } = FullPageTranslationsPanel.elements;
    SharedTranslationsTestUtils._assertSelectedLanguage(fromMenuList, {
      langTag,
      l10nId,
    });
  }

  /**
   * Asserts that the selected to-language matches the provided language tag.
   *
   * @param {object} options - Options containing 'langTag' and 'l10nId' to assert against.
   * @param {string} [options.langTag] - The BCP-47 language tag to match.
   * @param {string} [options.l10nId] - The localization Id to match.
   */
  static assertSelectedToLanguage({ langTag, l10nId }) {
    const { toMenuList } = FullPageTranslationsPanel.elements;
    SharedTranslationsTestUtils._assertSelectedLanguage(toMenuList, {
      langTag,
      l10nId,
    });
  }

  /**
   * Assert some property about the translations button.
   *
   * @param {Record<string, boolean>} visibleAssertions
   * @param {string} message The message for the assertion.
   * @returns {HTMLElement}
   */
  static async assertTranslationsButton(visibleAssertions, message) {
    const elements = {
      button: document.getElementById("translations-button"),
      icon: document.getElementById("translations-button-icon"),
      circleArrows: document.getElementById(
        "translations-button-circle-arrows"
      ),
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
   * Simulates the effect of clicking the always-offer-translations menuitem.
   * Requires that the settings menu of the translations panel is open,
   * otherwise the test will fail.
   */
  static async clickAlwaysOfferTranslations() {
    logAction();
    await FullPageTranslationsTestUtils.#clickSettingsMenuItemByL10nId(
      "translations-panel-settings-always-offer-translation"
    );
  }

  /**
   * Simulates the effect of clicking the always-translate-language menuitem.
   * Requires that the settings menu of the translations panel is open,
   * otherwise the test will fail.
   */
  static async clickAlwaysTranslateLanguage({
    downloadHandler = null,
    pivotTranslation = false,
  } = {}) {
    logAction();
    await FullPageTranslationsTestUtils.#clickSettingsMenuItemByL10nId(
      "translations-panel-settings-always-translate-language"
    );
    if (downloadHandler) {
      await FullPageTranslationsTestUtils.assertTranslationsButton(
        { button: true, circleArrows: true, locale: false, icon: true },
        "The icon presents the loading indicator."
      );
      await downloadHandler(pivotTranslation ? 2 : 1);
    }
  }

  /**
   * Simulates clicking the cancel button.
   */
  static async clickCancelButton() {
    logAction();
    const { cancelButton } = FullPageTranslationsPanel.elements;
    assertVisibility({ visible: { cancelButton } });
    await FullPageTranslationsTestUtils.waitForPanelPopupEvent(
      "popuphidden",
      () => {
        click(cancelButton, "Clicking the cancel button");
      }
    );
  }

  /**
   * Simulates clicking the change-source-language button.
   *
   * @param {object} config
   * @param {boolean} config.firstShow
   *  - True if the first-show view should be expected
   *    False if the default view should be expected
   */
  static async clickChangeSourceLanguageButton({ firstShow = false } = {}) {
    logAction();
    const { changeSourceLanguageButton } = FullPageTranslationsPanel.elements;
    assertVisibility({ visible: { changeSourceLanguageButton } });
    await FullPageTranslationsTestUtils.waitForPanelPopupEvent(
      "popupshown",
      () => {
        click(
          changeSourceLanguageButton,
          "Click the change-source-language button"
        );
      },
      firstShow
        ? FullPageTranslationsTestUtils.assertPanelViewFirstShow
        : FullPageTranslationsTestUtils.assertPanelViewDefault
    );
  }

  /**
   * Simulates clicking the dismiss-error button.
   */
  static async clickDismissErrorButton() {
    logAction();
    const { dismissErrorButton } = FullPageTranslationsPanel.elements;
    assertVisibility({ visible: { dismissErrorButton } });
    await FullPageTranslationsTestUtils.waitForPanelPopupEvent(
      "popuphidden",
      () => {
        click(dismissErrorButton, "Click the dismiss-error button");
      }
    );
  }

  /**
   * Simulates the effect of clicking the manage-languages menuitem.
   * Requires that the settings menu of the translations panel is open,
   * otherwise the test will fail.
   */
  static async clickManageLanguages() {
    logAction();
    await FullPageTranslationsTestUtils.#clickSettingsMenuItemByL10nId(
      "translations-panel-settings-manage-languages"
    );
  }

  /**
   * Simulates the effect of clicking the never-translate-language menuitem.
   * Requires that the settings menu of the translations panel is open,
   * otherwise the test will fail.
   */
  static async clickNeverTranslateLanguage() {
    logAction();
    await FullPageTranslationsTestUtils.#clickSettingsMenuItemByL10nId(
      "translations-panel-settings-never-translate-language"
    );
  }

  /**
   * Simulates the effect of clicking the never-translate-site menuitem.
   * Requires that the settings menu of the translations panel is open,
   * otherwise the test will fail.
   */
  static async clickNeverTranslateSite() {
    logAction();
    await FullPageTranslationsTestUtils.#clickSettingsMenuItemByL10nId(
      "translations-panel-settings-never-translate-site"
    );
  }

  /**
   * Simulates clicking the restore-page button.
   */
  static async clickRestoreButton() {
    logAction();
    const { restoreButton } = FullPageTranslationsPanel.elements;
    assertVisibility({ visible: { restoreButton } });
    await FullPageTranslationsTestUtils.waitForPanelPopupEvent(
      "popuphidden",
      () => {
        click(restoreButton, "Click the restore-page button");
      }
    );
  }

  /*
   * Simulates the effect of toggling a menu item in the translations panel
   * settings menu. Requires that the settings menu is currently open,
   * otherwise the test will fail.
   */
  static async #clickSettingsMenuItemByL10nId(l10nId) {
    info(`Toggling the "${l10nId}" settings menu item.`);
    click(getByL10nId(l10nId), `Clicking the "${l10nId}" settings menu item.`);
    await closeSettingsMenuIfOpen();
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
  static async clickTranslateButton({
    downloadHandler = null,
    pivotTranslation = false,
  } = {}) {
    logAction();
    const { translateButton } = FullPageTranslationsPanel.elements;
    assertVisibility({ visible: { translateButton } });
    await FullPageTranslationsTestUtils.waitForPanelPopupEvent(
      "popuphidden",
      () => {
        click(translateButton);
      }
    );

    if (downloadHandler) {
      await FullPageTranslationsTestUtils.assertTranslationsButton(
        { button: true, circleArrows: true, locale: false, icon: true },
        "The icon presents the loading indicator."
      );
      await downloadHandler(pivotTranslation ? 2 : 1);
    }
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
  static async openPanel({
    onOpenPanel = null,
    openFromAppMenu = false,
    openWithKeyboard = false,
  }) {
    logAction();
    await closeAllOpenPanelsAndMenus();
    if (openFromAppMenu) {
      await FullPageTranslationsTestUtils.#openPanelViaAppMenu({
        onOpenPanel,
        openWithKeyboard,
      });
    } else {
      await FullPageTranslationsTestUtils.#openPanelViaTranslationsButton({
        onOpenPanel,
        openWithKeyboard,
      });
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
  static async #openPanelViaAppMenu({
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

    await FullPageTranslationsTestUtils.waitForPanelPopupEvent(
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
   * Opens the translations panel via the translations button.
   *
   * @param {object} config
   * @param {Function} config.onOpenPanel
   *  - A function to run as soon as the panel opens.
   * @param {boolean} config.openWithKeyboard
   *  - Open the panel by synthesizing the keyboard. If false, synthesizes the mouse.
   */
  static async #openPanelViaTranslationsButton({
    onOpenPanel = null,
    openWithKeyboard = false,
  }) {
    logAction();
    const { button } =
      await FullPageTranslationsTestUtils.assertTranslationsButton(
        { button: true },
        "The translations button is visible."
      );
    await FullPageTranslationsTestUtils.waitForPanelPopupEvent(
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
   * Opens the translations panel settings menu.
   * Requires that the translations panel is already open.
   */
  static async openTranslationsSettingsMenu() {
    logAction();
    const gearIcons = getAllByL10nId("translations-panel-settings-button");
    for (const gearIcon of gearIcons) {
      if (BrowserTestUtils.isHidden(gearIcon)) {
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
   * Switches the selected from-language to the provided language tag.
   *
   * @param {string} langTag - A BCP-47 language tag.
   */
  static changeSelectedFromLanguage(langTag) {
    logAction(langTag);
    const { fromMenuList } = FullPageTranslationsPanel.elements;
    fromMenuList.value = langTag;
    fromMenuList.dispatchEvent(new Event("command"));
  }

  /**
   * Switches the selected to-language to the provided language tag.
   *
   * @param {string} langTag - A BCP-47 language tag.
   */
  static changeSelectedToLanguage(langTag) {
    logAction(langTag);
    const { toMenuList } = FullPageTranslationsPanel.elements;
    toMenuList.value = langTag;
    toMenuList.dispatchEvent(new Event("command"));
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
  static async waitForPanelPopupEvent(
    eventName,
    callback,
    postEventAssertion = null
  ) {
    // De-lazify the panel elements.
    FullPageTranslationsPanel.elements;
    await SharedTranslationsTestUtils._waitForPopupEvent(
      "full-page-translations-panel",
      eventName,
      callback,
      postEventAssertion
    );
  }
}

/**
 * A class containing test utility functions specific to testing select translations.
 */
class SelectTranslationsTestUtils {
  /**
   * Opens the context menu then asserts properties of the translate-selection item in the context menu.
   *
   * @param {Function} runInPage - A content-exposed function to run within the context of the page.
   * @param {object} options - Options for how to open the context menu and what properties to assert about the translate-selection item.
   *
   * The following options will only work when testing SELECT_TEST_PAGE_URL.
   *
   * @param {boolean} options.expectMenuItemVisible - Whether the select-translations menu item should be present in the context menu.
   * @param {boolean} options.expectedTargetLanguage - The expected target language to be shown in the context menu.
   * @param {boolean} options.selectFrenchSection - Selects the section of French text.
   * @param {boolean} options.selectEnglishSection - Selects the section of English text.
   * @param {boolean} options.selectSpanishSection - Selects the section of Spanish text.
   * @param {boolean} options.selectFrenchSentence - Selects a French sentence.
   * @param {boolean} options.selectEnglishSentence - Selects an English sentence.
   * @param {boolean} options.selectSpanishSentence - Selects a Spanish sentence.
   * @param {boolean} options.openAtFrenchSection - Opens the context menu at the section of French text.
   * @param {boolean} options.openAtEnglishSection - Opens the context menu at the section of English text.
   * @param {boolean} options.openAtSpanishSection - Opens the context menu at the section of Spanish text.
   * @param {boolean} options.openAtFrenchSentence - Opens the context menu at a French sentence.
   * @param {boolean} options.openAtEnglishSentence - Opens the context menu at an English sentence.
   * @param {boolean} options.openAtSpanishSentence - Opens the context menu at a Spanish sentence.
   * @param {boolean} options.openAtFrenchHyperlink - Opens the context menu at a hyperlinked French text.
   * @param {boolean} options.openAtEnglishHyperlink - Opens the context menu at an hyperlinked English text.
   * @param {boolean} options.openAtSpanishHyperlink - Opens the context menu at a hyperlinked Spanish text.
   * @param {string} [message] - A message to log to info.
   * @throws Throws an error if the properties of the translate-selection item do not match the expected options.
   */
  static async assertContextMenuTranslateSelectionItem(
    runInPage,
    {
      expectMenuItemVisible,
      expectedTargetLanguage,
      selectFrenchSection,
      selectEnglishSection,
      selectSpanishSection,
      selectFrenchSentence,
      selectEnglishSentence,
      selectSpanishSentence,
      openAtFrenchSection,
      openAtEnglishSection,
      openAtSpanishSection,
      openAtFrenchSentence,
      openAtEnglishSentence,
      openAtSpanishSentence,
      openAtFrenchHyperlink,
      openAtEnglishHyperlink,
      openAtSpanishHyperlink,
    },
    message
  ) {
    logAction();

    if (message) {
      info(message);
    }

    await closeAllOpenPanelsAndMenus();

    await SelectTranslationsTestUtils.openContextMenu(runInPage, {
      expectMenuItemVisible,
      expectedTargetLanguage,
      selectFrenchSection,
      selectEnglishSection,
      selectSpanishSection,
      selectFrenchSentence,
      selectEnglishSentence,
      selectSpanishSentence,
      openAtFrenchSection,
      openAtEnglishSection,
      openAtSpanishSection,
      openAtFrenchSentence,
      openAtEnglishSentence,
      openAtSpanishSentence,
      openAtFrenchHyperlink,
      openAtEnglishHyperlink,
      openAtSpanishHyperlink,
    });

    const menuItem = maybeGetById(
      "context-translate-selection",
      /* ensureIsVisible */ false
    );

    if (expectMenuItemVisible !== undefined) {
      const visibility = expectMenuItemVisible ? "visible" : "hidden";
      assertVisibility({ [visibility]: { menuItem } });
    }

    if (expectMenuItemVisible === true) {
      if (expectedTargetLanguage) {
        // Target language expected, check for the data-l10n-id with a `{$language}` argument.
        const expectedL10nId =
          selectFrenchSection ||
          selectEnglishSection ||
          selectSpanishSection ||
          selectFrenchSentence ||
          selectEnglishSentence ||
          selectSpanishSentence
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
          selectFrenchSection ||
          selectEnglishSection ||
          selectSpanishSection ||
          selectFrenchSentence ||
          selectEnglishSentence ||
          selectSpanishSentence
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
  }

  /**
   * Elements that should always be visible in the SelectTranslationsPanel.
   */
  static #alwaysPresentElements = {
    betaIcon: true,
    copyButton: true,
    doneButton: true,
    fromLabel: true,
    fromMenuList: true,
    header: true,
    toLabel: true,
    toMenuList: true,
    textArea: true,
  };

  /**
   * Asserts that for each provided expectation, the visible state of the corresponding
   * element in FullPageTranslationsPanel.elements both exists and matches the visibility expectation.
   *
   * @param {object} expectations
   *   A list of expectations for the visibility of any subset of SelectTranslationsPanel.elements
   */
  static #assertPanelElementVisibility(expectations = {}) {
    SharedTranslationsTestUtils._assertPanelElementVisibility(
      SelectTranslationsPanel.elements,
      {
        ...SelectTranslationsTestUtils.#alwaysPresentElements,
        // Overwrite any of the above defaults with the passed in expectations.
        ...expectations,
      }
    );
  }

  /**
   * Waits for the panel's translation state to reach the given phase,
   * if it is not currently in that phase already.
   *
   * @param {string} phase - The phase of the panel's translation state to wait for.
   */
  static async waitForPanelState(phase) {
    const currentPhase = SelectTranslationsPanel.phase();
    if (currentPhase !== phase) {
      info(
        `Waiting for SelectTranslationsPanel to change state from "${currentPhase}" to "${phase}"`
      );
      await BrowserTestUtils.waitForEvent(
        document,
        "SelectTranslationsPanelStateChanged",
        event => event.detail.phase === phase
      );
    }
  }

  /**
   * Asserts that the SelectTranslationsPanel UI matches the expected
   * state when the panel has completed its translation.
   */
  static async assertPanelViewTranslated() {
    const { textArea } = SelectTranslationsPanel.elements;
    await SelectTranslationsTestUtils.waitForPanelState("translated");
    ok(
      !textArea.classList.contains("translating"),
      "The textarea should not have the translating class."
    );
    SelectTranslationsTestUtils.#assertPanelElementVisibility({
      ...SelectTranslationsTestUtils.#alwaysPresentElements,
    });
    SelectTranslationsTestUtils.#assertConditionalUIEnabled({
      textArea: true,
      copyButton: true,
      translateFullPageButton: true,
    });
    SelectTranslationsTestUtils.#assertPanelHasTranslatedText();
    SelectTranslationsTestUtils.#assertPanelTextAreaHeight();
    await SelectTranslationsTestUtils.#assertPanelTextAreaOverflow();
  }

  static #assertPanelTextAreaDirection(langTag = null) {
    const expectedTextDirection = langTag
      ? Services.intl.getScriptDirection(langTag)
      : null;
    const { textArea } = SelectTranslationsPanel.elements;
    const actualTextDirection = textArea.getAttribute("dir");

    is(
      actualTextDirection,
      expectedTextDirection,
      `The text direction should be ${expectedTextDirection}`
    );
  }

  /**
   * Asserts that the SelectTranslationsPanel translated text area is
   * both scrollable and scrolled to the top.
   */
  static async #assertPanelTextAreaOverflow() {
    const { textArea } = SelectTranslationsPanel.elements;
    if (textArea.style.overflow !== "auto") {
      await BrowserTestUtils.waitForMutationCondition(
        textArea,
        { attributes: true, attributeFilter: ["style"] },
        () => textArea.style.overflow === "auto"
      );
    }
    if (textArea.scrollHeight > textArea.clientHeight) {
      info("Ensuring that the textarea is scrolled to the top.");
      await waitForCondition(() => textArea.scrollTop === 0);
    }
  }

  /**
   * Asserts that the SelectTranslationsPanel translated text area is
   * the correct height for the length of the translated text.
   */
  static #assertPanelTextAreaHeight() {
    const { textArea } = SelectTranslationsPanel.elements;

    if (
      SelectTranslationsPanel.getSourceText().length <
      SelectTranslationsPanel.textLengthThreshold
    ) {
      is(
        textArea.style.height,
        SelectTranslationsPanel.shortTextHeight,
        "The panel text area should have the short-text height"
      );
    } else {
      is(
        textArea.style.height,
        SelectTranslationsPanel.longTextHeight,
        "The panel text area should have the long-text height"
      );
    }
  }

  /**
   * Asserts that the SelectTranslationsPanel UI matches the expected
   * state when the panel is actively translating text.
   */
  static async assertPanelViewActivelyTranslating() {
    const { textArea } = SelectTranslationsPanel.elements;
    await SelectTranslationsTestUtils.waitForPanelState("translating");
    ok(
      textArea.classList.contains("translating"),
      "The textarea should have the translating class."
    );
    SelectTranslationsTestUtils.#assertPanelElementVisibility({
      ...SelectTranslationsTestUtils.#alwaysPresentElements,
    });
    SelectTranslationsTestUtils.#assertPanelHasTranslatingPlaceholder();
  }

  /**
   * Asserts that the SelectTranslationsPanel UI matches the expected
   * state when no from-language is selected in the panel.
   */
  static async assertPanelViewNoFromLangSelected() {
    const { textArea } = SelectTranslationsPanel.elements;
    await SelectTranslationsTestUtils.waitForPanelState("idle");
    ok(
      !textArea.classList.contains("translating"),
      "The textarea should not have the translating class."
    );
    SelectTranslationsTestUtils.#assertPanelElementVisibility({
      ...SelectTranslationsTestUtils.#alwaysPresentElements,
    });
    await SelectTranslationsTestUtils.#assertPanelHasIdlePlaceholder();
    SelectTranslationsTestUtils.#assertConditionalUIEnabled({
      textArea: false,
      copyButton: false,
      translateFullPageButton: false,
    });
    SelectTranslationsTestUtils.assertSelectedFromLanguage(null);
  }

  /**
   * Asserts that the SelectTranslationsPanel UI contains the
   * idle placeholder text.
   */
  static async #assertPanelHasIdlePlaceholder() {
    const { textArea } = SelectTranslationsPanel.elements;
    const expected = await document.l10n.formatValue(
      "select-translations-panel-idle-placeholder-text"
    );
    is(
      textArea.value,
      expected,
      "Translated text area should be the idle placeholder."
    );
    SelectTranslationsTestUtils.#assertPanelTextAreaDirection();
  }

  /**
   * Asserts that the SelectTranslationsPanel UI contains the
   * translating placeholder text.
   */
  static async #assertPanelHasTranslatingPlaceholder() {
    const { textArea } = SelectTranslationsPanel.elements;
    const expected = await document.l10n.formatValue(
      "select-translations-panel-translating-placeholder-text"
    );
    is(
      textArea.value,
      expected,
      "Active translation text area should have the translating placeholder."
    );
    SelectTranslationsTestUtils.#assertPanelTextAreaDirection();
    SelectTranslationsTestUtils.#assertConditionalUIEnabled({
      textArea: true,
      copyButton: false,
      translateFullPageButton: true,
    });
  }

  /**
   * Asserts that the SelectTranslationsPanel UI contains the
   * translated text.
   */
  static #assertPanelHasTranslatedText() {
    const { textArea, fromMenuList, toMenuList } =
      SelectTranslationsPanel.elements;
    const fromLanguage = fromMenuList.value;
    const toLanguage = toMenuList.value;
    SelectTranslationsTestUtils.#assertPanelTextAreaDirection(toLanguage);
    SelectTranslationsTestUtils.#assertConditionalUIEnabled({
      textArea: true,
      copyButton: true,
      translateFullPageButton: true,
    });

    if (fromLanguage === toLanguage) {
      is(
        SelectTranslationsPanel.getSourceText(),
        SelectTranslationsPanel.getTranslatedText(),
        "The source text should passthrough as the translated text."
      );
      return;
    }

    const translatedSuffix = ` [${fromLanguage} to ${toLanguage}]`;
    ok(
      textArea.value.endsWith(translatedSuffix),
      `Translated text should match ${fromLanguage} to ${toLanguage}`
    );
    is(
      SelectTranslationsPanel.getSourceText().length,
      SelectTranslationsPanel.getTranslatedText().length -
        translatedSuffix.length,
      "Expected translated text length to correspond to the source text length."
    );
  }

  /**
   * Asserts the enabled state of action buttons in the SelectTranslationsPanel.
   *
   * @param {boolean} expectEnabled - Whether the buttons should be enabled (true) or not (false).
   */
  static #assertConditionalUIEnabled({
    copyButton: copyButtonEnabled,
    translateFullPageButton: translateFullPageButtonEnabled,
    textArea: textAreaEnabled,
  }) {
    const { copyButton, translateFullPageButton, textArea } =
      SelectTranslationsPanel.elements;
    is(
      copyButton.disabled,
      !copyButtonEnabled,
      `The copy button should be ${copyButtonEnabled ? "enabled" : "disabled"}.`
    );
    is(
      translateFullPageButton.disabled,
      !translateFullPageButtonEnabled,
      `The translate-full-page button should be ${
        translateFullPageButtonEnabled ? "enabled" : "disabled"
      }.`
    );
    is(
      textArea.disabled,
      !textAreaEnabled,
      `The translated-text area should be ${
        textAreaEnabled ? "enabled" : "disabled"
      }.`
    );
  }

  /**
   * Asserts that the selected from-language matches the provided language tag.
   *
   * @param {string} langTag - A BCP-47 language tag.
   */
  static assertSelectedFromLanguage(langTag = null) {
    const { fromMenuList } = SelectTranslationsPanel.elements;
    SelectTranslationsTestUtils.#assertSelectedLanguage(fromMenuList, langTag);
  }

  /**
   * Asserts that the selected to-language matches the provided language tag.
   *
   * @param {string} langTag - A BCP-47 language tag.
   */
  static assertSelectedToLanguage(langTag = null) {
    const { toMenuList } = SelectTranslationsPanel.elements;
    SelectTranslationsTestUtils.#assertSelectedLanguage(toMenuList, langTag);
  }

  /**
   * Asserts the selected language in the given  menu list if a langTag is provided.
   * If no langTag is given, asserts that the menulist displays the localized placeholder.
   *
   * @param {object} menuList - The menu list object to check.
   * @param {string} [langTag] - The optional language tag to assert against.
   */
  static #assertSelectedLanguage(menuList, langTag) {
    if (langTag) {
      SharedTranslationsTestUtils._assertSelectedLanguage(menuList, {
        langTag,
      });
    } else {
      SharedTranslationsTestUtils._assertSelectedLanguage(menuList, {
        l10nId: "translations-panel-choose-language",
      });
      SharedTranslationsTestUtils._assertHasFocus(menuList);
    }
  }

  /**
   * Simulates clicking the done button and waits for the panel to close.
   */
  static async clickDoneButton() {
    logAction();
    const { doneButton } = SelectTranslationsPanel.elements;
    assertVisibility({ visible: { doneButton } });
    await SelectTranslationsTestUtils.waitForPanelPopupEvent(
      "popuphidden",
      () => {
        click(doneButton, "Clicking the done button");
      }
    );
  }

  /**
   * Opens the context menu at a specified element on the page, based on the provided options.
   *
   * @param {Function} runInPage - A content-exposed function to run within the context of the page.
   * @param {object} options - Options for opening the context menu.
   *
   * The following options will only work when testing SELECT_TEST_PAGE_URL.
   *
   * @param {boolean} options.selectFrenchSection - Selects the section of French text.
   * @param {boolean} options.selectEnglishSection - Selects the section of English text.
   * @param {boolean} options.selectSpanishSection - Selects the section of Spanish text.
   * @param {boolean} options.selectFrenchSentence - Selects a French sentence.
   * @param {boolean} options.selectEnglishSentence - Selects an English sentence.
   * @param {boolean} options.selectSpanishSentence - Selects a Spanish sentence.
   * @param {boolean} options.openAtFrenchSection - Opens the context menu at the section of French text.
   * @param {boolean} options.openAtEnglishSection - Opens the context menu at the section of English text.
   * @param {boolean} options.openAtSpanishSection - Opens the context menu at the section of Spanish text.
   * @param {boolean} options.openAtFrenchSentence - Opens the context menu at a French sentence.
   * @param {boolean} options.openAtEnglishSentence - Opens the context menu at an English sentence.
   * @param {boolean} options.openAtSpanishSentence - Opens the context menu at a Spanish sentence.
   * @param {boolean} options.openAtFrenchHyperlink - Opens the context menu at a hyperlinked French text.
   * @param {boolean} options.openAtEnglishHyperlink - Opens the context menu at an hyperlinked English text.
   * @param {boolean} options.openAtSpanishHyperlink - Opens the context menu at a hyperlinked Spanish text.
   * @throws Throws an error if no valid option was provided for opening the menu.
   */
  static async openContextMenu(runInPage, options) {
    logAction();

    const maybeSelectContentFrom = async keyword => {
      const conditionVariableName = `select${keyword}`;
      const selectorFunctionName = `get${keyword}`;

      if (options[conditionVariableName]) {
        await runInPage(
          async (TranslationsTest, data) => {
            const selectorFunction =
              TranslationsTest.getSelectors()[data.selectorFunctionName];
            if (typeof selectorFunction === "function") {
              const paragraph = selectorFunction();
              TranslationsTest.selectContentElement(paragraph);
            }
          },
          { selectorFunctionName }
        );
      }
    };

    await maybeSelectContentFrom("FrenchSection");
    await maybeSelectContentFrom("EnglishSection");
    await maybeSelectContentFrom("SpanishSection");
    await maybeSelectContentFrom("FrenchSentence");
    await maybeSelectContentFrom("EnglishSentence");
    await maybeSelectContentFrom("SpanishSentence");

    const maybeOpenContextMenuAt = async keyword => {
      const optionVariableName = `openAt${keyword}`;
      const selectorFunctionName = `get${keyword}`;

      if (options[optionVariableName]) {
        await SharedTranslationsTestUtils._waitForPopupEvent(
          "contentAreaContextMenu",
          "popupshown",
          async () => {
            await runInPage(
              async (TranslationsTest, data) => {
                const selectorFunction =
                  TranslationsTest.getSelectors()[data.selectorFunctionName];
                if (typeof selectorFunction === "function") {
                  const element = selectorFunction();
                  await TranslationsTest.rightClickContentElement(element);
                }
              },
              { selectorFunctionName }
            );
          }
        );
      }
    };

    await maybeOpenContextMenuAt("FrenchSection");
    await maybeOpenContextMenuAt("EnglishSection");
    await maybeOpenContextMenuAt("SpanishSection");
    await maybeOpenContextMenuAt("FrenchSentence");
    await maybeOpenContextMenuAt("EnglishSentence");
    await maybeOpenContextMenuAt("SpanishSentence");
    await maybeOpenContextMenuAt("FrenchHyperlink");
    await maybeOpenContextMenuAt("EnglishHyperlink");
    await maybeOpenContextMenuAt("SpanishHyperlink");
  }

  /**
   * Handles language-model downloads for the SelectTranslationsPanel, ensuring that expected
   * UI states match based on the resolved download state.
   *
   * @param {object} options - Configuration options for downloads.
   * @param {function(number): Promise<void>} options.downloadHandler - The function to resolve or reject the downloads.
   * @param {boolean} [options.pivotTranslation] - Whether to expect a pivot translation.
   *
   * @returns {Promise<void>}
   */
  static async handleDownloads({ downloadHandler, pivotTranslation }) {
    if (downloadHandler) {
      await SelectTranslationsTestUtils.assertPanelViewActivelyTranslating();
      await downloadHandler(pivotTranslation ? 2 : 1);
    }
  }

  /**
   * Switches the selected from-language to the provided language tags
   *
   * @param {string[]} langTags - An array of BCP-47 language tags.
   * @param {object} options - Configuration options for the language change.
   * @param {boolean} options.openDropdownMenu - Determines whether the language change should be made via a dropdown menu or directly.
   *
   * @returns {Promise<void>}
   */
  static async changeSelectedFromLanguage(langTags, options) {
    logAction(langTags);
    const { fromMenuList, fromMenuPopup } = SelectTranslationsPanel.elements;
    const { openDropdownMenu } = options;

    const switchFn = openDropdownMenu
      ? SelectTranslationsTestUtils.#changeSelectedLanguageViaDropdownMenu
      : SelectTranslationsTestUtils.#changeSelectedLanguageDirectly;

    await switchFn(
      langTags,
      { menuList: fromMenuList, menuPopup: fromMenuPopup },
      options
    );
  }

  /**
   * Switches the selected to-language to the provided language tag.
   *
   * @param {string[]} langTags - An array of BCP-47 language tags.
   * @param {object} options - Options for selecting paragraphs and opening the context menu.
   * @param {boolean} options.openDropdownMenu - Determines whether the language change should be made via a dropdown menu or directly.
   * @param {Function} options.downloadHandler - Handler for initiating downloads post language change, if applicable.
   * @param {Function} options.onChangeLanguage - Callback function to be executed after the language change.
   *
   * @returns {Promise<void>}
   */
  static async changeSelectedToLanguage(langTags, options) {
    logAction(langTags);
    const { toMenuList, toMenuPopup } = SelectTranslationsPanel.elements;
    const { openDropdownMenu } = options;

    const switchFn = openDropdownMenu
      ? SelectTranslationsTestUtils.#changeSelectedLanguageViaDropdownMenu
      : SelectTranslationsTestUtils.#changeSelectedLanguageDirectly;

    await switchFn(
      langTags,
      { menuList: toMenuList, menuPopup: toMenuPopup },
      options
    );
  }

  /**
   * Directly changes the selected language to each provided language tag without using a dropdown menu.
   *
   * @param {string[]} langTags - An array of BCP-47 language tags for direct selection.
   * @param {object} elements - Elements required for changing the selected language.
   * @param {Element} elements.menuList - The menu list element where languages are directly changed.
   * @param {object} options - Configuration options for language change and additional actions.
   * @param {Function} options.downloadHandler - Handler for initiating downloads post language change, if applicable.
   * @param {Function} options.onChangeLanguage - Callback function to be executed after the language change.
   *
   * @returns {Promise<void>}
   */
  static async #changeSelectedLanguageDirectly(langTags, elements, options) {
    const { menuList } = elements;
    const { onChangeLanguage, downloadHandler } = options;

    for (const langTag of langTags) {
      const menuListUpdated = BrowserTestUtils.waitForMutationCondition(
        menuList,
        { attributes: true, attributeFilter: ["value"] },
        () => menuList.value === langTag
      );

      menuList.value = langTag;
      menuList.dispatchEvent(new Event("command"));
      await menuListUpdated;
    }

    menuList.focus();
    EventUtils.synthesizeKey("KEY_Enter");

    if (downloadHandler) {
      await SelectTranslationsTestUtils.handleDownloads(options);
    }

    if (onChangeLanguage) {
      await onChangeLanguage();
    }
  }

  /**
   * Changes the selected language by opening the dropdown menu for each provided language tag.
   *
   * @param {string[]} langTags - An array of BCP-47 language tags for selection via dropdown.
   * @param {object} elements - Elements involved in the dropdown language selection process.
   * @param {Element} elements.menuList - The element that triggers the dropdown menu.
   * @param {Element} elements.menuPopup - The dropdown menu element containing selectable languages.
   * @param {object} options - Configuration options for language change and additional actions.
   * @param {Function} options.downloadHandler - Handler for initiating downloads post language change, if applicable.
   * @param {Function} options.onChangeLanguage - Callback function to be executed after the language change.
   *
   * @returns {Promise<void>}
   */
  static async #changeSelectedLanguageViaDropdownMenu(
    langTags,
    elements,
    options
  ) {
    const { menuList, menuPopup } = elements;
    const { onChangeLanguage } = options;
    for (const langTag of langTags) {
      await SelectTranslationsTestUtils.waitForPanelPopupEvent(
        "popupshown",
        () => click(menuList)
      );

      const menuItem = menuPopup.querySelector(`[value="${langTag}"]`);
      await SelectTranslationsTestUtils.waitForPanelPopupEvent(
        "popuphidden",
        () => {
          click(menuItem);
          // Synthesizing a click on the menuitem isn't closing the popup
          // as a click normally would, so this tab keypress is added to
          // ensure the popup closes.
          EventUtils.synthesizeKey("KEY_Tab");
        }
      );

      await SelectTranslationsTestUtils.handleDownloads(options);
      if (onChangeLanguage) {
        await onChangeLanguage();
      }
    }
  }

  /**
   * Opens the Select Translations panel via the context menu based on specified options.
   *
   * @param {Function} runInPage - A content-exposed function to run within the context of the page.
   * @param {object} options - Options for selecting paragraphs and opening the context menu.
   *
   * The following options will only work when testing SELECT_TEST_PAGE_URL.
   *
   * @param {string}  options.expectedFromLanguage - The expected from-language tag.
   * @param {string}  options.expectedToLanguage - The expected to-language tag.
   * @param {boolean} options.selectFrenchSection - Selects the section of French text.
   * @param {boolean} options.selectEnglishSection - Selects the section of English text.
   * @param {boolean} options.selectSpanishSection - Selects the section of Spanish text.
   * @param {boolean} options.selectFrenchSentence - Selects a French sentence.
   * @param {boolean} options.selectEnglishSentence - Selects an English sentence.
   * @param {boolean} options.selectSpanishSentence - Selects a Spanish sentence.
   * @param {boolean} options.openAtFrenchSection - Opens the context menu at the section of French text.
   * @param {boolean} options.openAtEnglishSection - Opens the context menu at the section of English text.
   * @param {boolean} options.openAtSpanishSection - Opens the context menu at the section of Spanish text.
   * @param {boolean} options.openAtFrenchSentence - Opens the context menu at a French sentence.
   * @param {boolean} options.openAtEnglishSentence - Opens the context menu at an English sentence.
   * @param {boolean} options.openAtSpanishSentence - Opens the context menu at a Spanish sentence.
   * @param {boolean} options.openAtFrenchHyperlink - Opens the context menu at a hyperlinked French text.
   * @param {boolean} options.openAtEnglishHyperlink - Opens the context menu at an hyperlinked English text.
   * @param {boolean} options.openAtSpanishHyperlink - Opens the context menu at a hyperlinked Spanish text.
   * @param {Function} [options.onOpenPanel] - An optional callback function to execute after the panel opens.
   * @param {string|null} [message] - An optional message to log to info.
   * @throws Throws an error if the context menu could not be opened with the provided options.
   * @returns {Promise<void>}
   */
  static async openPanel(runInPage, options, message) {
    logAction();

    if (message) {
      info(message);
    }

    await SelectTranslationsTestUtils.assertContextMenuTranslateSelectionItem(
      runInPage,
      options,
      message
    );

    const menuItem = getById("context-translate-selection");

    await SelectTranslationsTestUtils.waitForPanelPopupEvent(
      "popupshown",
      async () => {
        click(menuItem);
        await closeContextMenuIfOpen();
      },
      async () => {
        const { onOpenPanel } = options;
        await SelectTranslationsTestUtils.handleDownloads(options);
        if (onOpenPanel) {
          await onOpenPanel();
        }
      }
    );

    const { expectedFromLanguage, expectedToLanguage } = options;
    if (expectedFromLanguage !== undefined) {
      SelectTranslationsTestUtils.assertSelectedFromLanguage(
        expectedFromLanguage
      );
    }
    if (expectedToLanguage !== undefined) {
      SelectTranslationsTestUtils.assertSelectedToLanguage(expectedToLanguage);
    }
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
  static async waitForPanelPopupEvent(
    eventName,
    callback,
    postEventAssertion = null
  ) {
    // De-lazify the panel elements.
    SelectTranslationsPanel.elements;
    await SharedTranslationsTestUtils._waitForPopupEvent(
      "select-translations-panel",
      eventName,
      callback,
      postEventAssertion
    );
  }
}

class TranslationsSettingsTestUtils {
  /**
   * Opens the Translation Settings page by clicking the settings button sent in the argument.
   *
   * @param  {HTMLElement} settingsButton
   * @returns {Element}
   */
  static async openAboutPreferencesTranslationsSettingsPane(settingsButton) {
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
      translationsSettingsDescription: document.getElementById(
        "translations-settings-description"
      ),
      translateAlwaysHeader: document.getElementById(
        "translations-settings-always-translate"
      ),
      translateNeverHeader: document.getElementById(
        "translations-settings-never-translate"
      ),
      translateAlwaysMenuList: document.getElementById(
        "translations-settings-always-translate-list"
      ),
      translateNeverMenuList: document.getElementById(
        "translations-settings-never-translate-list"
      ),
      translateNeverSiteHeader: document.getElementById(
        "translations-settings-never-sites-header"
      ),
      translateNeverSiteDesc: document.getElementById(
        "translations-settings-never-sites"
      ),
      translateDownloadLanguagesHeader: document
        .getElementById("translations-settings-download-section")
        .querySelector("h2"),
      translateDownloadLanguagesLearnMore: document.getElementById(
        "download-languages-learn-more"
      ),
      translateDownloadLanguagesList: document.getElementById(
        "translations-settings-download-section"
      ),
    };

    return elements;
  }
}
