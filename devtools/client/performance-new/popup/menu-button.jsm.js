/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check
"use strict";

/**
 * @typedef {import("../@types/perf").PerformancePref} PerformancePref
 */

/**
 * This file controls the enabling and disabling of the menu button for the profiler.
 * Care should be taken to keep it minimal as it can be run with browser initialization.
 */

/**
 * TS-TODO
 *
 * This function replaces lazyRequireGetter, and TypeScript can understand it. It's
 * currently duplicated until we have consensus that TypeScript is a good idea.
 *
 * @template T
 * @type {(callback: () => T) => () => T}
 */
function requireLazy(callback) {
  /** @type {T | undefined} */
  let cache;
  return () => {
    if (cache === undefined) {
      cache = callback();
    }
    return cache;
  };
}

// Provide an exports object for the JSM to be properly read by TypeScript.
/** @type {any} */ (this).exports = {};

const lazyServices = requireLazy(() =>
  /** @type {import("resource://gre/modules/Services.jsm")} */
  (ChromeUtils.import("resource://gre/modules/Services.jsm"))
);
const lazyCustomizableUI = requireLazy(() =>
  /** @type {import("resource:///modules/CustomizableUI.jsm")} */
  (ChromeUtils.import("resource:///modules/CustomizableUI.jsm"))
);
const lazyCustomizableWidgets = requireLazy(() =>
  /** @type {import("resource:///modules/CustomizableWidgets.jsm")} */
  (ChromeUtils.import("resource:///modules/CustomizableWidgets.jsm"))
);
const lazyPopupPanel = requireLazy(() =>
  /** @type {import("devtools/client/performance-new/popup/panel.jsm.js")} */
  (ChromeUtils.import(
    "resource://devtools/client/performance-new/popup/panel.jsm.js"
  ))
);

/** @type {PerformancePref["PopupEnabled"]} */
const BUTTON_ENABLED_PREF = "devtools.performance.popup.enabled";
const WIDGET_ID = "profiler-button";

/**
 * @return {boolean}
 */
function isEnabled() {
  const { Services } = lazyServices();
  return Services.prefs.getBoolPref(BUTTON_ENABLED_PREF, false);
}

/**
 * @param {HTMLDocument} document
 * @param {boolean} isChecked
 * @return {void}
 */
function setMenuItemChecked(document, isChecked) {
  const menuItem = document.querySelector("#menu_toggleProfilerButtonMenu");
  if (!menuItem) {
    return;
  }
  menuItem.setAttribute("checked", isChecked.toString());
}

/**
 * Toggle the menu button, and initialize the widget if needed.
 *
 * @param {ChromeDocument} document - The browser's document.
 * @return {void}
 */
function toggle(document) {
  const { CustomizableUI } = lazyCustomizableUI();
  const { Services } = lazyServices();

  const toggledValue = !isEnabled();
  Services.prefs.setBoolPref(BUTTON_ENABLED_PREF, toggledValue);

  if (toggledValue) {
    initialize();
    CustomizableUI.addWidgetToArea(WIDGET_ID, CustomizableUI.AREA_NAVBAR);
  } else {
    setMenuItemChecked(document, false);
    CustomizableUI.destroyWidget(WIDGET_ID);

    // The widgets are not being properly destroyed. This is a workaround
    // until Bug 1552565 lands.
    const element = document.getElementById("PanelUI-profiler");
    delete (/** @type {any} */ (element._addedEventListeners));
  }
}

/**
 * This function takes the button element, and returns a function that's used to
 * update the profiler button whenever the profiler activation status changed.
 *
 * @param {HTMLElement} buttonElement
 * @returns {() => void}
 */
function updateButtonColorForElement(buttonElement) {
  return () => {
    const { Services } = lazyServices();
    const isRunning = Services.profiler.IsActive();

    // Use photon blue-60 when active.
    buttonElement.style.fill = isRunning ? "#0060df" : "";
  };
}

/**
 * This function creates the widget definition for the CustomizableUI. It should
 * only be run if the profiler button is enabled.
 * @return {void}
 */
function initialize() {
  const { CustomizableUI } = lazyCustomizableUI();
  const { CustomizableWidgets } = lazyCustomizableWidgets();
  const { Services } = lazyServices();

  const widget = CustomizableUI.getWidget(WIDGET_ID);
  if (widget && widget.provider == CustomizableUI.PROVIDER_API) {
    // This widget has already been created.
    return;
  }

  /** @type {null | (() => void)} */
  let observer = null;
  const viewId = "PanelUI-profiler";

  /**
   * This is mutable state that will be shared between panel displays.
   *
   * @type {import("devtools/client/performance-new/popup/panel.jsm.js").State}
   */
  const panelState = {
    cleanup: [],
    isInfoCollapsed: true,
  };

  const item = {
    id: WIDGET_ID,
    type: "view",
    viewId,
    tooltiptext: "profiler-button.tooltiptext",

    onViewShowing:
      /**
       * @type {(event: {
       *   target: ChromeHTMLElement | XULElement,
       *   detail: {
       *     addBlocker: (blocker: Promise<void>) => void
       *   }
       * }) => void}
       */
      event => {
        try {
          // The popup logic is stored in a separate script so it doesn't have
          // to be parsed at browser startup, and will only be lazily loaded
          // when the popup is viewed.
          const {
            selectElementsInPanelview,
            createViewControllers,
            addPopupEventHandlers,
            initializePopup,
          } = lazyPopupPanel();

          const panelElements = selectElementsInPanelview(event.target);
          const panelView = createViewControllers(panelState, panelElements);
          addPopupEventHandlers(panelState, panelElements, panelView);
          initializePopup(panelState, panelElements, panelView);
        } catch (error) {
          // Surface any errors better in the console.
          console.error(error);
        }
      },

    /**
     * @type {(event: { target: ChromeHTMLElement | XULElement }) => void}
     */
    onViewHiding(event) {
      // Clean-up the view. This removes all of the event listeners.
      for (const fn of panelState.cleanup) {
        fn();
      }
      panelState.cleanup = [];
    },

    /** @type {(document: HTMLDocument) => void} */
    onBeforeCreated: document => {
      /** @type {PerformancePref["PopupIntroDisplayed"]} */
      const popupIntroDisplayedPref =
        "devtools.performance.popup.intro-displayed";

      // Determine the state of the popup's info being collapsed BEFORE the view
      // is shown, and update the collapsed state. This way the transition animation
      // isn't run.
      panelState.isInfoCollapsed = Services.prefs.getBoolPref(
        popupIntroDisplayedPref
      );
      if (!panelState.isInfoCollapsed) {
        // We have displayed the intro, don't show it again by default.
        Services.prefs.setBoolPref(popupIntroDisplayedPref, true);
      }

      setMenuItemChecked(document, true);
    },

    /** @type {(document: HTMLElement) => void} */
    onCreated: buttonElement => {
      observer = updateButtonColorForElement(buttonElement);
      Services.obs.addObserver(observer, "profiler-started");
      Services.obs.addObserver(observer, "profiler-stopped");

      // Also run the observer right away to update the color if the profiler is
      // already running at startup.
      observer();
    },

    onDestroyed: () => {
      if (observer) {
        Services.obs.removeObserver(observer, "profiler-started");
        Services.obs.removeObserver(observer, "profiler-stopped");
        observer = null;
      }
    },
  };

  CustomizableUI.createWidget(item);
  CustomizableWidgets.push(item);
}

const ProfilerMenuButton = { toggle, initialize, isEnabled };

exports.ProfilerMenuButton = ProfilerMenuButton;

// Object.keys() confuses the linting which expects a static array expression.
// eslint-disable-next-line
var EXPORTED_SYMBOLS = Object.keys(exports);
