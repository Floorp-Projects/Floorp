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
 * @param {Function} assertion
 * @param {string} message The messag for the assertion.
 * @returns {HTMLElement}
 */
function assertTranslationsButton(assertion, message) {
  return TestUtils.waitForCondition(() => {
    const button = document.getElementById("translations-button");
    if (!button) {
      return false;
    }
    if (assertion(button)) {
      ok(button, message);
      return button;
    }
    return false;
  }, message);
}

/**
 * Navigate to a URL and indicate a message as to why.
 */
function navigate(url, message) {
  info(message);
  BrowserTestUtils.loadURIString(gBrowser.selectedBrowser, url);
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
