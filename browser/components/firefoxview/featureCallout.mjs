/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  AboutWelcomeParent: "resource:///actors/AboutWelcomeParent.jsm",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "featureTourProgress",
  "browser.firefox-view.feature-tour",
  '{"message":"","screen":"","complete":true}',
  _handlePrefChange,
  val => JSON.parse(val)
);

async function _handlePrefChange() {
  if (document.visibilityState === "hidden") {
    return;
  }
  let prefVal = lazy.featureTourProgress;
  if (prefVal.complete) {
    _endTour();
  } else if (prefVal.screen !== CURRENT_SCREEN?.id) {
    READY = false;
    let container = document.getElementById(CONTAINER_ID);
    container?.classList.add("hidden");
    // wait for fade out transition
    setTimeout(async () => {
      _loadConfig(lazy.featureTourProgress.message);
      container?.remove();
      await _renderCallout();
    }, TRANSITION_MS);
  }
}

function _addCalloutLinkElements() {
  function addStylesheet(href) {
    const link = document.head.appendChild(document.createElement("link"));
    link.rel = "stylesheet";
    link.href = href;
  }
  function addLocalization(hrefs) {
    hrefs.forEach(href => {
      // eslint-disable-next-line no-undef
      MozXULElement.insertFTLIfNeeded(href);
    });
  }

  // Update styling to be compatible with about:welcome bundle
  addStylesheet(
    "chrome://activity-stream/content/aboutwelcome/aboutwelcome.css"
  );

  addLocalization([
    "browser/newtab/onboarding.ftl",
    "browser/spotlight.ftl",
    "branding/brand.ftl",
    "browser/branding/brandings.ftl",
    "browser/newtab/asrouter.ftl",
    "browser/featureCallout.ftl",
  ]);
}

let CURRENT_SCREEN;
let CONFIG;
let RENDER_OBSERVER;
let READY = false;

const TRANSITION_MS = 500;
const CONTAINER_ID = "root";
const MESSAGES = [
  {
    id: "FIREFOX_VIEW_FEATURE_TOUR",
    template: "multistage",
    backdrop: "transparent",
    transitions: false,
    disableHistoryUpdates: true,
    screens: [
      {
        id: "FEATURE_CALLOUT_1",
        parent_selector: "#tabpickup-steps",
        content: {
          position: "callout",
          arrow_position: "top",
          title: {
            string_id: "callout-firefox-view-tab-pickup-title",
          },
          subtitle: {
            string_id: "callout-firefox-view-tab-pickup-subtitle",
          },
          logo: {
            imageURL: "chrome://browser/content/callout-tab-pickup.svg",
            darkModeImageURL:
              "chrome://browser/content/callout-tab-pickup-dark.svg",
            height: "128px",
          },
          primary_button: {
            label: {
              string_id: "callout-primary-advance-button-label",
            },
            action: {
              type: "SET_PREF",
              data: {
                pref: {
                  name: "browser.firefox-view.feature-tour",
                  value: JSON.stringify({
                    message: "FIREFOX_VIEW_FEATURE_TOUR",
                    screen: "FEATURE_CALLOUT_2",
                    complete: false,
                  }),
                },
              },
            },
          },
          dismiss_button: {
            action: {
              type: "SET_PREF",
              data: {
                pref: {
                  name: "browser.firefox-view.feature-tour",
                  value: JSON.stringify({
                    message: "FIREFOX_VIEW_FEATURE_TOUR",
                    screen: "FEATURE_CALLOUT_1",
                    complete: true,
                  }),
                },
              },
            },
          },
        },
      },
      {
        id: "FEATURE_CALLOUT_2",
        parent_selector: "#recently-closed-tabs-container",
        content: {
          position: "callout",
          arrow_position: "bottom",
          title: {
            string_id: "callout-firefox-view-recently-closed-title",
          },
          subtitle: {
            string_id: "callout-firefox-view-recently-closed-subtitle",
          },
          primary_button: {
            label: {
              string_id: "callout-primary-advance-button-label",
            },
            action: {
              type: "SET_PREF",
              data: {
                pref: {
                  name: "browser.firefox-view.feature-tour",
                  value: JSON.stringify({
                    message: "FIREFOX_VIEW_FEATURE_TOUR",
                    screen: "FEATURE_CALLOUT_3",
                    complete: false,
                  }),
                },
              },
            },
          },
          dismiss_button: {
            action: {
              type: "SET_PREF",
              data: {
                pref: {
                  name: "browser.firefox-view.feature-tour",
                  value: JSON.stringify({
                    message: "FIREFOX_VIEW_FEATURE_TOUR",
                    screen: "FEATURE_CALLOUT_2",
                    complete: true,
                  }),
                },
              },
            },
          },
        },
      },
      {
        id: "FEATURE_CALLOUT_3",
        parent_selector: "#colorways.content-container",
        content: {
          position: "callout",
          arrow_position: "end",
          title: {
            string_id: "callout-firefox-view-colorways-title",
          },
          subtitle: {
            string_id: "callout-firefox-view-colorways-subtitle",
          },
          logo: {
            imageURL: "chrome://browser/content/callout-colorways.svg",
            darkModeImageURL:
              "chrome://browser/content/callout-colorways-dark.svg",
            height: "128px",
          },
          primary_button: {
            label: {
              string_id: "callout-primary-complete-button-label",
            },
            action: {
              type: "SET_PREF",
              data: {
                pref: {
                  name: "browser.firefox-view.feature-tour",
                  value: JSON.stringify({
                    message: "FIREFOX_VIEW_FEATURE_TOUR",
                    screen: "",
                    complete: true,
                  }),
                },
              },
            },
          },
          dismiss_button: {
            action: {
              type: "SET_PREF",
              data: {
                pref: {
                  name: "browser.firefox-view.feature-tour",
                  value: JSON.stringify({
                    message: "FIREFOX_VIEW_FEATURE_TOUR",
                    screen: "FEATURE_CALLOUT_3",
                    complete: true,
                  }),
                },
              },
            },
          },
        },
      },
    ],
  },
];

function _createContainer() {
  let container = document.createElement("div");
  container.classList.add("onboardingContainer", "featureCallout", "hidden");
  container.id = CONTAINER_ID;
  let parent = document.querySelector(CURRENT_SCREEN?.parent_selector);
  container.setAttribute("aria-describedby", `#${CONTAINER_ID} .welcome-text`);
  container.tabIndex = 0;
  parent.insertAdjacentElement("afterend", container);
  return container;
}

/**
 * Set callout's position relative to parent element
 */
function _positionCallout() {
  const positions = ["top", "bottom", "left", "right"];
  const container = document.getElementById(CONTAINER_ID);
  const parentEl = document.querySelector(CURRENT_SCREEN?.parent_selector);
  const arrowPosition = CURRENT_SCREEN?.content?.arrow_position || "top";
  const margin = 15;
  // Is the document layout right to left?
  const RTL = document.dir === "rtl";

  if (!container || !parentEl) {
    return;
  }

  function getOffset(el) {
    const rect = el.getBoundingClientRect();
    return {
      left: rect.left + window.scrollX,
      right: rect.right + window.scrollX,
      top: rect.top + window.scrollY,
      bottom: rect.bottom + window.scrollY,
    };
  }

  function clearPosition() {
    positions.forEach(position => {
      container.style[position] = "unset";
      if (container.classList.contains(`arrow-${position}`)) {
        container.classList.remove(`arrow-${position}`);
      }
      if (container.classList.contains(`arrow-inline-${position}`)) {
        container.classList.remove(`arrow-inline-${position}`);
      }
    });
  }

  function centerHorizontally() {
    let sideOffset = (parentEl.offsetWidth - container.offsetWidth) / 2;
    let containerSide = RTL
      ? window.innerWidth - getOffset(parentEl).right + sideOffset
      : getOffset(parentEl).left + sideOffset;
    container.style[RTL ? "right" : "left"] = `${Math.max(
      containerSide,
      margin
    )}px`;
  }

  // Position callout relative to a parent element
  const positioners = {
    top() {
      let containerTop = getOffset(parentEl).bottom - margin;
      container.style.top = `${Math.min(
        window.innerHeight - container.offsetHeight - margin,
        containerTop
      )}px`;
      centerHorizontally(container, parentEl);
      container.classList.add("arrow-top");
    },
    // Point to an element below the callout
    bottom() {
      let containerTop =
        getOffset(parentEl).top - container.clientHeight + margin;
      container.style.top = `${Math.max(containerTop, 0)}px`;
      centerHorizontally(container, parentEl);
      container.classList.add("arrow-bottom");
    },
    // Point to an element to the right of the callout
    left() {
      let containerLeft = getOffset(parentEl).right - margin;
      container.style.left = `${Math.min(
        window.innerWidth - container.offsetWidth - margin,
        containerLeft
      )}px`;
      container.style.top = `${getOffset(parentEl).top}px`;
      container.classList.add("arrow-inline-start");
    },
    // Point to an element to the left of the callout
    right() {
      let containerLeft =
        getOffset(parentEl).left - container.offsetWidth + margin;
      container.style.left = `${Math.max(containerLeft, margin)}px`;
      container.style.top = `${getOffset(parentEl).top}px`;
      container.classList.add("arrow-inline-end");
    },
  };

  clearPosition(container);

  if (!container.classList.contains("callout-arrow")) {
    container.classList.add("callout-arrow");
  }

  if (["start", "end"].includes(arrowPosition)) {
    if (RTL) {
      positioners[arrowPosition === "start" ? "right" : "left"]();
    } else {
      positioners[arrowPosition === "start" ? "left" : "right"]();
    }
  } else {
    positioners[arrowPosition]();
  }

  container.classList.remove("hidden");
}

function _addPositionListeners() {
  window.addEventListener("scroll", _positionCallout);
  window.addEventListener("resize", _positionCallout);
}

function _removePositionListeners() {
  window.removeEventListener("scroll", _positionCallout);
  window.removeEventListener("resize", _positionCallout);
}

function _setupWindowFunctions() {
  const AWParent = new lazy.AboutWelcomeParent();
  const receive = name => data =>
    AWParent.onContentMessage(`AWPage:${name}`, data, document);
  // Expose top level functions expected by the bundle.
  window.AWGetDefaultSites = () => {};
  window.AWGetFeatureConfig = () => CONFIG;
  window.AWGetFxAMetricsFlowURI = () => {};
  window.AWGetImportableSites = () => "[]";
  window.AWGetRegion = receive("GET_REGION");
  window.AWGetSelectedTheme = receive("GET_SELECTED_THEME");
  // Do not send telemetry if message config sets metrics as 'block'.
  window.AWSendEventTelemetry =
    CONFIG?.metrics === "block" ? () => {} : receive("TELEMETRY_EVENT");
  window.AWSendToDeviceEmailsSupported = receive(
    "SEND_TO_DEVICE_EMAILS_SUPPORTED"
  );
  window.AWSendToParent = (name, data) => receive(name)(data);
  window.AWFinish = _endTour;
}

function _endTour() {
  // wait for fade out transition
  let container = document.getElementById(CONTAINER_ID);
  container?.classList.add("hidden");
  setTimeout(() => {
    container?.remove();
    _removePositionListeners();
    RENDER_OBSERVER?.disconnect();
  }, TRANSITION_MS);
}

async function _addScriptsAndRender(container) {
  // Add React script
  async function getReactReady() {
    return new Promise(function(resolve) {
      let reactScript = document.createElement("script");
      reactScript.src = "resource://activity-stream/vendor/react.js";
      container.appendChild(reactScript);
      reactScript.addEventListener("load", resolve);
    });
  }
  // Add ReactDom script
  async function getDomReady() {
    return new Promise(function(resolve) {
      let domScript = document.createElement("script");
      domScript.src = "resource://activity-stream/vendor/react-dom.js";
      container.appendChild(domScript);
      domScript.addEventListener("load", resolve);
    });
  }
  // Load React, then React Dom
  await getReactReady();
  await getDomReady();
  // Load the bundle to render the content as configured.
  let bundleScript = document.createElement("script");
  bundleScript.src =
    "resource://activity-stream/aboutwelcome/aboutwelcome.bundle.js";
  container.appendChild(bundleScript);
}

function _observeRender(container) {
  RENDER_OBSERVER?.observe(container, { childList: true });
}

function _loadConfig(messageId) {
  // If the parent element a screen describes doesn't exist, remove screen
  // and ensure last screen displays the final primary CTA
  // (for example, when there are no active colorways in about:firefoxview)
  function _getRelevantScreens(screens) {
    const finalCTA = screens[screens.length - 1].content.primary_button;
    screens = screens.filter((s, i) => {
      return document.querySelector(s.parent_selector);
    });
    screens[screens.length - 1].content.primary_button = finalCTA;
    return screens;
  }

  let content = MESSAGES.find(m => m.id === messageId);
  const screenId = lazy.featureTourProgress.screen;
  let screenIndex;
  if (content?.screens && screenId) {
    content.screens = _getRelevantScreens(content.screens);
    screenIndex = content.screens.findIndex(s => s.id === screenId);
    content.startScreen = screenIndex;
  }
  CURRENT_SCREEN = content?.screens?.[screenIndex || 0];
  CONFIG = content;
}

async function _renderCallout() {
  let container = _createContainer();
  // This results in rendering the Feature Callout
  await _addScriptsAndRender(container);
  _observeRender(container);
}
/**
 * Render content based on about:welcome multistage template.
 */
async function showFeatureCallout(messageId) {
  // Don't show the feature tour if user has already completed it.
  if (lazy.featureTourProgress.complete) {
    return;
  }

  _loadConfig(messageId);

  if (!CONFIG) {
    return;
  }

  RENDER_OBSERVER = new MutationObserver(function() {
    // Check if the Feature Callout screen has loaded for the first time
    if (!READY && document.querySelector(`#${CONTAINER_ID} .screen`)) {
      READY = true;
      _positionCallout();
      let container = document.getElementById(CONTAINER_ID);
      container.focus();
      // Alert screen readers to the presence of the callout
      container.setAttribute("role", "alert");
    }
  });

  _addCalloutLinkElements();
  // Add handlers for repositioning callout
  _addPositionListeners();
  _setupWindowFunctions();
  await _renderCallout();
}

window.addEventListener("DOMContentLoaded", () => {
  // Get the message id from the feature tour pref
  // (If/when this surface is used with other pages,
  // add logic to select the correct pref for a given
  // page's tour using its location)
  showFeatureCallout(lazy.featureTourProgress.message);
});

// When the window is focused, ensure tour is synced with tours in
// any other instances of the parent page
window.addEventListener("visibilitychange", () => {
  _handlePrefChange();
});
