/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * This file controls the enabling and disabling of the menu button for the profiler.
 * Care should be taken to keep it minimal as it can be run with browser initialization.
 */

ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "CustomizableUI",
  "resource:///modules/CustomizableUI.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "CustomizableWidgets",
  "resource:///modules/CustomizableWidgets.jsm"
);

// The profiler's menu button and its popup can be enabled/disabled by the user.
// This is the pref to control whether the user has turned it on or not.
// This pref is repeated across many files in order to avoid loading this file if
// it's not needed. Make sure and search the rest of the codebase for other uses.
const BUTTON_ENABLED_PREF = "devtools.performance.popup.enabled";
const WIDGET_ID = "profiler-button";

function isEnabled() {
  return Services.prefs.getBoolPref(BUTTON_ENABLED_PREF, false);
}

function setMenuItemChecked(document, isChecked) {
  const menuItem = document.querySelector("#menu_toggleProfilerButtonMenu");
  if (!menuItem) {
    return;
  }
  menuItem.setAttribute("checked", isChecked);
}

/**
 * Toggle the menu button, and initialize the widget if needed.
 *
 * @param {Object} document - The browser's document.
 */
function toggle(document) {
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
    delete element._addedEventListeners;
  }
}

/**
 * This is a utility function to get the iframe from an event.
 * @param {Object} The event fired by the CustomizableUI interface, contains a target.
 */
function getIframeFromEvent(event) {
  const panelview = event.target;
  const document = panelview.ownerDocument;

  // Create the iframe, and append it.
  const iframe = document.getElementById("PanelUI-profilerIframe");
  if (!iframe) {
    throw new Error("Unable to select the PanelUI-profilerIframe.");
  }
  return iframe;
}

// This function takes the button element, and returns a function that's used to
// update the profiler button whenever the profiler activation status changed.
const updateButtonColorForElement = buttonElement => () => {
  const isRunning = Services.profiler.IsActive();

  // Use photon blue-60 when active.
  buttonElement.style.fill = isRunning ? "#0060df" : "";
};

/**
 * This function creates the widget definition for the CustomizableUI. It should
 * only be run if the profiler button is enabled.
 */
function initialize() {
  const widget = CustomizableUI.getWidget(WIDGET_ID);
  if (widget && widget.provider == CustomizableUI.PROVIDER_API) {
    // This widget has already been created.
    return;
  }

  let observer;

  const item = {
    id: WIDGET_ID,
    type: "view",
    viewId: "PanelUI-profiler",
    tooltiptext: "profiler-button.tooltiptext",
    onViewShowing: event => {
      const iframe = getIframeFromEvent(event);
      iframe.src = "chrome://devtools/content/performance-new/popup/popup.html";

      // Provide a mechanism for the iframe to close the popup.
      iframe.contentWindow.gClosePopup = () => {
        CustomizableUI.hidePanelForNode(iframe);
      };

      // Provide a mechanism for the iframe to resize the popup.
      iframe.contentWindow.gResizePopup = height => {
        iframe.style.height = `${Math.min(600, height)}px`;
      };
    },
    onViewHiding(event) {
      const iframe = getIframeFromEvent(event);
      // Unset the iframe src so that when the popup DOM element is moved, the popup's
      // contents aren't re-initialized.
      iframe.src = "";
    },
    onBeforeCreated: document => {
      setMenuItemChecked(document, true);
    },
    onCreated: buttonElement => {
      observer = updateButtonColorForElement(buttonElement);
      Services.obs.addObserver(observer, "profiler-started");
      Services.obs.addObserver(observer, "profiler-stopped");

      // Also run the observer right away to update the color if the profiler is
      // already running at startup.
      observer();
    },
    onDestroyed: () => {
      Services.obs.removeObserver(observer, "profiler-started");
      Services.obs.removeObserver(observer, "profiler-stopped");
      observer = null;
    },
  };
  CustomizableUI.createWidget(item);
  CustomizableWidgets.push(item);
}

const ProfilerMenuButton = { toggle, initialize };

var EXPORTED_SYMBOLS = ["ProfilerMenuButton"];
