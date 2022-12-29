/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*eslint-env browser*/

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const TRANSITION_MS = 500;
const CONTAINER_ID = "root";

/**
 * Feature Callout fetches messages relevant to a given source and displays them in
 * the parent page pointing to the element they describe.
 * @param {Window} Window in which messages will be rendered
 * @param {String} Name of the pref used to track progress through a given feature tour
 * @param {String} Optional string to pass as the source when checking for messages to show, 
 * defaults to this.doc.location.pathname.toLowerCase().
 * @param {Browser} browser

 */
export class FeatureCallout {
  constructor({ win, prefName, source, browser }) {
    this.win = win || window;
    this.doc = win.document;
    this.browser = browser || this.win.docShell.chromeEventHandler;
    this.config = null;
    this.loadingConfig = false;
    this.currentScreen = null;
    this.renderObserver = null;
    this.savedActiveElement = null;
    this.ready = false;
    this.listenersRegistered = false;
    this.AWSetup = false;
    this.source = source || this.doc.location.pathname.toLowerCase();
    this.focusHandler = this._focusHandler.bind(this);

    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "featureTourProgress",
      prefName,
      '{"screen":"","complete":true}',
      this._handlePrefChange.bind(this),
      val => JSON.parse(val)
    );

    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "cfrFeaturesUserPref",
      "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features",
      true,
      function(pref, previous, latest) {
        if (latest) {
          this.showFeatureCallout();
        } else {
          this._handlePrefChange();
        }
      }.bind(this)
    );
    this.featureTourProgress; // Load initial value of progress pref

    XPCOMUtils.defineLazyModuleGetters(this, {
      AboutWelcomeParent: "resource:///actors/AboutWelcomeParent.jsm",
      ASRouter: "resource://activity-stream/lib/ASRouter.jsm",
      PageEventManager: "resource://activity-stream/lib/PageEventManager.jsm",
    });

    XPCOMUtils.defineLazyGetter(this, "pageEventManager", () => {
      this.win.pageEventManager = new this.PageEventManager(this.doc);
      return this.win.pageEventManager;
    });

    const inChrome =
      this.win.location.toString() === "chrome://browser/content/browser.xhtml";
    // When the window is focused, ensure tour is synced with tours in
    // any other instances of the parent page. This does not apply when
    // the Callout is shown in the browser chrome.
    if (!inChrome) {
      this.win.addEventListener(
        "visibilitychange",
        this._handlePrefChange.bind(this)
      );
    }

    const positionCallout = this._positionCallout.bind(this);

    this._addPositionListeners = () => {
      if (!this.listenersRegistered) {
        this.win.addEventListener("resize", positionCallout);
        const parentEl = this.doc.querySelector(
          this.currentScreen?.parent_selector
        );
        parentEl?.addEventListener("toggle", positionCallout);
        this.listenersRegistered = true;
      }
    };

    this._removePositionListeners = () => {
      if (this.listenersRegistered) {
        this.win.removeEventListener("resize", positionCallout);
        const parentEl = this.doc.querySelector(
          this.currentScreen?.parent_selector
        );
        parentEl?.removeEventListener("toggle", positionCallout);
        this.listenersRegistered = false;
      }
    };
  }

  async _handlePrefChange() {
    if (this.doc.visibilityState === "hidden" || !this.featureTourProgress) {
      return;
    }

    // If we have more than one screen, it means that we're
    // displaying a feature tour, and transitions are handled
    // based on the value of a tour progress pref. Otherwise,
    // just show the feature callout.
    if (this.config?.screens.length === 1) {
      this.showFeatureCallout();
      return;
    }

    // If a pref change results from an event in a Spotlight message,
    // reload the page to clear the Spotlight and initialize the
    // feature callout with the next message in the tour.
    if (this.currentScreen == "spotlight") {
      this.win.location.reload();
      return;
    }

    let prefVal = this.featureTourProgress;
    // End the tour according to the tour progress pref or if the user disabled
    // contextual feature recommendations.
    if (prefVal.complete || !this.cfrFeaturesUserPref) {
      this._endTour();
      this.currentScreen = null;
    } else if (prefVal.screen !== this.currentScreen?.id) {
      this.ready = false;
      const container = this.doc.getElementById(CONTAINER_ID);
      container?.classList.add("hidden");
      this.win.pageEventManager?.clear();
      // wait for fade out transition
      this.win.setTimeout(async () => {
        await this._loadConfig();
        container?.remove();
        this._removePositionListeners();
        await this._renderCallout();
      }, TRANSITION_MS);
    }
  }
  _addCalloutLinkElements() {
    const addStylesheet = href => {
      if (this.doc.querySelector(`link[href="${href}"]`)) {
        return;
      }
      const link = this.doc.head.appendChild(this.doc.createElement("link"));
      link.rel = "stylesheet";
      link.href = href;
    };
    const addLocalization = hrefs => {
      hrefs.forEach(href => {
        // eslint-disable-next-line no-undef
        this.win.MozXULElement.insertFTLIfNeeded(href);
      });
    };

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

  _createContainer() {
    let parent = this.doc.querySelector(this.currentScreen?.parent_selector);
    // Don't render the callout if the parent element is not present.
    // This means the message was misconfigured, mistargeted, or the
    // content of the parent page is not as expected.
    if (!parent && !this.currentScreen?.content.callout_position_override) {
      return false;
    }

    let container = this.doc.createElement("div");
    container.classList.add(
      "onboardingContainer",
      "featureCallout",
      "callout-arrow",
      "hidden"
    );
    container.id = CONTAINER_ID;
    container.setAttribute(
      "aria-describedby",
      `#${CONTAINER_ID} .welcome-text`
    );
    container.tabIndex = 0;
    this.doc.body.prepend(container);
    return container;
  }

  /**
   * Set callout's position relative to parent element
   */
  _positionCallout() {
    const container = this.doc.getElementById(CONTAINER_ID);
    const parentEl = this.doc.querySelector(
      this.currentScreen?.parent_selector
    );
    const doc = this.doc;
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
    const arrowPosition = this.currentScreen?.content?.arrow_position || "top";
    // Callout should overlap the parent element by 17px (so the box, not
    // including the arrow, will overlap by 5px)
    const arrowWidth = 12;
    let overlap = 17;
    // If we have no overlap, we send the callout the same number of pixels
    // in the opposite direction
    overlap = this.currentScreen?.content?.noCalloutOverlap
      ? overlap * -1
      : overlap;
    overlap -= arrowWidth;
    // Is the document layout right to left?
    const RTL = this.doc.dir === "rtl";
    const customPosition = this.currentScreen?.content
      .callout_position_override;

    // Early exit if the container doesn't exist,
    // or if we're missing a parent element and don't have a custom callout position
    if (!container || (!parentEl && !customPosition)) {
      return;
    }

    const getOffset = el => {
      const rect = el.getBoundingClientRect();
      return {
        left: rect.left + this.win.scrollX,
        right: rect.right + this.win.scrollX,
        top: rect.top + this.win.scrollY,
        bottom: rect.bottom + this.win.scrollY,
      };
    };

    const clearPosition = () => {
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
    };

    const addArrowPositionClassToContainer = finalArrowPosition => {
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
    };

    const overridePosition = () => {
      // We override _every_ positioner here, because we want to manually set all
      // container.style.positions in every positioner's "position" function
      // regardless of the actual arrow position
      for (const position in positioners) {
        positioners[position].position = () => {
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
    };

    const positioners = {
      // availableSpace should be the space between the edge of the page in the assumed direction
      // and the edge of the parent (with the callout being intended to fit between those two edges)
      // while needed space should be the space necessary to fit the callout container
      top: {
        availableSpace() {
          return (
            doc.documentElement.clientHeight -
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
          alignHorizontally("center");
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
          alignHorizontally("center");
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
            centerVertically();
          }
        },
      },
      left: {
        availableSpace() {
          return doc.documentElement.clientWidth - getOffset(parentEl).right;
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
            centerVertically();
          }
        },
      },
      "top-start": {
        availableSpace() {
          doc.documentElement.clientHeight -
            getOffset(parentEl).top -
            parentEl.clientHeight;
        },
        neededSpace: container.clientHeight - overlap,
        position() {
          // Point to an element above and at the start of the callout
          let containerTop =
            getOffset(parentEl).top + parentEl.clientHeight - overlap;
          container.style.top = `${Math.max(
            container.clientHeight - overlap,
            containerTop
          )}px`;
          alignHorizontally("start");
        },
      },
      "top-end": {
        availableSpace() {
          doc.documentElement.clientHeight -
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
          alignHorizontally("end");
        },
      },
    };

    const calloutFits = position => {
      // Does callout element fit in this position relative
      // to the parent element without going off screen?

      // Only consider which edge of the callout the arrow points from,
      // not the alignment of the arrow along the edge of the callout
      let edgePosition = position.split("-")[0];
      return (
        positioners[edgePosition].availableSpace() >
        positioners[edgePosition].neededSpace
      );
    };

    const choosePosition = () => {
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
    };

    const centerVertically = () => {
      let topOffset = (container.offsetHeight - parentEl.offsetHeight) / 2;
      container.style.top = `${getOffset(parentEl).top - topOffset}px`;
    };

    /**
     * Horizontally align a top/bottom-positioned callout according to the
     * passed position.
     * @param {string} [position = "start"] <"start"|"end"|"center">
     */
    const alignHorizontally = position => {
      switch (position) {
        case "center": {
          let sideOffset = (parentEl.clientWidth - container.clientWidth) / 2;
          let containerSide = RTL
            ? doc.documentElement.clientWidth -
              getOffset(parentEl).right +
              sideOffset
            : getOffset(parentEl).left + sideOffset;
          container.style[RTL ? "right" : "left"] = `${Math.max(
            containerSide,
            0
          )}px`;
          break;
        }
        default: {
          let containerSide =
            RTL ^ (position === "end")
              ? parentEl.getBoundingClientRect().left +
                parentEl.clientWidth -
                container.clientWidth
              : parentEl.getBoundingClientRect().left;
          container.style.left = `${Math.max(containerSide, 0)}px`;
          break;
        }
      }
    };

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

  _setupWindowFunctions() {
    if (this.AWSetup) {
      return;
    }
    const AWParent = new this.AboutWelcomeParent();
    this.win.addEventListener("unload", () => {
      AWParent.didDestroy();
    });
    const receive = name => data =>
      AWParent.onContentMessage(`AWPage:${name}`, data, this.doc);
    // Expose top level functions expected by the bundle.
    this.win.AWGetFeatureConfig = () => this.config;
    this.win.AWGetRegion = receive("GET_REGION");
    this.win.AWGetSelectedTheme = receive("GET_SELECTED_THEME");
    // Do not send telemetry if message config sets metrics as 'block'.
    if (this.config?.metrics !== "block") {
      this.win.AWSendEventTelemetry = receive("TELEMETRY_EVENT");
    }
    this.win.AWSendToDeviceEmailsSupported = receive(
      "SEND_TO_DEVICE_EMAILS_SUPPORTED"
    );
    this.win.AWSendToParent = (name, data) => receive(name)(data);
    this.win.AWFinish = () => {
      this._endTour();
    };
    this.AWSetup = true;
  }

  _clearWindowFunctions() {
    const windowFuncs = [
      "AWGetFeatureConfig",
      "AWGetRegion",
      "AWGetSelectedTheme",
      "AWSendEventTelemetry",
      "AWSendToDeviceEmailsSupported",
      "AWSendToParent",
      "AWFinish",
    ];
    windowFuncs.forEach(func => delete this.win[func]);
  }

  _endTour(skipFadeOut = false) {
    // We don't want focus events that happen during teardown to effect
    // this.savedActiveElement
    this.win.removeEventListener("focus", this.focusHandler, {
      capture: true,
    });
    this.win.pageEventManager?.clear();

    // We're deleting featureTourProgress here to ensure that the
    // reference is freed for garbage collection. This prevents errors
    // caused by lingering instances when instantiating and removing
    // multiple feature tour instances in succession.
    delete this.featureTourProgress;
    this.ready = false;
    // wait for fade out transition
    let container = this.doc.getElementById(CONTAINER_ID);
    container?.classList.add("hidden");
    this._clearWindowFunctions();
    this.win.setTimeout(
      () => {
        container?.remove();
        this.renderObserver?.disconnect();
        // Put the focus back to the last place the user focused outside of the
        // featureCallout windows.
        if (this.savedActiveElement) {
          this.savedActiveElement.focus({ focusVisible: true });
        }
      },
      skipFadeOut ? 0 : TRANSITION_MS
    );
  }

  async _addScriptsAndRender() {
    const reactSrc = "resource://activity-stream/vendor/react.js";
    const domSrc = "resource://activity-stream/vendor/react-dom.js";
    // Add React script
    const getReactReady = async () => {
      return new Promise(resolve => {
        let reactScript = this.doc.createElement("script");
        reactScript.src = reactSrc;
        this.doc.head.appendChild(reactScript);
        reactScript.addEventListener("load", resolve);
      });
    };
    // Add ReactDom script
    const getDomReady = async () => {
      return new Promise(resolve => {
        let domScript = this.doc.createElement("script");
        domScript.src = domSrc;
        this.doc.head.appendChild(domScript);
        domScript.addEventListener("load", resolve);
      });
    };
    // Load React, then React Dom
    if (!this.doc.querySelector(`[src="${reactSrc}"]`)) {
      await getReactReady();
    }
    if (!this.doc.querySelector(`[src="${domSrc}"]`)) {
      await getDomReady();
    }
    // Load the bundle to render the content as configured.
    let bundleScript = this.doc.createElement("script");
    bundleScript.src =
      "resource://activity-stream/aboutwelcome/aboutwelcome.bundle.js";
    this.doc.head.appendChild(bundleScript);
  }

  _observeRender(container) {
    this.renderObserver?.observe(container, { childList: true });
  }

  async _loadConfig() {
    if (this.loadingConfig) {
      return false;
    }
    this.loadingConfig = true;
    await this.ASRouter.waitForInitialized;
    let result = await this.ASRouter.sendTriggerMessage({
      browser: this.browser,
      // triggerId and triggerContext
      id: "featureCalloutCheck",
      context: { source: this.source },
    });
    this.loadingConfig = false;

    if (result.message.template !== "feature_callout") {
      this.currentScreen = result.message.template;
      return false;
    }

    this.config = result.message.content;

    let newScreen = this.config?.screens?.[this.config?.startScreen || 0];
    if (newScreen?.id === this.currentScreen?.id) {
      return false;
    }

    // Only add an impression if we actually have a message to impress
    if (Object.keys(result.message).length) {
      this.ASRouter.addImpression(result.message);
    }

    this.currentScreen = newScreen;
    return true;
  }

  async _renderCallout() {
    let container = this._createContainer();
    if (container) {
      // This results in rendering the Feature Callout
      await this._addScriptsAndRender();
      this._observeRender(container);
      this._addPositionListeners();
    }
  }

  _focusHandler(e) {
    let container = this.doc.getElementById(CONTAINER_ID);
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
    this.savedActiveElement = this.doc.activeElement;
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
  _attachPageEventListeners(listeners) {
    listeners?.forEach(({ params, action }) =>
      this.pageEventManager[params.options?.once ? "once" : "on"](
        params,
        event => {
          this._handlePageEventAction(action, event);
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
  _handlePageEventAction(action, event) {
    const page = this.doc.location.href;
    const message_id = this.config?.id.toUpperCase();
    const source = this._getUniqueElementIdentifier(event.target);
    this.win.AWSendEventTelemetry?.({
      event: "PAGE_EVENT",
      event_context: {
        action: action.type ?? (action.dismiss ? "DISMISS" : ""),
        reason: event.type?.toUpperCase(),
        source,
        page,
      },
      message_id,
    });
    if (action.type) {
      this.win.AWSendToParent("SPECIAL_ACTION", action);
    }
    if (action.dismiss) {
      this.win.AWSendEventTelemetry?.({
        event: "DISMISS",
        event_context: { source: `PAGE_EVENT:${source}`, page },
        message_id,
      });
      this._endTour();
    }
  }

  /**
   * For a given element, calculate a unique string that identifies it.
   * @param {Element} target Element to calculate the selector for
   * @returns {String} Computed event target selector, e.g. `button#next`
   */
  _getUniqueElementIdentifier(target) {
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
      if (this.doc.querySelectorAll(source).length > 1) {
        let uniqueAncestor = target.closest(`[id]:not(:scope, :root, body)`);
        if (uniqueAncestor) {
          source = `${this._getUniqueElementIdentifier(
            uniqueAncestor
          )} > ${source}`;
        }
      }
    }
    return source;
  }

  async showFeatureCallout() {
    let updated = await this._loadConfig();

    if (!updated || !this.config?.screens?.length) {
      return;
    }

    this.renderObserver = new this.win.MutationObserver(() => {
      // Check if the Feature Callout screen has loaded for the first time
      if (!this.ready && this.doc.querySelector(`#${CONTAINER_ID} .screen`)) {
        // Once the screen element is added to the DOM, wait for the
        // animation frame after next to ensure that _positionCallout
        // has access to the rendered screen with the correct height
        this.win.requestAnimationFrame(() => {
          this.win.requestAnimationFrame(() => {
            this.ready = true;
            this._attachPageEventListeners(
              this.currentScreen?.content?.page_event_listeners
            );
            this._positionCallout();
            let container = this.doc.getElementById(CONTAINER_ID);
            container.focus();
            this.win.addEventListener("focus", this.focusHandler, {
              capture: true, // get the event before retargeting
            });
          });
        });
      }
    });

    this.win.pageEventManager?.clear();
    this.ready = false;
    const container = this.doc.getElementById(CONTAINER_ID);
    container?.remove();

    // If user has disabled CFR, don't show any callouts. But make sure we load
    // the necessary stylesheets first, since re-enabling CFR should allow
    // callouts to be shown without needing to reload. In the future this could
    // allow adding a CTA to disable recommendations with a label like "Don't show
    // these again" (or potentially a toggle to re-enable them).
    if (!this.cfrFeaturesUserPref) {
      this.currentScreen = null;
      return;
    }

    this._addCalloutLinkElements();
    this._setupWindowFunctions();
    await this._renderCallout();
  }
}
