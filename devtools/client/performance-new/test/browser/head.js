/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Allow tests to use "require".
 */
const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");

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
 * @returns {Promise<HTMLElement>}
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
 * This function looks inside of a document for some element that contains
 * the given text. It runs in a loop every requestAnimationFrame until it
 * finds the element. If it doesn't find the element it throws an error.
 *
 * @param {HTMLDocument} document
 * @param {string} text
 * @returns {Promise<HTMLElement>}
 */
async function getElementFromDocumentByText(document, text) {
  const xpath = `//*[contains(text(), '${text}')]`;
  return waitUntil(
    () => getElementByXPath(document, xpath),
    `Trying to find the element with the text "${text}".`
  );
}
/**
 * This function is similar to getElementFromPopupByText, but it immediately
 * returns and does not wait for an element to exist.
 * @param {string} text
 * @returns {HTMLElement?}
 */
function maybeGetElementFromPopupByText(text) {
  info(`Immediately trying to find the element with the text "${text}".`);
  const xpath = `//*[contains(text(), '${text}')]`;
  return getElementByXPath(getIframeDocument(), xpath);
}

/**
 * Returns the popup's document.
 * @returns {Document}
 */
function getIframeDocument() {
  const iframe = document.getElementById("PanelUI-profilerIframe");
  if (!iframe) {
    throw new Error(
      "This function assumes the profiler iframe is already present."
    );
  }
  return iframe.contentDocument;
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

/**
 * Close the popup, and wait for it to be destroyed.
 */
async function closePopup() {
  const iframe = document.querySelector("#PanelUI-profilerIframe");

  if (!iframe) {
    throw new Error(
      "Could not find the profiler iframe when attempting to close the popup. Was it " +
        "already closed?"
    );
  }

  const panel = iframe.closest("panel");
  if (!panel) {
    throw new Error(
      "Could not find the closest panel to the profiler's iframe."
    );
  }

  info("Hide the profiler popup.");
  panel.hidePopup();

  info("Wait for the profiler popup to be completely hidden.");
  while (true) {
    if (!iframe.ownerDocument.contains(iframe)) {
      info("The iframe was removed.");
      return;
    }
    await tick();
  }
}

/**
 * Open about:profiling in a new tab, and output helpful log messages.
 *
 * @template T
 * @param {(Document) => T} callback
 * @returns {Promise<T>}
 */
function openAboutProfiling(callback) {
  info("Begin to open about:profiling in a new tab.");
  return BrowserTestUtils.withNewTab(
    "about:profiling",
    async contentBrowser => {
      info("about:profiling is now open in a tab.");
      return callback(contentBrowser.contentDocument);
    }
  );
}

/**
 * Start and stop the profiler to get the current active configuration. This is
 * done programmtically through the nsIProfiler interface, rather than through click
 * interactions, since the about:profiling page does not include buttons to control
 * the recording.
 *
 * @returns {Object}
 */
function getActiveConfiguration() {
  const { startProfiler, stopProfiler } = ChromeUtils.import(
    "resource://devtools/client/performance-new/popup/background.jsm.js"
  );

  info("Start the profiler with the current about:profiling configuration.");
  startProfiler();

  // Immediately pause the sampling, to make sure the test runs fast. The profiler
  // only needs to be started to initialize the configuration.
  Services.profiler.PauseSampling();

  const { activeConfiguration } = Services.profiler;
  if (!activeConfiguration) {
    throw new Error(
      "Expected to find an active configuration for the profile."
    );
  }

  info("Stop the profiler after getting the active configuration.");
  stopProfiler();

  return activeConfiguration;
}

/**
 * Start the profiler programmatically and check that the active configuration has
 * a feature enabled
 *
 * @param {string} feature
 * @return {boolean}
 */
function activeConfigurationHasFeature(feature) {
  const { features } = getActiveConfiguration();
  return features.includes(feature);
}

/**
 * Start the profiler programmatically and check that the active configuration is
 * tracking a thread.
 *
 * @param {string} thread
 * @return {boolean}
 */
function activeConfigurationHasThread(thread) {
  const { threads } = getActiveConfiguration();
  return threads.includes(thread);
}

/**
 * @param {HTMLElement} featureTextElement
 * @param {HTMLInputElement}
 */
function getFeatureInputFromLabel(featureTextElement) {
  const input = featureTextElement.parentElement.querySelector("input");
  if (!input) {
    throw new Error("Could not find the input near text element.");
  }
  return input;
}
