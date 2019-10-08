/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Wait for a single requestAnimationFrame tick.
 */
function tick() {
  return new Promise(resolve => requestAnimationFrame(resolve));
}

/**
 * It can be confusing when waiting for something asynchronously. This function
 * logs out a message periodically (every 1 second) in order to create helpful
 * log messages.
 * @param {string} message
 * @returns {Function}
 */
function createPeriodicLogger() {
  let startTime = Date.now();
  let lastCount = 0;
  let lastMessage = null;

  return message => {
    if (lastMessage === message) {
      // The messages are the same, check if we should log them.
      const now = Date.now();
      const count = Math.floor((now - startTime) / 1000);
      if (count !== lastCount) {
        info(
          `${message} (After ${count} ${count === 1 ? "second" : "seconds"})`
        );
        lastCount = count;
      }
    } else {
      // The messages are different, log them now, and reset the waiting time.
      info(message);
      startTime = Date.now();
      lastCount = 0;
      lastMessage = message;
    }
  };
}

/**
 * Wait until a condition is fullfilled.
 * @param {Function} condition
 * @param {string?} logMessage
 * @return The truthy result of the condition.
 */
async function waitUntil(condition, message) {
  const logPeriodically = createPeriodicLogger();

  // Loop through the condition.
  while (true) {
    if (message) {
      logPeriodically(message);
    }
    const result = condition();
    if (result) {
      return result;
    }

    await tick();
  }
}

/**
 * This function will select a node from the XPath.
 * @returns {HTMLElement?}
 */
function getElementByXPath(document, path) {
  return document.evaluate(
    path,
    document,
    null,
    XPathResult.FIRST_ORDERED_NODE_TYPE,
    null
  ).singleNodeValue;
}

/**
 * This function looks inside of the profiler popup's iframe for some element
 * that contains some text. It runs in a loop every requestAnimationFrame until
 * it finds an element. If it doesn't find the element it throws an error.
 * It also doesn't assume the popup will be visible yet, as this popup showing
 * is an async event.
 * @param {string} text
 * @param {number} maxTicks (optional)
 */
async function getElementFromPopupByText(text) {
  const xpath = `//*[contains(text(), '${text}')]`;
  return waitUntil(() => {
    const iframe = document.getElementById("PanelUI-profilerIframe");
    if (iframe) {
      return getElementByXPath(iframe.contentDocument, xpath);
    }
    return null;
  }, `Trying to find the element with the text "${text}".`);
}

/**
 * Make sure the profiler popup is enabled.
 */
async function makeSureProfilerPopupIsEnabled() {
  info("Make sure the profiler popup is enabled.");

  info("> Load the profiler menu button.");
  const { ProfilerMenuButton } = ChromeUtils.import(
    "resource://devtools/client/performance-new/popup/menu-button.jsm.js"
  );

  if (!ProfilerMenuButton.isEnabled()) {
    info("> The menu button is not enabled, turn it on.");
    ProfilerMenuButton.toggle(document);

    await waitUntil(
      () => gBrowser.ownerDocument.getElementById("profiler-button"),
      "> Waiting until the profiler button is added to the browser."
    );

    await SimpleTest.promiseFocus(gBrowser.ownerGlobal);

    registerCleanupFunction(() => {
      info(
        "Clean up after the test by disabling the profiler popup menu button."
      );
      if (!ProfilerMenuButton.isEnabled()) {
        throw new Error(
          "Expected the profiler popup to still be enabled during the test cleanup."
        );
      }
      ProfilerMenuButton.toggle(document);
    });
  } else {
    info("> The menu button was already enabled.");
  }
}

/**
 * This function toggles the profiler menu button, and then uses user gestures
 * to click it open.
 */
function toggleOpenProfilerPopup() {
  info("Toggle open the profiler popup.");

  info("> Find the profiler menu button.");
  const profilerButton = document.getElementById("profiler-button");
  if (!profilerButton) {
    throw new Error("Could not find the profiler button in the menu.");
  }

  info("> Trigger a click on the profiler menu button.");
  profilerButton.click();
}

/**
 * This function overwrites the default profiler.firefox.com URL for tests. This
 * ensures that the tests do not attempt to access external URLs.
 * @param {string} url
 * @returns {Promise}
 */
function setProfilerFrontendUrl(url) {
  info("Setting the profiler URL to the fake frontend.");
  return SpecialPowers.pushPrefEnv({
    set: [
      // Make sure observer and testing function run in the same process
      ["devtools.performance.recording.ui-base-url", url],
      ["devtools.performance.recording.ui-base-url-path", ""],
    ],
  });
}

/**
 * This function checks the document title of a tab to see what the state is.
 * This creates a simple messaging mechanism between the content page and the
 * test harness. This function runs in a loop every requestAnimationFrame, and
 * checks for a sucess title. In addition, an "initialTitle" and "errorTitle"
 * can be specified for nicer test output.
 * @param {object}
 *   {
 *     initialTitle: string,
 *     successTitle: string,
 *     errorTitle: string
 *   }
 */
async function checkTabLoadedProfile({
  initialTitle,
  successTitle,
  errorTitle,
}) {
  const logPeriodically = createPeriodicLogger();

  info("Attempting to see if the selected tab can receive a profile.");

  return waitUntil(() => {
    switch (gBrowser.selectedTab.textContent) {
      case initialTitle:
        logPeriodically(`> Waiting for the profile to be received.`);
        return false;
      case successTitle:
        ok(true, "The profile was successfully injected to the page");
        BrowserTestUtils.removeTab(gBrowser.selectedTab);
        return true;
      case errorTitle:
        throw new Error(
          "The fake frontend indicated that there was an error injecting the profile."
        );
      default:
        logPeriodically(`> Waiting for the fake frontend tab to be loaded.`);
        return false;
    }
  });
}
