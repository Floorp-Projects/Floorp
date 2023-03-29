/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};
XPCOMUtils.defineLazyModuleGetters(lazy, {
  AboutWelcomeParent: "resource:///actors/AboutWelcomeParent.jsm",
  ASRouter: "resource://activity-stream/lib/ASRouter.jsm",
  PageEventManager: "resource://activity-stream/lib/PageEventManager.jsm",
});

const TRANSITION_MS = 500;
const CONTAINER_ID = "multi-stage-message-root";
const BUNDLE_SRC =
  "resource://activity-stream/aboutwelcome/aboutwelcome.bundle.js";

/**
 * Feature Callout fetches messages relevant to a given source and displays them
 * in the parent page pointing to the element they describe.
 */
export class FeatureCallout {
  /**
   * @typedef {Object} FeatureCalloutOptions
   * @property {Window} win window in which messages will be rendered
   * @property {String} prefName name of the pref used to track progress through
   *   a given feature tour, e.g. "browser.pdfjs.feature-tour"
   * @property {String} [page] string to pass as the page when requesting
   *   messages from ASRouter and sending telemetry. for browser chrome, the
   *   string "chrome" is used
   * @property {MozBrowser} [browser] <browser> element responsible for the
   *   feature callout. for content pages, this is the browser element that the
   *   callout is being shown in. for chrome, this is the active browser
   * @property {FeatureCalloutTheme} [theme] @see FeatureCallout.themePresets
   */

  /** @param {FeatureCalloutOptions} options */
  constructor({ win, prefName, page, browser, theme = {} } = {}) {
    this.win = win;
    this.doc = win.document;
    this.browser = browser || this.win.docShell.chromeEventHandler;
    this.config = null;
    this.loadingConfig = false;
    this.message = null;
    this.currentScreen = null;
    this.renderObserver = null;
    this.savedActiveElement = null;
    this.ready = false;
    this._positionListenersRegistered = false;
    this.AWSetup = false;
    this.page = page;
    this._initTheme(theme);

    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "featureTourProgress",
      prefName,
      '{"screen":"","complete":true}',
      this._handlePrefChange.bind(this),
      val => {
        try {
          return JSON.parse(val);
        } catch (error) {
          return null;
        }
      }
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

    // When the window is focused, ensure tour is synced with tours in any other
    // instances of the parent page. This does not apply when the Callout is
    // shown in the browser chrome.
    if (this.page !== "chrome") {
      this.win.addEventListener("visibilitychange", this);
    }
  }

  /**
   * Get the page event manager and instantiate it if necessary. Only used by
   * _attachPageEventListeners, since we don't want to do this unnecessary work
   * if a message with page event listeners hasn't loaded. Other consumers
   * should use `this._pageEventManager?.property` instead.
   */
  get _loadPageEventManager() {
    if (!this._pageEventManager) {
      this._pageEventManager = new lazy.PageEventManager(this.doc);
    }
    return this._pageEventManager;
  }

  _addPositionListeners() {
    if (!this._positionListenersRegistered) {
      this.win.addEventListener("resize", this);
      const parentEl = this.doc.querySelector(
        this.currentScreen?.parent_selector
      );
      parentEl?.addEventListener("toggle", this);
      this._positionListenersRegistered = true;
    }
  }

  _removePositionListeners() {
    if (this._positionListenersRegistered) {
      this.win.removeEventListener("resize", this);
      const parentEl = this.doc.querySelector(
        this.currentScreen?.parent_selector
      );
      parentEl?.removeEventListener("toggle", this);
      this._positionListenersRegistered = false;
    }
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
      this.endTour();
      this.currentScreen = null;
    } else if (prefVal.screen !== this.currentScreen?.id) {
      this.ready = false;
      this._container?.classList.add("hidden");
      this._pageEventManager?.clear();
      // wait for fade out transition
      this.win.setTimeout(async () => {
        await this._loadConfig();
        this._container?.remove();
        this._removePositionListeners();
        this.doc.querySelector(`[src="${BUNDLE_SRC}"]`)?.remove();
        await this._renderCallout();
      }, TRANSITION_MS);
    }
  }

  handleEvent(event) {
    switch (event.type) {
      case "focus": {
        if (!this._container) {
          return;
        }
        // If focus has fired on the feature callout window itself, or on something
        // contained in that window, ignore it, as we can't possibly place the focus
        // on it after the callout is closd.
        if (
          event.target === this._container ||
          (Node.isInstance(event.target) &&
            this._container.contains(event.target))
        ) {
          return;
        }
        // Save this so that if the next focus event is re-entering the popup,
        // then we'll put the focus back here where the user left it once we exit
        // the feature callout series.
        this.savedActiveElement = this.doc.activeElement;
        break;
      }

      case "keypress": {
        if (event.key !== "Escape") {
          return;
        }
        if (!this._container) {
          return;
        }
        let focusedElement =
          this.page === "chrome"
            ? Services.focus.focusedElement
            : this.doc.activeElement;
        // If the window has a focused element, let it handle the ESC key instead.
        if (
          !focusedElement ||
          focusedElement === this.doc.body ||
          focusedElement === this.browser ||
          this._container.contains(focusedElement)
        ) {
          this.win.AWSendEventTelemetry?.({
            event: "DISMISS",
            event_context: {
              source: `KEY_${event.key}`,
              page: this.page,
            },
            message_id: this.config?.id.toUpperCase(),
          });
          this._dismiss();
          event.preventDefault();
        }
        break;
      }

      case "visibilitychange":
        this._handlePrefChange();
        break;

      case "resize":
      case "toggle":
        this._positionCallout();
        break;

      default:
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
      "toolkit/branding/brandings.ftl",
      "browser/newtab/asrouter.ftl",
      "browser/featureCallout.ftl",
    ]);
  }

  _createContainer() {
    let parent = this.doc.querySelector(this.currentScreen?.parent_selector);
    // Don't render the callout if the parent element is not present.
    // This means the message was misconfigured, mistargeted, or the
    // content of the parent page is not as expected.
    if (!parent && !this.currentScreen?.content?.callout_position_override) {
      if (this.message?.template === "feature_callout") {
        Services.telemetry.recordEvent(
          "messaging_experiments",
          "feature_callout",
          "create_failed",
          `${this.message.id || "no_message"}-${this.currentScreen
            ?.parent_selector || "no_current_screen"}`
        );
      }

      return false;
    }

    if (!this._container?.parentElement) {
      this._container = this.doc.createElement("div");
      this._container.classList.add(
        "onboardingContainer",
        "featureCallout",
        "callout-arrow",
        "hidden"
      );
      this._container.id = CONTAINER_ID;
      // This value is reported as the "page" in about:welcome telemetry
      this._container.dataset.page = this.page;
      this._container.setAttribute(
        "aria-describedby",
        `#${CONTAINER_ID} .welcome-text`
      );
      this._container.tabIndex = 0;
      this._applyTheme();
      this.doc.body.prepend(this._container);
    }
    return this._container;
  }

  /**
   * Set callout's position relative to parent element
   */
  _positionCallout() {
    const container = this._container;
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

    const addValueToPixelValue = (value, pixelValue) => {
      return `${Number(pixelValue.split("px")[0]) + value}px`;
    };

    const subtractPixelValueFromValue = (pixelValue, value) => {
      return `${value - Number(pixelValue.split("px")[0])}px`;
    };

    const overridePosition = () => {
      // We override _every_ positioner here, because we want to manually set all
      // container.style.positions in every positioner's "position" function
      // regardless of the actual arrow position
      // Note: We override the position functions with new functions here,
      // but they don't actually get executed until the respective position functions are called
      // and this function is not executed unless the message has a custom position property.

      // We're positioning relative to a parent element's bounds,
      // if that parent element exists.

      for (const position in positioners) {
        positioners[position].position = () => {
          if (customPosition.top) {
            container.style.top = addValueToPixelValue(
              parentEl.getBoundingClientRect().top,
              customPosition.top
            );
          }

          if (customPosition.left) {
            const leftPosition = addValueToPixelValue(
              parentEl.getBoundingClientRect().left,
              customPosition.left
            );

            RTL
              ? (container.style.right = leftPosition)
              : (container.style.left = leftPosition);
          }

          if (customPosition.right) {
            const rightPosition = subtractPixelValueFromValue(
              customPosition.right,
              parentEl.getBoundingClientRect().right - container.clientWidth
            );

            RTL
              ? (container.style.right = rightPosition)
              : (container.style.left = rightPosition);
          }

          if (customPosition.bottom) {
            container.style.top = subtractPixelValueFromValue(
              customPosition.bottom,
              parentEl.getBoundingClientRect().bottom - container.clientHeight
            );
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
     * @param {String} [position = "start"] <"start"|"end"|"center">
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
      overridePosition();
    }

    let finalPosition = choosePosition();
    if (finalPosition) {
      positioners[finalPosition].position();
      addArrowPositionClassToContainer(finalPosition);
    }

    container.classList.remove("hidden");
  }

  /** Expose top level functions expected by the aboutwelcome bundle. */
  _setupWindowFunctions() {
    if (this.AWSetup) {
      return;
    }
    const AWParent = new lazy.AboutWelcomeParent();
    this.win.addEventListener("unload", () => {
      AWParent.didDestroy();
    });
    const receive = name => data =>
      AWParent.onContentMessage(`AWPage:${name}`, data, this.doc);
    this.win.AWGetFeatureConfig = () => this.config;
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
      this.endTour();
    };
    this.win.AWEvaluateScreenTargeting = receive("EVALUATE_SCREEN_TARGETING");
    this.AWSetup = true;
  }

  /** Clean up the functions defined above. */
  _clearWindowFunctions() {
    const windowFuncs = [
      "AWGetFeatureConfig",
      "AWGetSelectedTheme",
      "AWSendEventTelemetry",
      "AWSendToDeviceEmailsSupported",
      "AWSendToParent",
      "AWFinish",
    ];
    windowFuncs.forEach(func => delete this.win[func]);
  }

  endTour(skipFadeOut = false) {
    // We don't want focus events that happen during teardown to affect
    // this.savedActiveElement
    this.win.removeEventListener("focus", this, {
      capture: true,
      passive: true,
    });
    this.win.removeEventListener("keypress", this, { capture: true });
    this._pageEventManager?.clear();

    // We're deleting featureTourProgress here to ensure that the
    // reference is freed for garbage collection. This prevents errors
    // caused by lingering instances when instantiating and removing
    // multiple feature tour instances in succession.
    delete this.featureTourProgress;
    this.ready = false;
    // wait for fade out transition
    this._container?.classList.add("hidden");
    this._clearWindowFunctions();
    this.win.setTimeout(
      () => {
        this._container?.remove();
        this.renderObserver?.disconnect();
        this._removePositionListeners();
        this.doc.querySelector(`[src="${BUNDLE_SRC}"]`)?.remove();
        // Put the focus back to the last place the user focused outside of the
        // featureCallout windows.
        if (this.savedActiveElement) {
          this.savedActiveElement.focus({ focusVisible: true });
        }
      },
      skipFadeOut ? 0 : TRANSITION_MS
    );
  }

  _dismiss() {
    let action = this.currentScreen?.content.dismiss_button?.action;
    if (action?.type) {
      this.win.AWSendToParent("SPECIAL_ACTION", action);
    } else {
      this.endTour();
    }
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
    this.doc.querySelector(`[src="${BUNDLE_SRC}"]`)?.remove();
    let bundleScript = this.doc.createElement("script");
    bundleScript.src = BUNDLE_SRC;
    this.doc.head.appendChild(bundleScript);
  }

  _observeRender(container) {
    this.renderObserver?.observe(container, { childList: true });
  }

  /**
   * Request a message from ASRouter, targeting the `browser` and `page` values
   * passed to the constructor. The message content is stored in this.config,
   * which is returned by AWGetFeatureConfig. The aboutwelcome bundle will use
   * that function to get the content. It will only be called when the bundle
   * loads, so the bundle must be reloaded for a new message to be rendered.
   * @returns {Promise<boolean>} true if a message is loaded, false if not.
   */
  async _loadConfig() {
    if (this.loadingConfig) {
      return false;
    }
    this.loadingConfig = true;
    await lazy.ASRouter.waitForInitialized;
    let result = await lazy.ASRouter.sendTriggerMessage({
      browser: this.browser,
      // triggerId and triggerContext
      id: "featureCalloutCheck",
      context: { source: this.page },
    });
    this.message = result.message;
    this.loadingConfig = false;

    if (result.message.template !== "feature_callout") {
      // If another message type, like a Spotlight modal, is included
      // in the tour, save the template name as the current screen.
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
      lazy.ASRouter.addImpression(result.message);
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
      this._loadPageEventManager[params.options?.once ? "once" : "on"](
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
    const page = this.page;
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
      this._dismiss();
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
      if (!this.ready && this._container.querySelector(".screen")) {
        // Once the screen element is added to the DOM, wait for the
        // animation frame after next to ensure that _positionCallout
        // has access to the rendered screen with the correct height
        this.win.requestAnimationFrame(() => {
          this.win.requestAnimationFrame(() => {
            this.ready = true;
            this._attachPageEventListeners(
              this.currentScreen?.content?.page_event_listeners
            );
            this.win.addEventListener("keypress", this, { capture: true });
            this._positionCallout();
            let button = this._container.querySelector(".primary");
            button.focus();
            this.win.addEventListener("focus", this, {
              capture: true, // get the event before retargeting
              passive: true,
            });
          });
        });
      }
    });

    this._pageEventManager?.clear();
    this.ready = false;
    this._container?.remove();

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

  /**
   * @typedef {Object} FeatureCalloutTheme An object with a set of custom color
   *   schemes and/or a preset key. If both are provided, the preset will be
   *   applied first, then the custom themes will override the preset values.
   * @property {String} [preset] Key of {@link FeatureCallout.themePresets}
   * @property {ColorScheme} [light] Custom light scheme
   * @property {ColorScheme} [dark] Custom dark scheme
   * @property {ColorScheme} [hcm] Custom high contrast scheme
   * @property {ColorScheme} [all] Custom scheme that will be applied in all
   *   cases, but overridden by the other schemes if they are present. This is
   *   useful if the values are already controlled by the browser theme.
   * @property {Boolean} [simulateContent] Set to true if the feature callout
   *   exists in the browser chrome but is meant to be displayed over the
   *   content area to appear as if it is part of the page. This will cause the
   *   styles to use a media query targeting the content instead of the chrome,
   *   so that if the browser theme doesn't match the content color scheme, the
   *   callout will correctly follow the content scheme. This is currently used
   *   for the feature callouts displayed over the PDF.js viewer.
   */

  /**
   * @typedef {Object} ColorScheme An object with key-value pairs, with keys
   *   from {@link FeatureCallout.themePropNames}, mapped to CSS color values
   */

  /**
   * Combine the preset and custom themes into a single object and store it.
   * @param {FeatureCalloutTheme} theme
   */
  _initTheme(theme) {
    /** @type {FeatureCalloutTheme} */
    this.theme = Object.assign(
      {},
      FeatureCallout.themePresets[theme.preset],
      theme
    );
  }

  /**
   * Apply all the theme colors to the feature callout's root element as CSS
   * custom properties in inline styles. These custom properties are consumed by
   * _feature-callout-theme.scss, which is bundled with the other styles that
   * are loaded by {@link FeatureCallout.prototype._addCalloutLinkElements}.
   */
  _applyTheme() {
    if (this._container) {
      // This tells the stylesheets to use -moz-content-prefers-color-scheme
      // instead of prefers-color-scheme, in order to follow the content color
      // scheme instead of the chrome color scheme, in case of a mismatch when
      // the feature callout exists in the chrome but is meant to look like it's
      // part of the content of a page in a browser tab (like PDF.js).
      this._container.classList.toggle(
        "simulateContent",
        this.page === "chrome" && this.theme.simulateContent
      );
      for (const type of ["light", "dark", "hcm"]) {
        const scheme = this.theme[type];
        for (const name of FeatureCallout.themePropNames) {
          this._setThemeVariable(
            `--fc-${name}-${type}`,
            scheme?.[name] || this.theme.all[name]
          );
        }
      }
    }
  }

  /**
   * Set or remove a CSS custom property on the feature callout container
   * @param {String} name Name of the CSS custom property
   * @param {String|void} [value] Value of the property, or omit to remove it
   */
  _setThemeVariable(name, value) {
    if (value) {
      this._container.style.setProperty(name, value);
    } else {
      this._container.style.removeProperty(name);
    }
  }

  /** A list of all the theme properties that can be set */
  static themePropNames = [
    "background",
    "color",
    "border",
    "accent-color",
    "button-background",
    "button-color",
    "button-border",
    "button-background-hover",
    "button-color-hover",
    "button-border-hover",
    "button-background-active",
    "button-color-active",
    "button-border-active",
  ];

  /** @type {Object<String, FeatureCalloutTheme>} */
  static themePresets = {
    // For themed system pages like New Tab and Firefox View. Themed content
    // colors inherit from the user's theme through contentTheme.js.
    "themed-content": {
      all: {
        background: "var(--newtab-background-color-secondary)",
        color: "var(--newtab-text-primary-color, var(--in-content-page-color))",
        border:
          "color-mix(in srgb, var(--newtab-background-color-secondary) 80%, #000)",
        "accent-color": "var(--in-content-primary-button-background)",
        "button-background": "color-mix(in srgb, transparent 93%, #000)",
        "button-color":
          "var(--newtab-text-primary-color, var(--in-content-page-color))",
        "button-border": "transparent",
        "button-background-hover": "color-mix(in srgb, transparent 88%, #000)",
        "button-color-hover":
          "var(--newtab-text-primary-color, var(--in-content-page-color))",
        "button-border-hover": "transparent",
        "button-background-active": "color-mix(in srgb, transparent 80%, #000)",
        "button-color-active":
          "var(--newtab-text-primary-color, var(--in-content-page-color))",
        "button-border-active": "transparent",
      },
      dark: {
        border:
          "color-mix(in srgb, var(--newtab-background-color-secondary) 80%, #FFF)",
        "button-background": "color-mix(in srgb, transparent 80%, #000)",
        "button-background-hover": "color-mix(in srgb, transparent 65%, #000)",
        "button-background-active": "color-mix(in srgb, transparent 55%, #000)",
      },
      hcm: {
        background: "-moz-dialog",
        color: "-moz-dialogtext",
        border: "-moz-dialogtext",
        "accent-color": "LinkText",
        "button-background": "ButtonFace",
        "button-color": "ButtonText",
        "button-border": "ButtonText",
        "button-background-hover": "ButtonText",
        "button-color-hover": "ButtonFace",
        "button-border-hover": "ButtonText",
        "button-background-active": "ButtonText",
        "button-color-active": "ButtonFace",
        "button-border-active": "ButtonText",
      },
    },
    // PDF.js colors are from toolkit/components/pdfjs/content/web/viewer.css
    pdfjs: {
      all: {
        background: "#FFF",
        color: "rgb(12, 12, 13)",
        border: "#CFCFD8",
        "accent-color": "#0A84FF",
        "button-background": "rgb(215, 215, 219)",
        "button-color": "rgb(12, 12, 13)",
        "button-border": "transparent",
        "button-background-hover": "rgb(221, 222, 223)",
        "button-color-hover": "rgb(12, 12, 13)",
        "button-border-hover": "transparent",
        "button-background-active": "rgb(221, 222, 223)",
        "button-color-active": "rgb(12, 12, 13)",
        "button-border-active": "transparent",
      },
      dark: {
        background: "#1C1B22",
        color: "#F9F9FA",
        border: "#3A3944",
        "button-background": "rgb(74, 74, 79)",
        "button-color": "#F9F9FA",
        "button-background-hover": "rgb(102, 102, 103)",
        "button-color-hover": "#F9F9FA",
        "button-background-active": "rgb(102, 102, 103)",
        "button-color-active": "#F9F9FA",
      },
      hcm: {
        background: "-moz-dialog",
        color: "-moz-dialogtext",
        border: "CanvasText",
        "accent-color": "Highlight",
        "button-background": "ButtonFace",
        "button-color": "ButtonText",
        "button-border": "ButtonText",
        "button-background-hover": "Highlight",
        "button-color-hover": "CanvasText",
        "button-border-hover": "Highlight",
        "button-background-active": "Highlight",
        "button-color-active": "CanvasText",
        "button-border-active": "Highlight",
      },
    },
    // These colors are intended to inherit the user's theme properties from the
    // main chrome window, for callouts to be anchored to chrome elements.
    // Specific schemes aren't necessary since the theme and frontend
    // stylesheets handle these variables' values.
    chrome: {
      all: {
        background: "var(--arrowpanel-background)",
        color: "var(--arrowpanel-color)",
        border: "var(--arrowpanel-border-color)",
        "accent-color": "var(--focus-outline-color)",
        "button-background": "var(--button-bgcolor)",
        "button-color": "var(--arrowpanel-color)",
        "button-border": "transparent",
        "button-background-hover": "var(--button-hover-bgcolor)",
        "button-color-hover": "var(--arrowpanel-color)",
        "button-border-hover": "transparent",
        "button-background-active": "var(--button-active-bgcolor)",
        "button-color-active": "var(--arrowpanel-color)",
        "button-border-active": "transparent",
      },
    },
  };
}
