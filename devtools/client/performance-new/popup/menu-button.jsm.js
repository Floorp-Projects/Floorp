/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check
"use strict";

/**
 * This file controls the enabling and disabling of the menu button for the profiler.
 * Care should be taken to keep it minimal as it can be run with browser initialization.
 */

// Provide an exports object for the JSM to be properly read by TypeScript.
/** @type {any} */ (this).exports = {};

const { createLazyLoaders } = ChromeUtils.import(
  "resource://devtools/client/performance-new/typescript-lazy-load.jsm.js"
);

const lazy = createLazyLoaders({
  Services: () => ChromeUtils.import("resource://gre/modules/Services.jsm"),
  CustomizableUI: () =>
    ChromeUtils.import("resource:///modules/CustomizableUI.jsm"),
  CustomizableWidgets: () =>
    ChromeUtils.import("resource:///modules/CustomizableWidgets.jsm"),
  PopupPanel: () =>
    ChromeUtils.import(
      "resource://devtools/client/performance-new/popup/panel.jsm.js"
    ),
});

const WIDGET_ID = "profiler-button";

/**
 * Add the profiler button to the navbar.
 *
 * @param {ChromeDocument} document  The browser's document.
 * @return {void}
 */
function addToNavbar(document) {
  const { CustomizableUI } = lazy.CustomizableUI();

  CustomizableUI.addWidgetToArea(WIDGET_ID, CustomizableUI.AREA_NAVBAR);
}

/**
 * Remove the widget and place it in the customization palette. This will also
 * disable the shortcuts.
 *
 * @return {void}
 */
function remove() {
  const { CustomizableUI } = lazy.CustomizableUI();
  CustomizableUI.removeWidgetFromArea(WIDGET_ID);
}

/**
 * See if the profiler menu button is in the navbar, or other active areas. The
 * placement is null when it's inactive in the customization palette.
 *
 * @return {boolean}
 */
function isInNavbar() {
  const { CustomizableUI } = lazy.CustomizableUI();
  return Boolean(CustomizableUI.getPlacementOfWidget("profiler-button"));
}

/**
 * Opens the popup for the profiler.
 * @param {Document} document
 */
function openPopup(document) {
  // First find the button.
  /** @type {HTMLButtonElement | null} */
  const button = document.querySelector("#profiler-button");
  if (!button) {
    throw new Error("Could not find the profiler button.");
  }
  button.click();
}

/**
 * This function creates the widget definition for the CustomizableUI. It should
 * only be run if the profiler button is enabled.
 * @param {(isEnabled: boolean) => void} toggleProfilerKeyShortcuts
 * @return {void}
 */
function initialize(toggleProfilerKeyShortcuts) {
  const { CustomizableUI } = lazy.CustomizableUI();
  const { CustomizableWidgets } = lazy.CustomizableWidgets();
  const { Services } = lazy.Services();

  const widget = CustomizableUI.getWidget(WIDGET_ID);
  if (widget && widget.provider == CustomizableUI.PROVIDER_API) {
    // This widget has already been created.
    return;
  }

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

  /**
   * Handle when the customization changes for the button. This event is not
   * very specific, and fires for any CustomizableUI widget. This event is
   * pretty rare to fire, and only affects users of the profiler button,
   * so it shouldn't have much overhead even if it runs a lot.
   */
  function handleCustomizationChange() {
    const isEnabled = isInNavbar();
    toggleProfilerKeyShortcuts(isEnabled);

    if (!isEnabled) {
      // The profiler menu button is no longer in the navbar, make sure that the
      // "intro-displayed" preference is reset.
      /** @type {import("../@types/perf").PerformancePref["PopupIntroDisplayed"]} */
      const popupIntroDisplayedPref =
        "devtools.performance.popup.intro-displayed";
      Services.prefs.setBoolPref(popupIntroDisplayedPref, false);

      if (Services.profiler.IsActive()) {
        Services.profiler.StopProfiler();
      }
    }
  }

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
          } = lazy.PopupPanel();

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

    /**
     * Perform any general initialization for this widget. This is called once per
     * browser window.
     *
     * @type {(document: HTMLDocument) => void}
     */
    onBeforeCreated: document => {
      /** @type {import("../@types/perf").PerformancePref["PopupIntroDisplayed"]} */
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

      // Handle customization event changes. If the profiler is no longer in the
      // navbar, then reset the popup intro preference.
      const window = document.defaultView;
      if (window) {
        /** @type {any} */ (window).gNavToolbox.addEventListener(
          "customizationchange",
          handleCustomizationChange
        );
      }

      toggleProfilerKeyShortcuts(isInNavbar());
    },

    /**
     * This method is used when we need to operate upon the button element itself.
     * This is called once per browser window.
     *
     * @type {(buttonElement: HTMLElement) => void}
     */
    onCreated: buttonElement => {
      const window = buttonElement?.ownerDocument?.defaultView;
      if (!window) {
        console.error(
          "Unable to find the window of the profiler button element."
        );
        return;
      }

      function updateButtonColor() {
        // Use photon blue-60 when active.
        buttonElement.style.fill = Services.profiler.IsActive()
          ? "#0060df"
          : "";
      }

      updateButtonColor();

      Services.obs.addObserver(updateButtonColor, "profiler-started");
      Services.obs.addObserver(updateButtonColor, "profiler-stopped");

      window.addEventListener("unload", () => {
        Services.obs.removeObserver(updateButtonColor, "profiler-started");
        Services.obs.removeObserver(updateButtonColor, "profiler-stopped");
      });
    },
  };

  CustomizableUI.createWidget(item);
  CustomizableWidgets.push(item);
}

const ProfilerMenuButton = {
  initialize,
  addToNavbar,
  isInNavbar,
  openPopup,
  remove,
};

exports.ProfilerMenuButton = ProfilerMenuButton;

// Object.keys() confuses the linting which expects a static array expression.
// eslint-disable-next-line
var EXPORTED_SYMBOLS = Object.keys(exports);
