/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check
"use strict";

/**
 * @typedef {import("../@types/perf").PerformancePref} PerformancePref
 * */
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
    delete /** @type {any} */ (element)._addedEventListeners;
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

  /** @typedef {() => void} Observer */

  /** @type {null | Observer} */
  let observer = null;

  const item = {
    id: WIDGET_ID,
    type: "view",
    viewId: "PanelUI-profiler",
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
        const panelview = event.target;
        const document = panelview.ownerDocument;
        if (!document) {
          throw new Error(
            "Expected to find a document on the panelview element."
          );
        }

        // Create an iframe and append it to the panelview.
        const iframe = document.createXULElement("iframe");
        iframe.id = "PanelUI-profilerIframe";
        iframe.className = "PanelUI-developer-iframe";
        iframe.src =
          "chrome://devtools/content/performance-new/popup/popup.xhtml";

        panelview.appendChild(iframe);
        /** @type {any} - Cast to an any since we're assigning values to the window object. */
        const contentWindow = iframe.contentWindow;

        // Provide a mechanism for the iframe to close the popup.
        contentWindow.gClosePopup = () => {
          CustomizableUI.hidePanelForNode(iframe);
        };

        // Provide a mechanism for the iframe to resize the popup.
        /** @type {(height: number) => void} */
        contentWindow.gResizePopup = height => {
          iframe.style.height = `${Math.min(600, height)}px`;
        };

        // Tell the iframe whether we're in dark mode or not. This approach
        // unfortunately has two flaws. First, since this is a boolean setting,
        // we ignore any theme information and just flip on our preset dark mode
        // if the theme is dark enough. It might look out of place depending on
        // the system or Firefox theme in use. Second, we won't detect all dark
        // mode cases here. If the user is using the default Firefox theme, which
        // normally adjusts to system-themes, we'll only be able to detect dark
        // mode on Windows and Macintosh. In Linux, instead of having a global
        // dark mode setting, we use GTK-theme colors directly for theming, and
        // those won't be detected here. Similarly, if a Windows user is not using
        // the particular dark mode system setting, but instead using a darker
        // system theme, then that will also not be detected here.
        contentWindow.gIsDarkMode = document.documentElement.hasAttribute(
          "lwt-popup-brighttext"
        );

        // The popup has an annoying rendering "blip" when first rendering the react
        // components. This adds a blocker until the content is ready to show.
        event.detail.addBlocker(
          new Promise(resolve => {
            contentWindow.gReportReady = () => {
              // Delete the function gReportReady so we don't leave any dangling
              // references between windows.
              delete contentWindow.gReportReady;
              // Now resolve this promise to open the window.
              resolve();
            };
          })
        );
      },

    /**
     * @type {(event: { target: ChromeHTMLElement | XULElement }) => void}
     */
    onViewHiding(event) {
      const document = event.target.ownerDocument;

      // Create the iframe, and append it.
      const iframe = document.getElementById("PanelUI-profilerIframe");
      if (!iframe) {
        throw new Error("Unable to select the PanelUI-profilerIframe.");
      }

      // Remove the iframe so it doesn't leak.
      iframe.remove();
    },

    /** @type {(document: HTMLDocument) => void} */
    onBeforeCreated: document => {
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
