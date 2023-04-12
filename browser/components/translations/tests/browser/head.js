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
 * Get an element by its l10n id, as this is a user-visible way to find an element.
 * The `l10nId` represents the text that a user would actually see.
 *
 * @param {string} l10nId
 */
function getByL10nId(l10nId) {
  const element = document.querySelector(`[data-l10n-id="${l10nId}"]`);
  if (!element) {
    throw new Error("Could not find the element by l10n id: " + l10nId);
  }
  const { visibility, display } = window.getComputedStyle(element);
  if (visibility !== "visible" || display === "none") {
    throw new Error("The element is not visible in the DOM: " + l10nId);
  }
  return element;
}

/**
 * XUL popups will fire the popupshown and popuphidden events. These will fire for
 * any type of popup in the browser. This function waits for one of those events, and
 * checks that the viewId of the popup is PanelUI-profiler
 *
 * @param {"popupshown" | "popuphidden"} eventName
 * @returns {Promise<void>}
 */
function waitForTranslationsPopupEvent(eventName) {
  return new Promise(resolve => {
    const panel = document.getElementById("translations-panel");
    if (!panel) {
      throw new Error("Unable to find the translations panel element.");
    }

    function handleEvent(event) {
      if (event.type === eventName) {
        panel.removeEventListener(eventName, handleEvent);
        // Resolve after a setTimeout so that any other event handlers will finish
        // first. Only then will the test resume.
        setTimeout(resolve, 0);
      }
    }

    panel.addEventListener(eventName, handleEvent);
  });
}
