/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check

/**
 * This file controls the logic of the profiler popup view.
 */

/**
 * @typedef {ReturnType<typeof selectElementsInPanelview>} Elements
 * @typedef {ReturnType<typeof createViewControllers>} ViewController
 */

/**
 * @typedef {Object} State - The mutable state of the popup.
 * @property {Array<() => void>} cleanup - Functions to cleanup once the view is hidden.
 * @property {boolean} isInfoCollapsed
 */

import { createLazyLoaders } from "resource://devtools/client/performance-new/shared/typescript-lazy-load.sys.mjs";

const lazy = createLazyLoaders({
  PanelMultiView: () =>
    ChromeUtils.importESModule("resource:///modules/PanelMultiView.sys.mjs"),
  Background: () =>
    ChromeUtils.importESModule(
      "resource://devtools/client/performance-new/shared/background.sys.mjs"
    ),
});

/**
 * This function collects all of the selection of the elements inside of the panel.
 *
 * @param {XULElement} panelview
 */
function selectElementsInPanelview(panelview) {
  const document = panelview.ownerDocument;

  // Forcefully cast the window to the type Window
  /** @type {any} */
  const windowAny = document.defaultView;
  /** @type {Window} */
  const window = windowAny;

  /**
   * Get an element or throw an error if it's not found. This is more friendly
   * for TypeScript.
   *
   * @param {string} id
   * @return {HTMLElement}
   */
  function getElementById(id) {
    /** @type {HTMLElement | null} */
    // @ts-ignore - Bug 1674368
    const { PanelMultiView } = lazy.PanelMultiView();
    const element = PanelMultiView.getViewNode(document, id);
    if (!element) {
      throw new Error(`Could not find the element from the ID "${id}"`);
    }
    return element;
  }

  return {
    document,
    panelview,
    window,
    inactive: getElementById("PanelUI-profiler-inactive"),
    active: getElementById("PanelUI-profiler-active"),
    presetDescription: getElementById("PanelUI-profiler-content-description"),
    presetsEditSettings: getElementById(
      "PanelUI-profiler-content-edit-settings"
    ),
    presetsMenuList: /** @type {MenuListElement} */ (
      getElementById("PanelUI-profiler-presets")
    ),
    header: getElementById("PanelUI-profiler-header"),
    info: getElementById("PanelUI-profiler-info"),
    menupopup: getElementById("PanelUI-profiler-presets-menupopup"),
    infoButton: getElementById("PanelUI-profiler-info-button"),
    learnMore: getElementById("PanelUI-profiler-learn-more"),
    startRecording: getElementById("PanelUI-profiler-startRecording"),
    stopAndDiscard: getElementById("PanelUI-profiler-stopAndDiscard"),
    stopAndCapture: getElementById("PanelUI-profiler-stopAndCapture"),
    settingsSection: getElementById("PanelUI-profiler-content-settings"),
    contentRecording: getElementById("PanelUI-profiler-content-recording"),
  };
}

/**
 * This function returns an interface that can be used to control the view of the
 * panel based on the current mutable State.
 *
 * @param {State} state
 * @param {Elements} elements
 */
function createViewControllers(state, elements) {
  return {
    updateInfoCollapse() {
      const { header, info, infoButton } = elements;
      header.setAttribute(
        "isinfocollapsed",
        state.isInfoCollapsed ? "true" : "false"
      );
      // @ts-ignore - Bug 1674368
      infoButton.checked = !state.isInfoCollapsed;

      if (state.isInfoCollapsed) {
        const { height } = info.getBoundingClientRect();
        info.style.marginBlockEnd = `-${height}px`;
      } else {
        info.style.marginBlockEnd = "0";
      }
    },

    updatePresets() {
      const { presets, getRecordingSettings } = lazy.Background();
      const { presetName } = getRecordingSettings(
        "aboutprofiling",
        Services.profiler.GetFeatures()
      );
      const preset = presets[presetName];
      if (preset) {
        elements.presetDescription.style.display = "block";
        elements.document.l10n.setAttributes(
          elements.presetDescription,
          preset.l10nIds.popup.description
        );
        elements.presetsMenuList.value = presetName;
      } else {
        elements.presetDescription.style.display = "none";
        // We don't remove the l10n-id attribute as the element is hidden anyway.
        // It will be updated again when it's displayed next time.
        elements.presetsMenuList.value = "custom";
      }
    },

    updateProfilerState() {
      if (Services.profiler.IsActive()) {
        elements.inactive.hidden = true;
        elements.active.hidden = false;
        elements.settingsSection.hidden = true;
        elements.contentRecording.hidden = false;
      } else {
        elements.inactive.hidden = false;
        elements.active.hidden = true;
        elements.settingsSection.hidden = false;
        elements.contentRecording.hidden = true;
      }
    },

    createPresetsList() {
      // Check the DOM if the presets were built or not. We can't cache this value
      // in the `State` object, as the `State` object will be removed if the
      // button is removed from the toolbar, but the DOM changes will still persist.
      if (elements.menupopup.getAttribute("presetsbuilt") === "true") {
        // The presets were already built.
        return;
      }

      const { presets } = lazy.Background();
      const currentPreset = Services.prefs.getCharPref(
        "devtools.performance.recording.preset"
      );

      const menuitems = Object.entries(presets).map(([id, preset]) => {
        const { document, presetsMenuList } = elements;
        const menuitem = document.createXULElement("menuitem");
        document.l10n.setAttributes(menuitem, preset.l10nIds.popup.label);
        menuitem.setAttribute("value", id);
        if (id === currentPreset) {
          presetsMenuList.setAttribute("value", id);
        }
        return menuitem;
      });

      elements.menupopup.prepend(...menuitems);
      elements.menupopup.setAttribute("presetsbuilt", "true");
    },

    hidePopup() {
      const panel = elements.panelview.closest("panel");
      if (!panel) {
        throw new Error("Could not find the panel from the panelview.");
      }
      /** @type {any} */ (panel).hidePopup();
    },
  };
}

/**
 * Perform all of the business logic to present the popup view once it is open.
 *
 * @param {State} state
 * @param {Elements} elements
 * @param {ViewController} view
 */
function initializeView(state, elements, view) {
  view.createPresetsList();

  state.cleanup.push(() => {
    // The UI should be collapsed by default for the next time the popup
    // is open.
    state.isInfoCollapsed = true;
    view.updateInfoCollapse();
  });

  // Turn off all animations while initializing the popup.
  elements.header.setAttribute("animationready", "false");

  elements.window.requestAnimationFrame(() => {
    // Allow the elements to layout once, the updateInfoCollapse implementation measures
    // the size of the container. It needs to wait a second before the bounding box
    // returns an actual size.
    view.updateInfoCollapse();
    view.updateProfilerState();
    view.updatePresets();

    // Now wait for another rAF, and turn the animations back on.
    elements.window.requestAnimationFrame(() => {
      elements.header.setAttribute("animationready", "true");
    });
  });
}

/**
 * This function is in charge of settings all of the events handlers for the view.
 * The handlers must also add themselves to the `state.cleanup` for them to be
 * properly cleaned up once the view is destroyed.
 *
 * @param {State} state
 * @param {Elements} elements
 * @param {ViewController} view
 */
function addPopupEventHandlers(state, elements, view) {
  const { changePreset, startProfiler, stopProfiler, captureProfile } =
    lazy.Background();

  /**
   * Adds a handler that automatically is removed once the panel is hidden.
   *
   * @param {HTMLElement} element
   * @param {string} type
   * @param {(event: Event) => void} handler
   */
  function addHandler(element, type, handler) {
    element.addEventListener(type, handler);
    state.cleanup.push(() => {
      element.removeEventListener(type, handler);
    });
  }

  addHandler(elements.infoButton, "click", event => {
    // Any button command event in the popup will cause it to close. Prevent this
    // from happening on click.
    event.preventDefault();

    state.isInfoCollapsed = !state.isInfoCollapsed;
    view.updateInfoCollapse();
  });

  addHandler(elements.startRecording, "click", () => {
    startProfiler("aboutprofiling");
  });

  addHandler(elements.stopAndDiscard, "click", () => {
    stopProfiler();
  });

  addHandler(elements.stopAndCapture, "click", () => {
    captureProfile("aboutprofiling");
    view.hidePopup();
  });

  addHandler(elements.learnMore, "click", () => {
    elements.window.openWebLinkIn("https://profiler.firefox.com/docs/", "tab");
    view.hidePopup();
  });

  addHandler(elements.presetsMenuList, "command", () => {
    changePreset(
      "aboutprofiling",
      elements.presetsMenuList.value,
      Services.profiler.GetFeatures()
    );
    view.updatePresets();
  });

  addHandler(elements.presetsEditSettings, "click", () => {
    elements.window.openTrustedLinkIn("about:profiling", "tab");
    view.hidePopup();
  });

  // Update the view when the profiler starts/stops.
  // These are all events that can affect the current state of the profiler.
  const events = ["profiler-started", "profiler-stopped"];
  for (const event of events) {
    Services.obs.addObserver(view.updateProfilerState, event);
    state.cleanup.push(() => {
      Services.obs.removeObserver(view.updateProfilerState, event);
    });
  }
}

/**
 * Initialize everything needed for the popup to work fine.
 * @param {State} panelState
 * @param {XULElement} panelview
 */
export function initializePopup(panelState, panelview) {
  const panelElements = selectElementsInPanelview(panelview);
  const panelviewControllers = createViewControllers(panelState, panelElements);
  addPopupEventHandlers(panelState, panelElements, panelviewControllers);
  initializeView(panelState, panelElements, panelviewControllers);
}
