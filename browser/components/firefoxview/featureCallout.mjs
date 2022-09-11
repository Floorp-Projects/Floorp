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
  ASRouter: "resource://activity-stream/lib/ASRouter.jsm",
});

// When expanding the use of Feature Callout
// to new about: pages, make `progressPref` a
// configurable field on callout messages and
// use it to determine which pref to observe
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "featureTourProgress",
  "browser.firefox-view.feature-tour",
  '{"message":"","screen":"","complete":true}',
  _handlePrefChange,
  val => JSON.parse(val)
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "cfrFeaturesUserPref",
  "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features",
  true,
  _handlePrefChange
);

/* Work around the pref callback being run after the document has been unlinked.
   See bug 1543537. */
const docWeak = Cu.getWeakReference(document);
async function _handlePrefChange() {
  const doc = docWeak.get();
  if (!doc || doc.visibilityState === "hidden") {
    return;
  }
  let prefVal = lazy.featureTourProgress;
  // End the tour according to the tour progress pref or if the user disabled
  // contextual feature recommendations.
  if (prefVal.complete || !lazy.cfrFeaturesUserPref) {
    _endTour();
    CURRENT_SCREEN = null;
  } else if (prefVal.screen !== CURRENT_SCREEN?.id) {
    READY = false;
    const container = doc.getElementById(CONTAINER_ID);
    container?.classList.add("hidden");
    // wait for fade out transition
    setTimeout(async () => {
      await _loadConfig();
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

function _createContainer() {
  let parent = document.querySelector(CURRENT_SCREEN?.parent_selector);
  // Don't render the callout if the parent element is not present.
  // This means the message was misconfigured, mistargeted, or the
  // content of the parent page is not as expected.
  if (!parent) {
    return false;
  }

  let container = document.createElement("div");
  container.classList.add(
    "onboardingContainer",
    "featureCallout",
    "callout-arrow",
    "hidden"
  );
  container.id = CONTAINER_ID;
  container.setAttribute("aria-describedby", `#${CONTAINER_ID} .welcome-text`);
  container.tabIndex = 0;
  parent.setAttribute("aria-owns", `${CONTAINER_ID}`);
  document.body.appendChild(container);
  return container;
}

/**
 * Set callout's position relative to parent element
 */
function _positionCallout() {
  const container = document.getElementById(CONTAINER_ID);
  const parentEl = document.querySelector(CURRENT_SCREEN?.parent_selector);
  // All possible arrow positions
  const arrowPositions = ["top", "bottom", "end", "start"];
  const arrowPosition = CURRENT_SCREEN?.content?.arrow_position || "top";
  // Callout should overlap the parent element by 17px (so the box, not
  // including the arrow, will overlap by 5px)
  const arrowWidth = 12;
  let overlap = 17;
  // If we have no overlap, we send the callout the same number of pixels
  // in the opposite direction
  overlap = CURRENT_SCREEN?.content?.noCalloutOverlap ? overlap * -1 : overlap;
  overlap -= arrowWidth;
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
    Object.keys(positioners).forEach(position => {
      container.style[position] = "unset";
    });
    arrowPositions.forEach(position => {
      if (container.classList.contains(`arrow-${position}`)) {
        container.classList.remove(`arrow-${position}`);
      }
      if (container.classList.contains(`arrow-inline-${position}`)) {
        container.classList.remove(`arrow-inline-${position}`);
      }
    });
  }

  const positioners = {
    // availableSpace should be the space between the edge of the page in the assumed direction
    // and the edge of the parent (with the callout being intended to fit between those two edges)
    // while needed space should be the space necessary to fit the callout container
    top: {
      availableSpace:
        document.body.offsetHeight -
        getOffset(parentEl).top -
        parentEl.offsetHeight +
        overlap,
      neededSpace: container.offsetHeight - overlap,
      position() {
        // Point to an element above the callout
        let containerTop =
          getOffset(parentEl).top + parentEl.offsetHeight - overlap;
        container.style.top = `${Math.max(
          container.offsetHeight - overlap,
          containerTop
        )}px`;
        centerHorizontally(container, parentEl);
        container.classList.add("arrow-top");
      },
    },
    bottom: {
      availableSpace: getOffset(parentEl).top + overlap,
      neededSpace: container.offsetHeight - overlap,
      position() {
        // Point to an element below the callout
        let containerTop =
          getOffset(parentEl).top - container.offsetHeight + overlap;
        container.style.top = `${Math.max(0, containerTop)}px`;
        centerHorizontally(container, parentEl);
        container.classList.add("arrow-bottom");
      },
    },
    right: {
      availableSpace: getOffset(parentEl).left + overlap,
      neededSpace: container.offsetWidth - overlap,
      position() {
        // Point to an element to the right of the callout
        let containerLeft =
          getOffset(parentEl).left - container.offsetWidth + overlap;
        if (RTL) {
          // Account for cases where the document body may be narrow than the window
          containerLeft -= window.innerWidth - document.body.offsetWidth;
        }
        container.style.left = `${Math.max(0, containerLeft)}px`;
        container.style.top = `${getOffset(parentEl).top}px`;
        container.classList.add("arrow-inline-end");
      },
    },
    left: {
      availableSpace:
        document.body.offsetWidth - getOffset(parentEl).right + overlap,
      neededSpace: container.offsetWidth - overlap,
      position() {
        // Point to an element to the left of the callout
        let containerLeft =
          getOffset(parentEl).left + parentEl.offsetWidth - overlap;
        if (RTL) {
          // Account for cases where the document body may be narrow than the window
          containerLeft -= window.innerWidth - document.body.offsetWidth;
        }
        container.style.left = `${(container.offsetWidth - overlap,
        containerLeft)}px`;
        container.style.top = `${getOffset(parentEl).top}px`;
        container.classList.add("arrow-inline-start");
      },
    },
  };

  function calloutFits(position) {
    // Does callout element fit in this position relative
    // to the parent element without going off screen?
    return (
      positioners[position].availableSpace > positioners[position].neededSpace
    );
  }

  function choosePosition() {
    let position = arrowPosition;
    if (!arrowPositions.includes(position)) {
      // Configured arrow position is not valid
      return false;
    }
    if (["start", "end"].includes(position)) {
      // position here is referencing the direction that the callout container
      // is pointing to, and therefore should be the _opposite_ side of the arrow
      // eg. if arrow is at the "end" in LTR layouts, the container is pointing
      // at an element to the right of itself, while in RTL layouts it is pointing to the left of itself
      position = RTL ^ (position === "start") ? "left" : "right";
    }
    if (calloutFits(position)) {
      return position;
    }
    let newPosition = Object.keys(positioners)
      .filter(p => p !== position)
      .find(p => calloutFits(p));
    // If the callout doesn't fit in any position, use the configured one.
    // The callout will be adjusted to overlap the parent element so that
    // the former doesn't go off screen.
    return newPosition || position;
  }

  function centerHorizontally() {
    let sideOffset = (parentEl.offsetWidth - container.offsetWidth) / 2;
    let containerSide = RTL
      ? window.innerWidth - getOffset(parentEl).right + sideOffset
      : getOffset(parentEl).left + sideOffset;
    container.style[RTL ? "right" : "left"] = `${Math.max(containerSide, 0)}px`;
  }

  clearPosition(container);

  let finalPosition = choosePosition();
  if (finalPosition) {
    positioners[finalPosition].position();
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
  addEventListener("unload", () => {
    AWParent.didDestroy();
  });
  const receive = name => data =>
    AWParent.onContentMessage(`AWPage:${name}`, data, document);
  // Expose top level functions expected by the bundle.
  window.AWGetFeatureConfig = () => CONFIG;
  window.AWGetRegion = receive("GET_REGION");
  window.AWGetSelectedTheme = receive("GET_SELECTED_THEME");
  // Do not send telemetry if message config sets metrics as 'block'.
  if (CONFIG?.metrics !== "block") {
    window.AWSendEventTelemetry = receive("TELEMETRY_EVENT");
  }
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

async function _loadConfig() {
  await lazy.ASRouter.waitForInitialized;
  let result = await lazy.ASRouter.sendTriggerMessage({
    // triggerId and triggerContext
    id: "featureCalloutCheck",
    context: { source: document.location.pathname.toLowerCase() },
  });
  CONFIG = result.message.content;

  // Only add an impression if we actually have a message to impress
  if (Object.keys(result.message).length) {
    lazy.ASRouter.addImpression(result.message);
  }

  CURRENT_SCREEN = CONFIG?.screens?.[CONFIG?.startScreen || 0];
}

async function _renderCallout() {
  let container = _createContainer();
  if (container) {
    // This results in rendering the Feature Callout
    await _addScriptsAndRender(container);
    _observeRender(container);
  }
}
/**
 * Render content based on about:welcome multistage template.
 */
async function showFeatureCallout(messageId) {
  await _loadConfig();

  if (!CONFIG?.screens?.length) {
    return;
  }

  RENDER_OBSERVER = new MutationObserver(function() {
    // Check if the Feature Callout screen has loaded for the first time
    if (!READY && document.querySelector(`#${CONTAINER_ID} .screen`)) {
      // Once the screen element is added to the DOM, wait for the
      // animation frame after next to ensure that _positionCallout
      // has access to the rendered screen with the correct height
      requestAnimationFrame(() => {
        requestAnimationFrame(() => {
          READY = true;
          _positionCallout();
          let container = document.getElementById(CONTAINER_ID);
          container.focus();
          // Alert screen readers to the presence of the callout
          container.setAttribute("role", "alert");
        });
      });
    }
  });

  _addCalloutLinkElements();
  // Add handlers for repositioning callout
  _addPositionListeners();
  _setupWindowFunctions();

  // If user has disabled CFR, don't show any callouts. But make sure we load
  // the necessary stylesheets first, since re-enabling CFR should allow
  // callouts to be shown without needing to reload. In the future this could
  // allow adding a CTA to disable recommendations with a label like "Don't show
  // these again" (or potentially a toggle to re-enable them).
  if (!lazy.cfrFeaturesUserPref) {
    CURRENT_SCREEN = null;
    return;
  }

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
  // If we have more than one screen, it means that we're
  // displaying a feature tour, in which transitions are handled
  // by the pref change observer.
  if (CONFIG?.screens.length > 1) {
    _handlePrefChange();
  } else {
    showFeatureCallout();
  }
});
