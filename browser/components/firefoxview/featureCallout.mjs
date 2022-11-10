/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  AboutWelcomeParent: "resource:///actors/AboutWelcomeParent.jsm",
  ASRouter: "resource://activity-stream/lib/ASRouter.jsm",
  PageEventManager: "resource://activity-stream/lib/PageEventManager.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "pageEventManager", () => {
  window.pageEventManager = new lazy.PageEventManager(document);
  return window.pageEventManager;
});

// When expanding the use of Feature Callout
// to new about: pages, make `progressPref` a
// configurable field on callout messages and
// use it to determine which pref to observe
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "featureTourProgress",
  "browser.firefox-view.feature-tour",
  '{"screen":"","complete":true}',
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

async function _handlePrefChange() {
  if (document.visibilityState === "hidden") {
    return;
  }

  // If we have more than one screen, it means that we're
  // displaying a feature tour, and transitions are handled
  // based on the value of a tour progress pref. Otherwise,
  // just show the feature callout.
  if (CONFIG?.screens.length === 1) {
    showFeatureCallout();
    return;
  }

  // If a pref change results from an event in a Spotlight message,
  // reload the page to clear the Spotlight and initialize the
  // feature callout with the next message in the tour.
  if (CURRENT_SCREEN == "spotlight") {
    location.reload();
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
    const container = document.getElementById(CONTAINER_ID);
    container?.classList.add("hidden");
    window.pageEventManager?.clear();
    // wait for fade out transition
    setTimeout(async () => {
      await _loadConfig();
      container?.remove();
      _removePositionListeners();
      await _renderCallout();
    }, TRANSITION_MS);
  }
}

function _addCalloutLinkElements() {
  function addStylesheet(href) {
    if (document.querySelector(`link[href="${href}"]`)) {
      return;
    }
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
let LISTENERS_REGISTERED = false;
let AWSetup = false;
let SAVED_ACTIVE_ELEMENT;
let LOADING_CONFIG = false;

const TRANSITION_MS = 500;
const CONTAINER_ID = "root";

function _createContainer() {
  let parent = document.querySelector(CURRENT_SCREEN?.parent_selector);
  // Don't render the callout if the parent element is not present.
  // This means the message was misconfigured, mistargeted, or the
  // content of the parent page is not as expected.
  if (!parent && !CURRENT_SCREEN?.content.callout_position_override) {
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
  document.body.prepend(container);
  return container;
}

/**
 * Set callout's position relative to parent element
 */
function _positionCallout() {
  const container = document.getElementById(CONTAINER_ID);
  const parentEl = document.querySelector(CURRENT_SCREEN?.parent_selector);
  // All possible arrow positions
  // If the position contains a dash, the value before the dash
  // refers to which edge of the feature callout the arrow points
  // from. The value after the dash describes where along that edge
  // the arrow sits, with middle as the default.
  const arrowPositions = [
    "top",
    "bottom",
    "end",
    "start",
    "top-end",
    "top-start",
  ];
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
  const customPosition = CURRENT_SCREEN?.content.callout_position_override;

  // Early exit if the container doesn't exist,
  // or if we're missing a parent element and don't have a custom callout position
  if (!container || (!parentEl && !customPosition)) {
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

  function addArrowPositionClassToContainer(finalArrowPosition) {
    let className;
    switch (finalArrowPosition) {
      case "bottom":
        className = "arrow-bottom";
        break;
      case "left":
        className = "arrow-inline-start";
        break;
      case "right":
        className = "arrow-inline-end";
        break;
      case "top-start":
        className = RTL ? "arrow-top-end" : "arrow-top-start";
        break;
      case "top-end":
        className = RTL ? "arrow-top-start" : "arrow-top-end";
        break;
      case "top":
      default:
        className = "arrow-top";
        break;
    }

    container.classList.add(className);
  }

  function overridePosition() {
    // We override _every_ positioner here, because we want to manually set all
    // container.style.positions in every positioner's "position" function
    // regardless of the actual arrow position
    for (const position in positioners) {
      positioners[position].position = function() {
        if (customPosition.top) {
          container.style.top = customPosition.top;
        }

        if (customPosition.left) {
          container.style.left = customPosition.left;
        }

        if (customPosition.right) {
          container.style.right = customPosition.right;
        }

        if (customPosition.bottom) {
          container.style.bottom = customPosition.bottom;
        }
      };
    }
  }

  const positioners = {
    // availableSpace should be the space between the edge of the page in the assumed direction
    // and the edge of the parent (with the callout being intended to fit between those two edges)
    // while needed space should be the space necessary to fit the callout container
    top: {
      availableSpace() {
        return (
          document.documentElement.clientHeight -
          getOffset(parentEl).top -
          parentEl.clientHeight
        );
      },
      neededSpace: container.clientHeight - overlap,
      position() {
        // Point to an element above the callout
        let containerTop =
          getOffset(parentEl).top + parentEl.clientHeight - overlap;
        container.style.top = `${Math.max(0, containerTop)}px`;
        centerHorizontally(container, parentEl);
      },
    },
    bottom: {
      availableSpace() {
        return getOffset(parentEl).top;
      },
      neededSpace: container.clientHeight - overlap,
      position() {
        // Point to an element below the callout
        let containerTop =
          getOffset(parentEl).top - container.clientHeight + overlap;
        container.style.top = `${Math.max(0, containerTop)}px`;
        centerHorizontally(container, parentEl);
      },
    },
    right: {
      availableSpace() {
        return getOffset(parentEl).left;
      },
      neededSpace: container.clientWidth - overlap,
      position() {
        // Point to an element to the right of the callout
        let containerLeft =
          getOffset(parentEl).left - container.clientWidth + overlap;
        container.style.left = `${Math.max(0, containerLeft)}px`;
        if (container.offsetHeight <= parentEl.offsetHeight) {
          container.style.top = `${getOffset(parentEl).top}px`;
        } else {
          centerVertically(container, parentEl);
        }
        container.classList.add("arrow-inline-end");
      },
    },
    left: {
      availableSpace() {
        return document.documentElement.clientWidth - getOffset(parentEl).right;
      },
      neededSpace: container.clientWidth - overlap,
      position() {
        // Point to an element to the left of the callout
        let containerLeft =
          getOffset(parentEl).left + parentEl.clientWidth - overlap;
        container.style.left = `${Math.max(0, containerLeft)}px`;
        if (container.offsetHeight <= parentEl.offsetHeight) {
          container.style.top = `${getOffset(parentEl).top}px`;
        } else {
          centerVertically(container, parentEl);
        }
        container.classList.add("arrow-inline-start");
      },
    },
    "top-end": {
      availableSpace() {
        document.documentElement.clientHeight -
          getOffset(parentEl).top -
          parentEl.clientHeight;
      },
      neededSpace: container.clientHeight - overlap,
      position() {
        // Point to an element above and at the end of the callout
        let containerTop =
          getOffset(parentEl).top + parentEl.clientHeight - overlap;
        container.style.top = `${Math.max(
          container.clientHeight - overlap,
          containerTop
        )}px`;
        alignEnd(container, parentEl);
      },
    },
  };

  function calloutFits(position) {
    // Does callout element fit in this position relative
    // to the parent element without going off screen?

    // Only consider which edge of the callout the arrow points from,
    // not the alignment of the arrow along the edge of the callout
    let edgePosition = position.split("-")[0];
    return (
      positioners[edgePosition].availableSpace() >
      positioners[edgePosition].neededSpace
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
    // If we're overriding the position, we don't need to sort for available space
    if (customPosition || calloutFits(position)) {
      return position;
    }
    let sortedPositions = Object.keys(positioners)
      .filter(p => p !== position)
      .filter(calloutFits)
      .sort((a, b) => {
        return (
          positioners[b].availableSpace() - positioners[b].neededSpace >
          positioners[a].availableSpace() - positioners[a].neededSpace
        );
      });
    // If the callout doesn't fit in any position, use the configured one.
    // The callout will be adjusted to overlap the parent element so that
    // the former doesn't go off screen.
    return sortedPositions[0] || position;
  }

  function centerHorizontally() {
    let sideOffset = (parentEl.clientWidth - container.clientWidth) / 2;
    let containerSide = RTL
      ? document.documentElement.clientWidth -
        getOffset(parentEl).right +
        sideOffset
      : getOffset(parentEl).left + sideOffset;
    container.style[RTL ? "right" : "left"] = `${Math.max(containerSide, 0)}px`;
  }

  function alignEnd() {
    let containerSide = RTL
      ? parentEl.getBoundingClientRect().left
      : parentEl.getBoundingClientRect().left +
        parentEl.clientWidth -
        container.clientWidth;
    container.style.left = `${Math.max(containerSide, 0)}px`;
  }

  function centerVertically() {
    let topOffset = (container.offsetHeight - parentEl.offsetHeight) / 2;
    container.style.top = `${getOffset(parentEl).top - topOffset}px`;
  }
  clearPosition(container);

  if (customPosition) {
    // We override the position functions with new functions here,
    // but they don't actually get executed in the override function
    overridePosition();
  }

  let finalPosition = choosePosition();
  if (finalPosition) {
    positioners[finalPosition].position();
    addArrowPositionClassToContainer(finalPosition);
  }

  container.classList.remove("hidden");
}

function _addPositionListeners() {
  if (!LISTENERS_REGISTERED) {
    window.addEventListener("resize", _positionCallout);
    const parentEl = document.querySelector(CURRENT_SCREEN?.parent_selector);
    parentEl?.addEventListener("toggle", _positionCallout);
    LISTENERS_REGISTERED = true;
  }
}

function _removePositionListeners() {
  if (LISTENERS_REGISTERED) {
    window.removeEventListener("resize", _positionCallout);
    const parentEl = document.querySelector(CURRENT_SCREEN?.parent_selector);
    parentEl?.removeEventListener("toggle", _positionCallout);
    LISTENERS_REGISTERED = false;
  }
}

function _setupWindowFunctions() {
  if (AWSetup) {
    return;
  }
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
  AWSetup = true;
}

function _endTour() {
  // We don't want focus events that happen during teardown to effect
  // SAVED_ACTIVE_ELEMENT
  window.removeEventListener("focus", focusHandler, { capture: true });
  window.pageEventManager?.clear();

  READY = false;
  // wait for fade out transition
  let container = document.getElementById(CONTAINER_ID);
  container?.classList.add("hidden");
  setTimeout(() => {
    container?.remove();
    RENDER_OBSERVER?.disconnect();

    // Put the focus back to the last place the user focused outside of the
    // featureCallout windows.
    if (SAVED_ACTIVE_ELEMENT) {
      SAVED_ACTIVE_ELEMENT.focus({ focusVisible: true });
    }
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
  if (LOADING_CONFIG) {
    return false;
  }
  LOADING_CONFIG = true;
  await lazy.ASRouter.waitForInitialized;
  let result = await lazy.ASRouter.sendTriggerMessage({
    browser: window.docShell.chromeEventHandler,
    // triggerId and triggerContext
    id: "featureCalloutCheck",
    context: { source: document.location.pathname.toLowerCase() },
  });
  LOADING_CONFIG = false;

  if (result.message.template !== "feature_callout") {
    CURRENT_SCREEN = result.message.template;
    return false;
  }

  CONFIG = result.message.content;

  let newScreen = CONFIG?.screens?.[CONFIG?.startScreen || 0];

  if (newScreen === CURRENT_SCREEN) {
    return false;
  }

  // Only add an impression if we actually have a message to impress
  if (Object.keys(result.message).length) {
    lazy.ASRouter.addImpression(result.message);
  }

  CURRENT_SCREEN = newScreen;
  return true;
}

async function _renderCallout() {
  let container = _createContainer();
  if (container) {
    // This results in rendering the Feature Callout
    await _addScriptsAndRender(container);
    _observeRender(container);
    _addPositionListeners();
  }
}
/**
 * Render content based on about:welcome multistage template.
 */
async function showFeatureCallout(messageId) {
  let updated = await _loadConfig();

  if (!updated || !CONFIG?.screens?.length) {
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
          _attachPageEventListeners(
            CURRENT_SCREEN?.content?.page_event_listeners
          );
          _positionCallout();
          let container = document.getElementById(CONTAINER_ID);
          container.focus();
          window.addEventListener("focus", focusHandler, {
            capture: true, // get the event before retargeting
          });
        });
      });
    }
  });

  _addCalloutLinkElements();
  // Add handlers for repositioning callout
  _setupWindowFunctions();

  window.pageEventManager?.clear();
  READY = false;
  const container = document.getElementById(CONTAINER_ID);
  container?.remove();

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

function focusHandler(e) {
  let container = document.getElementById(CONTAINER_ID);
  if (!container) {
    return;
  }

  // If focus has fired on the feature callout window itself, or on something
  // contained in that window, ignore it, as we can't possibly place the focus
  // on it after the callout is closd.
  if (
    e.target.id === CONTAINER_ID ||
    (Node.isInstance(e.target) && container.contains(e.target))
  ) {
    return;
  }

  // Save this so that if the next focus event is re-entering the popup,
  // then we'll put the focus back here where the user left it once we exit
  // the feature callout series.
  SAVED_ACTIVE_ELEMENT = document.activeElement;
}

/**
 * For each member of the screen's page_event_listeners array, add a listener.
 * @param {Array<PageEventListener>} listeners An array of listeners to set up
 *
 * @typedef {Object} PageEventListener
 * @property {PageEventListenerParams} params Event listener parameters
 * @property {PageEventListenerAction} action Sent when the event fires
 *
 * @typedef {Object} PageEventListenerParams See PageEventManager.jsm
 * @property {String} type Event type string e.g. `click`
 * @property {String} selectors Target selector, e.g. `tag.class, #id[attr]`
 * @property {PageEventListenerOptions} [options] addEventListener options
 *
 * @typedef {Object} PageEventListenerOptions
 * @property {Boolean} [capture] Use event capturing phase?
 * @property {Boolean} [once] Remove listener after first event?
 * @property {Boolean} [preventDefault] Prevent default action?
 *
 * @typedef {Object} PageEventListenerAction Action sent to AboutWelcomeParent
 * @property {String} [type] Action type, e.g. `OPEN_URL`
 * @property {Object} [data] Extra data, properties depend on action type
 * @property {Boolean} [dismiss] Dismiss screen after performing action?
 */
function _attachPageEventListeners(listeners) {
  listeners?.forEach(({ params, action }) =>
    lazy.pageEventManager[params.options?.once ? "once" : "on"](
      params,
      event => {
        _handlePageEventAction(action, event);
        if (params.options?.preventDefault) {
          event.preventDefault?.();
        }
      }
    )
  );
}

/**
 * Perform an action in response to a page event.
 * @param {PageEventListenerAction} action
 * @param {Event} event Triggering event
 */
function _handlePageEventAction(action, event) {
  window.AWSendEventTelemetry?.({
    event: "PAGE_EVENT",
    event_context: {
      action: action.type ?? (action.dismiss ? "DISMISS" : ""),
      reason: event.type?.toUpperCase(),
      source: _getUniqueElementIdentifier(event.target),
      page: document.location.href,
    },
    message_id: CONFIG?.id.toUpperCase(),
  });
  if (action.type) {
    window.AWSendToParent("SPECIAL_ACTION", action);
  }
  if (action.dismiss) {
    _endTour();
  }
}

/**
 * For a given element, calculate a unique string that identifies it.
 * @param {Element} target Element to calculate the selector for
 * @returns {String} Computed event target selector, e.g. `button#next`
 */
function _getUniqueElementIdentifier(target) {
  let source;
  if (Element.isInstance(target)) {
    source = target.localName;
    if (target.className) {
      source += `.${[...target.classList].join(".")}`;
    }
    if (target.id) {
      source += `#${target.id}`;
    }
    if (target.attributes.length) {
      source += `${[...target.attributes]
        .filter(attr => ["is", "role", "open"].includes(attr.name))
        .map(attr => `[${attr.name}="${attr.value}"]`)
        .join("")}`;
    }
    if (document.querySelectorAll(source).length > 1) {
      let uniqueAncestor = target.closest(`[id]:not(:scope, :root, body)`);
      if (uniqueAncestor) {
        source = `${_getUniqueElementIdentifier(uniqueAncestor)} > ${source}`;
      }
    }
  }
  return source;
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
