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
 * This function looks inside of a container for some element that has a label.
 * It runs in a loop every requestAnimationFrame until it finds the element. If
 * it doesn't find the element it throws an error.
 *
 * @param {Element} container
 * @param {string} label
 * @returns {Promise<HTMLElement>}
 */
function getElementByLabel(container, label) {
  return waitUntil(
    () => container.querySelector(`[label="${label}"]`),
    `Trying to find the button with the label "${label}".`
  );
}
/* exported getElementByLabel */

/**
 * This function looks inside of a container for some element that has a tooltip.
 * It runs in a loop every requestAnimationFrame until it finds the element. If
 * it doesn't find the element it throws an error.
 *
 * @param {Element} container
 * @param {string} tooltip
 * @returns {Promise<HTMLElement>}
 */
function getElementByTooltip(container, tooltip) {
  return waitUntil(
    () => container.querySelector(`[tooltiptext="${tooltip}"]`),
    `Trying to find the button with the tooltip "${tooltip}".`
  );
}
/* exported getElementByTooltip */

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
/* exported getElementByXPath */

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
  // Fallback on aria-label if there are no results for the text xpath.
  const xpath = `//*[contains(text(), '${text}')] | //*[contains(@aria-label, '${text}')]`;
  return waitUntil(
    () => getElementByXPath(document, xpath),
    `Trying to find the element with the text "${text}".`
  );
}
/* exported getElementFromDocumentByText */

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
/* exported maybeGetElementFromDocumentByText */

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
/* exported makeSureProfilerPopupIsEnabled */

/**
 * XUL popups will fire the popupshown and popuphidden events. These will fire for
 * any type of popup in the browser. This function waits for one of those events, and
 * checks that the viewId of the popup is PanelUI-profiler
 *
 * @param {Window} window
 * @param {"popupshown" | "popuphidden"} eventName
 * @returns {Promise<void>}
 */
function waitForProfilerPopupEvent(window, eventName) {
  return new Promise(resolve => {
    function handleEvent(event) {
      if (event.target.getAttribute("viewId") === "PanelUI-profiler") {
        window.removeEventListener(eventName, handleEvent);
        resolve();
      }
    }
    window.addEventListener(eventName, handleEvent);
  });
}
/* exported waitForProfilerPopupEvent */

/**
 * Do not use this directly in a test. Prefer withPopupOpen and openPopupAndEnsureCloses.
 *
 * This function toggles the profiler menu button, and then uses user gestures
 * to click it open. It waits a tick to make sure it has a chance to initialize.
 * @param {Window} window
 * @return {Promise<void>}
 */
async function _toggleOpenProfilerPopup(window) {
  info("Toggle open the profiler popup.");

  info("> Find the profiler menu button.");
  const profilerDropmarker = window.document.getElementById(
    "profiler-button-dropmarker"
  );
  if (!profilerDropmarker) {
    throw new Error(
      "Could not find the profiler button dropmarker in the toolbar."
    );
  }

  const popupShown = waitForProfilerPopupEvent(window, "popupshown");

  info("> Trigger a click on the profiler button dropmarker.");
  await EventUtils.synthesizeMouseAtCenter(profilerDropmarker, {}, window);

  if (profilerDropmarker.getAttribute("open") !== "true") {
    throw new Error(
      "This test assumes that the button will have an open=true attribute after clicking it."
    );
  }

  info("> Wait for the popup to be shown.");
  await popupShown;
  // Also wait a tick in case someone else is subscribing to the "popupshown" event
  // and is doing synchronous work with it.
  await tick();
}

/**
 * Do not use this directly in a test. Prefer withPopupOpen.
 *
 * This function uses a keyboard shortcut to close the profiler popup.
 * @param {Window} window
 * @return {Promise<void>}
 */
async function _closePopup(window) {
  const popupHiddenPromise = waitForProfilerPopupEvent(window, "popuphidden");
  info("> Trigger an escape key to hide the popup");
  EventUtils.synthesizeKey("KEY_Escape");

  info("> Wait for the popup to be hidden.");
  await popupHiddenPromise;
  // Also wait a tick in case someone else is subscribing to the "popuphidden" event
  // and is doing synchronous work with it.
  await tick();
}

/**
 * Perform some action on the popup, and close it afterwards.
 * @param {Window} window
 * @param {() => Promise<void>} callback
 */
async function withPopupOpen(window, callback) {
  await _toggleOpenProfilerPopup(window);
  await callback();
  await _closePopup(window);
}
/* exported withPopupOpen */

/**
 * This function opens the profiler popup, but also ensures that something else closes
 * it before the end of the test. This is useful for tests that trigger the profiler
 * popup to close through an implicit action, like opening a tab.
 *
 * @param {Window} window
 * @param {() => Promise<void>} callback
 */
async function openPopupAndEnsureCloses(window, callback) {
  await _toggleOpenProfilerPopup(window);
  // We want to ensure the popup gets closed by the test, during the callback.
  const popupHiddenPromise = waitForProfilerPopupEvent(window, "popuphidden");
  await callback();
  info("> Verifying that the popup was closed by the test.");
  await popupHiddenPromise;
}
/* exported openPopupAndEnsureCloses */

/**
 * This function overwrites the default profiler.firefox.com URL for tests. This
 * ensures that the tests do not attempt to access external URLs.
 * The origin needs to be on the allowlist in validateProfilerWebChannelUrl,
 * otherwise the WebChannel won't work. ("http://example.com" is on that list.)
 *
 * @param {string} origin - For example: http://example.com
 * @param {string} pathname - For example: /my/testing/frontend.html
 * @returns {Promise}
 */
function setProfilerFrontendUrl(origin, pathname) {
  return SpecialPowers.pushPrefEnv({
    set: [
      // Make sure observer and testing function run in the same process
      ["devtools.performance.recording.ui-base-url", origin],
      ["devtools.performance.recording.ui-base-url-path", pathname],
    ],
  });
}
/* exported setProfilerFrontendUrl */

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
    switch (gBrowser.selectedTab.label) {
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
/* exported checkTabLoadedProfile */

/**
 * This function checks the url of a tab so we can assert the frontend's url
 * with our expected url. This function runs in a loop every
 * requestAnimationFrame, and checks for a initialTitle. Asserts as soon as it
 * finds that title. We don't have to look for success title or error title
 * since we only care about the url.
 * @param {{
 *     initialTitle: string,
 *     successTitle: string,
 *     errorTitle: string,
 *     expectedUrl: string
 *   }}
 */
async function waitForTabUrl({
  initialTitle,
  successTitle,
  errorTitle,
  expectedUrl,
}) {
  const logPeriodically = createPeriodicLogger();

  info(`Waiting for the selected tab to have the url "${expectedUrl}".`);

  return waitUntil(() => {
    switch (gBrowser.selectedTab.label) {
      case initialTitle:
      case successTitle:
        if (gBrowser.currentURI.spec === expectedUrl) {
          ok(true, `The selected tab has the url ${expectedUrl}`);
          BrowserTestUtils.removeTab(gBrowser.selectedTab);
          return true;
        }
        throw new Error(
          `Found a different url on the fake frontend: ${gBrowser.currentURI.spec} (expecting ${expectedUrl})`
        );
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
/* exported waitForTabUrl */

/**
 * This function checks the document title of a tab as an easy way to pass
 * messages from a content page to the mochitest.
 * @param {string} title
 */
async function waitForTabTitle(title) {
  const logPeriodically = createPeriodicLogger();

  info(`Waiting for the selected tab to have the title "${title}".`);

  return waitUntil(() => {
    if (gBrowser.selectedTab.label === title) {
      ok(true, `The selected tab has the title ${title}`);
      return true;
    }
    logPeriodically(`> Waiting for the tab title to change.`);
    return false;
  });
}
/* exported waitForTabTitle */

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
      await TestUtils.waitForCondition(
        () =>
          contentBrowser.contentDocument.getElementById("root")
            .firstElementChild,
        "Document's root has been populated"
      );
      return callback(contentBrowser.contentDocument);
    }
  );
}
/* exported withAboutProfiling */

/**
 * Open DevTools and view the performance-new tab. After running the callback, clean
 * up the test.
 *
 * @param {string} [url="about:blank"] url for the new tab
 * @param {(Document, Document) => unknown} callback: the first parameter is the
 *                                          devtools panel's document, the
 *                                          second parameter is the opened tab's
 *                                          document.
 * @param {Window} [aWindow] The browser's window object we target
 * @returns {Promise<void>}
 */
async function withDevToolsPanel(url, callback, aWindow = window) {
  if (typeof url === "function") {
    aWindow = callback ?? window;
    callback = url;
    url = "about:blank";
  }

  const { gBrowser } = aWindow;

  const {
    gDevTools,
  } = require("resource://devtools/client/framework/devtools.js");

  info(`Create a new tab with url "${url}".`);
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  info("Begin to open the DevTools and the performance-new panel.");
  const toolbox = await gDevTools.showToolboxForTab(tab, {
    toolId: "performance",
  });

  const { document } = toolbox.getCurrentPanel().panelWin;

  info("The performance-new panel is now open and ready to use.");
  await callback(document, tab.linkedBrowser.contentDocument);

  info("About to remove the about:blank tab");
  await toolbox.destroy();

  // The previous asynchronous functions may resolve within a tick after opening a new tab.
  // We shouldn't remove the newly opened tab in the same tick.
  // Wait for the next tick here.
  await TestUtils.waitForTick();

  // Take care to register the TabClose event before we call removeTab, to avoid
  // race issues.
  const waitForClosingPromise = BrowserTestUtils.waitForTabClosing(tab);
  BrowserTestUtils.removeTab(tab);
  info("Requested closing the about:blank tab, waiting...");
  await waitForClosingPromise;
  info("The about:blank tab is now removed.");
}
/* exported withDevToolsPanel */

/**
 * Start and stop the profiler to get the current active configuration. This is
 * done programmtically through the nsIProfiler interface, rather than through click
 * interactions, since the about:profiling page does not include buttons to control
 * the recording.
 *
 * @returns {Object}
 */
function getActiveConfiguration() {
  const BackgroundJSM = ChromeUtils.import(
    "resource://devtools/client/performance-new/shared/background.jsm.js"
  );

  const { startProfiler, stopProfiler } = BackgroundJSM;

  info("Start the profiler with the current about:profiling configuration.");
  startProfiler("aboutprofiling");

  // Immediately pause the sampling, to make sure the test runs fast. The profiler
  // only needs to be started to initialize the configuration.
  Services.profiler.Pause();

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
/* exported getActiveConfiguration */

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
/* exported activeConfigurationHasFeature */

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
/* exported activeConfigurationHasThread */

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
/* exported devToolsActiveConfigurationHasFeature */

/**
 * This adapts the expectation using the current build's available profiler
 * features.
 * @param {string} fixture It can be either already trimmed or untrimmed.
 * @returns {string}
 */
function _adaptCustomPresetExpectationToCustomBuild(fixture) {
  const supportedFeatures = Services.profiler.GetFeatures();
  info("Supported features are: " + supportedFeatures.join(", "));

  // Some platforms do not support stack walking, we can adjust the passed
  // fixture so that tests are passing in these platforms too.
  // Most notably MacOS outside of Nightly and DevEdition.
  if (!supportedFeatures.includes("stackwalk")) {
    info(
      "Supported features do not include stackwalk, let's remove the Native Stacks from the expected output."
    );
    fixture = fixture.replace(/^.*Native Stacks.*\n/m, "");
  }

  return fixture;
}

/**
 * Get the content of the preset description.
 * @param {Element} devtoolsDocument
 * @returns {string}
 */
function getDevtoolsCustomPresetContent(devtoolsDocument) {
  return devtoolsDocument.querySelector(".perf-presets-custom").innerText;
}
/* exported getDevtoolsCustomPresetContent */

/**
 * This checks if the content of the preset description equals the fixture in
 * string form.
 * @param {Element} devtoolsDocument
 * @param {string} fixture
 */
function checkDevtoolsCustomPresetContent(devtoolsDocument, fixture) {
  // This removes all indentations and any start or end new line and other space characters.
  fixture = fixture.replace(/^\s+/gm, "").trim();
  // This removes unavailable features from the fixture content.
  fixture = _adaptCustomPresetExpectationToCustomBuild(fixture);
  is(getDevtoolsCustomPresetContent(devtoolsDocument), fixture);
}
/* exported checkDevtoolsCustomPresetContent */

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
/* exported getNearestInputFromText */

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
/* exported getActiveButtonFromText */

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
/* exported waitForProfilerMenuButton */

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
/* exported makeSureProfilerPopupIsDisabled */

/**
 * Open the WebChannel test document, that will enable the profiler popup via
 * WebChannel.
 * @param {Function} callback
 */
function withWebChannelTestDocument(callback) {
  return BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "http://example.com/browser/devtools/client/performance-new/test/browser/webchannel.html",
    },
    callback
  );
}
/* exported withWebChannelTestDocument */

// This has been stolen from the great library dom-testing-library.
// See https://github.com/testing-library/dom-testing-library/blob/91b9dc3b6f5deea88028e97aab15b3b9f3289a2a/src/events.js#L104-L123
// function written after some investigation here:
// https://github.com/facebook/react/issues/10135#issuecomment-401496776
function setNativeValue(element, value) {
  const { set: valueSetter } =
    Object.getOwnPropertyDescriptor(element, "value") || {};
  const prototype = Object.getPrototypeOf(element);
  const { set: prototypeValueSetter } =
    Object.getOwnPropertyDescriptor(prototype, "value") || {};
  if (prototypeValueSetter && valueSetter !== prototypeValueSetter) {
    prototypeValueSetter.call(element, value);
  } else {
    /* istanbul ignore if */
    // eslint-disable-next-line no-lonely-if -- Can't be ignored by istanbul otherwise
    if (valueSetter) {
      valueSetter.call(element, value);
    } else {
      throw new Error("The given element does not have a value setter");
    }
  }
}
/* exported setNativeValue */

/**
 * Set a React-friendly input value. Doing this the normal way doesn't work.
 * This reuses the previous function setNativeValue stolen from
 * dom-testing-library.
 *
 * See https://github.com/facebook/react/issues/10135
 *
 * @param {HTMLInputElement} input
 * @param {string} value
 */
function setReactFriendlyInputValue(input, value) {
  setNativeValue(input, value);

  // 'change' instead of 'input', see https://github.com/facebook/react/issues/11488#issuecomment-381590324
  input.dispatchEvent(new Event("change", { bubbles: true }));
}
/* exported setReactFriendlyInputValue */

/**
 * The recording state is the internal state machine that represents the async
 * operations that are going on in the profiler. This function sets up a helper
 * that will obtain the Redux store and query this internal state. This is useful
 * for unit testing purposes.
 *
 * @param {Document} document
 */
function setupGetRecordingState(document) {
  const selectors = require("resource://devtools/client/performance-new/store/selectors.js");
  const store = document.defaultView.gStore;
  if (!store) {
    throw new Error("Could not find the redux store on the window object.");
  }
  return () => selectors.getRecordingState(store.getState());
}
/* exported setupGetRecordingState */
