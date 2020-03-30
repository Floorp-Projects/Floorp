/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const BackgroundJSM = ChromeUtils.import(
  "resource://devtools/client/performance-new/popup/background.jsm.js"
);

registerCleanupFunction(() => {
  BackgroundJSM.revertRecordingPreferences();
});

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
 * This function looks inside of a document for some element that has a label.
 * It runs in a loop every requestAnimationFrame until it finds the element. If
 * it doesn't find the element it throws an error.
 *
 * @param {string} label
 * @returns {Promise<HTMLElement>}
 */
function getElementByLabel(document, label) {
  return waitUntil(
    () => document.querySelector(`[label="${label}"]`),
    `Trying to find the button with the label "${label}".`
  );
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
 * This function is similar to getElementFromDocumentByText, but it immediately
 * returns and does not wait for an element to exist.
 * @param {HTMLDocument} document
 * @param {string} text
 * @returns {HTMLElement?}
 */
function maybeGetElementFromDocumentByText(document, text) {
  info(`Immediately trying to find the element with the text "${text}".`);
  const xpath = `//*[contains(text(), '${text}')]`;
  return getElementByXPath(document, xpath);
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

  if (!ProfilerMenuButton.isInNavbar()) {
    // Make sure the feature flag is enabled.
    SpecialPowers.pushPrefEnv({
      set: [["devtools.performance.popup.feature-flag", true]],
    });

    info("> The menu button is not in the nav bar, add it.");
    ProfilerMenuButton.addToNavbar(document);

    await waitUntil(
      () => gBrowser.ownerDocument.getElementById("profiler-button"),
      "> Waiting until the profiler button is added to the browser."
    );

    await SimpleTest.promiseFocus(gBrowser.ownerGlobal);

    registerCleanupFunction(() => {
      info(
        "Clean up after the test by disabling the profiler popup menu button."
      );
      if (!ProfilerMenuButton.isInNavbar()) {
        throw new Error(
          "Expected the profiler popup to still be in the navbar during the test cleanup."
        );
      }
      ProfilerMenuButton.remove();
    });
  } else {
    info("> The menu button was already enabled.");
  }
}

/**
 * This function toggles the profiler menu button, and then uses user gestures
 * to click it open. It waits a tick to make sure it has a chance to initialize.
 * @return {Promise<void>}
 */
async function toggleOpenProfilerPopup() {
  info("Toggle open the profiler popup.");

  info("> Find the profiler menu button.");
  const profilerButton = document.getElementById("profiler-button");
  if (!profilerButton) {
    throw new Error("Could not find the profiler button in the menu.");
  }

  info("> Trigger a click on the profiler menu button.");
  profilerButton.click();
  await tick();
}

/**
 * This function overwrites the default profiler.firefox.com URL for tests. This
 * ensures that the tests do not attempt to access external URLs.
 * @param {string} url
 * @returns {Promise}
 */
function setProfilerFrontendUrl(url) {
  info(
    "Setting the profiler URL to the fake frontend. Note that this doesn't currently " +
      "support the WebChannels, so expect a few error messages about the WebChannel " +
      "URLs not being correct."
  );
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
 * This function checks the document title of a tab as an easy way to pass
 * messages from a content page to the mochitest.
 * @param {string} title
 */
async function waitForTabTitle(title) {
  const logPeriodically = createPeriodicLogger();

  info(`Waiting for the selected tab to have the title "${title}".`);

  return waitUntil(() => {
    if (gBrowser.selectedTab.textContent === title) {
      ok(true, `The selected tab has the title ${title}`);
      return true;
    }
    logPeriodically(`> Waiting for the tab title to change.`);
    return false;
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
function withAboutProfiling(callback) {
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
 * Open DevTools and view the performance-new tab. After running the callback, clean
 * up the test.
 *
 * @template T
 * @param {(Document) => T} callback
 * @returns {Promise<T>}
 */
async function withDevToolsPanel(callback) {
  SpecialPowers.pushPrefEnv({
    set: [["devtools.performance.new-panel-enabled", "true"]],
  });

  const { gDevTools } = require("devtools/client/framework/devtools");
  const { TargetFactory } = require("devtools/client/framework/target");

  info("Create a new about:blank tab.");
  const tab = BrowserTestUtils.addTab(gBrowser, "about:blank");

  info("Begin to open the DevTools and the performance-new panel.");
  const target = await TargetFactory.forTab(tab);
  const toolbox = await gDevTools.showToolbox(target, "performance");

  const { document } = toolbox.getCurrentPanel().panelWin;

  info("The performance-new panel is now open and ready to use.");
  await callback(document);

  info("About to remove the about:blank tab");
  await toolbox.destroy();
  BrowserTestUtils.removeTab(tab);
  info("The about:blank tab is now removed.");
  await new Promise(resolve => setTimeout(resolve, 500));
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
  const { startProfiler, stopProfiler } = BackgroundJSM;

  info("Start the profiler with the current about:profiling configuration.");
  startProfiler("aboutprofiling");

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
 * Use user driven events to start the profiler, and then get the active configuration
 * of the profiler. This is similar to functions in the head.js file, but is specific
 * for the DevTools situation. The UI complains if the profiler stops unexpectedly.
 *
 * @param {Document} document
 * @param {string} feature
 * @returns {boolean}
 */
async function devToolsActiveConfigurationHasFeature(document, feature) {
  info("Get the active configuration of the profiler via user driven events.");
  const start = await getActiveButtonFromText(document, "Start recording");
  info("Click the button to start recording.");
  start.click();

  // Get the cancel button first, so that way we know the profile has actually
  // been recorded.
  const cancel = await getActiveButtonFromText(document, "Cancel recording");

  const { activeConfiguration } = Services.profiler;
  if (!activeConfiguration) {
    throw new Error(
      "Expected to find an active configuration for the profile."
    );
  }

  info("Click the cancel button to discard the profile..");
  cancel.click();

  // Wait until the start button is back.
  await getActiveButtonFromText(document, "Start recording");

  return activeConfiguration.features.includes(feature);
}

/**
 * Selects an element with some given text, then it walks up the DOM until it finds
 * an input or select element via a call to querySelector.
 *
 * @param {Document} document
 * @param {string} text
 * @param {HTMLInputElement}
 */
async function getNearestInputFromText(document, text) {
  const textElement = await getElementFromDocumentByText(document, text);
  if (textElement.control) {
    // This is a label, just grab the input.
    return textElement.control;
  }
  // A non-label node
  let next = textElement;
  while ((next = next.parentElement)) {
    const input = next.querySelector("input, select");
    if (input) {
      return input;
    }
  }
  throw new Error("Could not find an input or select near the text element.");
}

/**
 * Grabs the closest button element from a given snippet of text, and make sure
 * the button is not disabled.
 *
 * @param {Document} document
 * @param {string} text
 * @param {HTMLButtonElement}
 */
async function getActiveButtonFromText(document, text) {
  // This could select a span inside the button, or the button itself.
  let button = await getElementFromDocumentByText(document, text);

  while (button.tagName !== "button") {
    // Walk up until a button element is found.
    button = button.parentElement;
    if (!button) {
      throw new Error(`Unable to find a button from the text "${text}"`);
    }
  }

  await waitUntil(
    () => !button.disabled,
    "Waiting until the button is not disabled."
  );

  return button;
}

/**
 * Wait until the profiler menu button is added.
 *
 * @returns Promise<void>
 */
async function waitForProfilerMenuButton() {
  info("Checking if the profiler menu button is enabled.");
  await waitUntil(
    () => gBrowser.ownerDocument.getElementById("profiler-button"),
    "> Waiting until the profiler button is added to the browser."
  );
}

/**
 * Make sure the profiler popup is disabled for the test.
 */
async function makeSureProfilerPopupIsDisabled() {
  info("Make sure the profiler popup is dsiabled.");

  info("> Load the profiler menu button module.");
  const { ProfilerMenuButton } = ChromeUtils.import(
    "resource://devtools/client/performance-new/popup/menu-button.jsm.js"
  );

  const isOriginallyInNavBar = ProfilerMenuButton.isInNavbar();

  if (isOriginallyInNavBar) {
    info("> The menu button is in the navbar, remove it for this test.");
    ProfilerMenuButton.remove();
  } else {
    info("> The menu button was not in the navbar yet.");
  }

  registerCleanupFunction(() => {
    info("Revert the profiler menu button to be back in its original place");
    if (isOriginallyInNavBar !== ProfilerMenuButton.isInNavbar()) {
      ProfilerMenuButton.remove();
    }
  });
}

/**
 * Open the WebChannel test document, that will enable the profiler popup via
 * WebChannel.
 * @param {Function} callback
 */
function withWebChannelTestDocument(callback) {
  return BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url:
        "http://example.com/browser/devtools/client/performance-new/test/browser/webchannel.html",
    },
    callback
  );
}

/**
 * Set a React-friendly input value. Doing this the normal way doesn't work.
 *
 * See https://github.com/facebook/react/issues/10135#issuecomment-500929024
 *
 * @param {HTMLInputElement} input
 * @param {string} value
 */
function setReactFriendlyInputValue(input, value) {
  const previousValue = input.value;

  input.value = value;

  const tracker = input._valueTracker;
  if (tracker) {
    tracker.setValue(previousValue);
  }

  // 'change' instead of 'input', see https://github.com/facebook/react/issues/11488#issuecomment-381590324
  input.dispatchEvent(new Event("change", { bubbles: true }));
}

/**
 * The recording state is the internal state machine that represents the async
 * operations that are going on in the profiler. This function sets up a helper
 * that will obtain the Redux store and query this internal state. This is useful
 * for unit testing purposes.
 *
 * @param {Document} document
 */
function setupGetRecordingState(document) {
  const selectors = require("devtools/client/performance-new/store/selectors");
  const store = document.defaultView.gStore;
  if (!store) {
    throw new Error("Could not find the redux store on the window object.");
  }
  return () => selectors.getRecordingState(store.getState());
}
