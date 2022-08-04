/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
  null,
  val => JSON.parse(val)
);

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
  ]);
}

let CURRENT_SCREEN;
let POSITION_HANDLER;
let CONFIG;

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
          title: "Hop between devices with tab pickup",
          subtitle:
            "Quickly grab open tabs from your phone and open them here for maximum flow.",
          logo: {
            imageURL:
              "chrome://activity-stream/content/data/content/assets/heart.webp",
          },
          primary_button: {
            label: "Next",
            action: {
              navigate: true,
            },
          },
          dismiss_button: {
            action: {
              navigate: true,
            },
          },
        },
      },
      {
        id: "FEATURE_CALLOUT_2",
        parent_selector: "#recently-closed-tabs-container",
        content: {
          position: "callout",
          title: "Get back your closed tabs in a snap",
          subtitle:
            "All your closed tabs will magically show up here. Never worry about accidentally closing a site again.",
          primary_button: {
            label: "Next",
            action: {
              navigate: true,
            },
          },
          dismiss_button: {
            action: {
              navigate: true,
            },
          },
        },
      },
      {
        id: "FEATURE_CALLOUT_3",
        parent_selector: "#colorways",
        content: {
          position: "callout",
          title: "Add a splash of color",
          subtitle:
            "Paint your browser with colorways, only in Firefox. Choose the shade that speaks to you.",
          logo: {
            imageURL:
              "chrome://activity-stream/content/data/content/assets/heart.webp",
          },
          primary_button: {
            label: "Finish",
            action: {
              navigate: true,
            },
          },
          dismiss_button: {
            action: {
              navigate: true,
            },
          },
        },
      },
    ],
  },
];

function _createContainer() {
  let container = document.createElement("div");
  container.classList.add("onboardingContainer", "featureCallout");
  container.id = CONTAINER_ID;
  document.body.appendChild(container);
  return container;
}

function _positionCallout() {
  let container = document.getElementById(CONTAINER_ID);
  let parentEl = document.querySelector(CURRENT_SCREEN?.parent_selector);

  if (!container || !parentEl) {
    return;
  }

  function getOffset(el) {
    const rect = el.getBoundingClientRect();
    return {
      left: rect.left + window.scrollX,
      bottom: rect.bottom + window.scrollY,
    };
  }
  // Set callout's position relative to parent element
  const margin = 15;
  let containerTop = getOffset(parentEl).bottom - margin;
  container.style.top = `${Math.min(
    window.innerHeight - container.offsetHeight - margin,
    containerTop
  )}px`;
  let leftOffset = (parentEl.offsetWidth - container.offsetWidth) / 2;
  let containerLeft = getOffset(parentEl).left + leftOffset;
  container.style.left = `${Math.min(
    window.innerWidth - container.offsetWidth - margin,
    Math.max(containerLeft, margin)
  )}px`;
}

function _addPositionListeners() {
  POSITION_HANDLER = () => {
    _positionCallout(CURRENT_SCREEN.parent_selector);
  };
  window.addEventListener("scroll", POSITION_HANDLER);
  window.addEventListener("resize", POSITION_HANDLER);
}

function _removePositionListeners() {
  if (POSITION_HANDLER) {
    window.removeEventListener("scroll", POSITION_HANDLER);
    window.removeEventListener("resize", POSITION_HANDLER);
  }
}

function _setupWindowFunctions() {
  const AWParent = new lazy.AboutWelcomeParent();
  const browser = document.chromeEventHandler;
  const receive = name => data =>
    AWParent.onContentMessage(`AWPage:${name}`, data, browser);
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
  window.AWNewScreen = screenIndex => {
    CURRENT_SCREEN = CONFIG.screens?.[screenIndex];
    if (CURRENT_SCREEN) {
      _positionCallout();
    }
  };
  window.AWFinish = () => {
    document.getElementById(CONTAINER_ID).remove();
    _removePositionListeners();
  };
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

/**
 * Render content based on about:welcome multistage template.
 */
export async function showFeatureCallout(messageId) {
  // Don't show the feature tour if user has already completed it.
  if (lazy.featureTourProgress.complete) {
    return;
  }

  CONFIG = MESSAGES.find(m => m.id === messageId);
  if (!CONFIG) {
    return;
  }

  _addCalloutLinkElements();
  let container = _createContainer();
  _setupWindowFunctions();

  // This results in rendering the Feature Callout
  await _addScriptsAndRender(container);

  // Add handlers for repositioning callout
  _addPositionListeners();
}
