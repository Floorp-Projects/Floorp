/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AboutWelcomeParent: "resource:///actors/AboutWelcomeParent.sys.mjs",
  ASRouter: "resource:///modules/asrouter/ASRouter.sys.mjs",
  CustomizableUI: "resource:///modules/CustomizableUI.sys.mjs",
  PageEventManager: "resource:///modules/asrouter/PageEventManager.sys.mjs",
});

const TRANSITION_MS = 500;
const CONTAINER_ID = "feature-callout";
const CONTENT_BOX_ID = "multi-stage-message-root";
const BUNDLE_SRC =
  "chrome://browser/content/aboutwelcome/aboutwelcome.bundle.js";

ChromeUtils.defineLazyGetter(lazy, "log", () => {
  const { Logger } = ChromeUtils.importESModule(
    "resource://messaging-system/lib/Logger.sys.mjs"
  );
  return new Logger("FeatureCallout");
});

/**
 * Feature Callout fetches messages relevant to a given source and displays them
 * in the parent page pointing to the element they describe.
 */
export class FeatureCallout {
  /**
   * @typedef {Object} FeatureCalloutOptions
   * @property {Window} win window in which messages will be rendered.
   * @property {{name: String, defaultValue?: String}} [pref] optional pref used
   *   to track progress through a given feature tour. for example:
   *   {
   *     name: "browser.pdfjs.feature-tour",
   *     defaultValue: '{ screen: "FEATURE_CALLOUT_1", complete: false }',
   *   }
   *   or { name: "browser.pdfjs.feature-tour" } (defaultValue is optional)
   * @property {String} [location] string to pass as the page when requesting
   *   messages from ASRouter and sending telemetry.
   * @property {String} context either "chrome" or "content". "chrome" is used
   *   when the callout is shown in the browser chrome, and "content" is used
   *   when the callout is shown in a content page like Firefox View.
   * @property {MozBrowser} [browser] <browser> element responsible for the
   *   feature callout. for content pages, this is the browser element that the
   *   callout is being shown in. for chrome, this is the active browser.
   * @property {Function} [listener] callback to be invoked on various callout
   *   events to keep the broker informed of the callout's state.
   * @property {FeatureCalloutTheme} [theme] @see FeatureCallout.themePresets
   */

  /** @param {FeatureCalloutOptions} options */
  constructor({
    win,
    pref,
    location,
    context,
    browser,
    listener,
    theme = {},
  } = {}) {
    this.win = win;
    this.doc = win.document;
    this.browser = browser || this.win.docShell.chromeEventHandler;
    this.config = null;
    this.loadingConfig = false;
    this.message = null;
    if (pref?.name) {
      this.pref = pref;
    }
    this._featureTourProgress = null;
    this.currentScreen = null;
    this.renderObserver = null;
    this.savedFocus = null;
    this.ready = false;
    this._positionListenersRegistered = false;
    this._panelConflictListenersRegistered = false;
    this.AWSetup = false;
    this.location = location;
    this.context = context;
    this.listener = listener;
    this._initTheme(theme);

    this._handlePrefChange = this._handlePrefChange.bind(this);

    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "cfrFeaturesUserPref",
      "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features",
      true
    );
    this.setupFeatureTourProgress();

    // When the window is focused, ensure tour is synced with tours in any other
    // instances of the parent page. This does not apply when the Callout is
    // shown in the browser chrome.
    if (this.context !== "chrome") {
      this.win.addEventListener("visibilitychange", this);
    }

    this.win.addEventListener("unload", this);
  }

  setupFeatureTourProgress() {
    if (this.featureTourProgress) {
      return;
    }
    if (this.pref?.name) {
      this._handlePrefChange(null, null, this.pref.name);
      Services.prefs.addObserver(this.pref.name, this._handlePrefChange);
    }
  }

  teardownFeatureTourProgress() {
    if (this.pref?.name) {
      Services.prefs.removeObserver(this.pref.name, this._handlePrefChange);
    }
    this._featureTourProgress = null;
  }

  get featureTourProgress() {
    return this._featureTourProgress;
  }

  /**
   * Get the page event manager and instantiate it if necessary. Only used by
   * _attachPageEventListeners, since we don't want to do this unnecessary work
   * if a message with page event listeners hasn't loaded. Other consumers
   * should use `this._pageEventManager?.property` instead.
   */
  get _loadPageEventManager() {
    if (!this._pageEventManager) {
      this._pageEventManager = new lazy.PageEventManager(this.win);
    }
    return this._pageEventManager;
  }

  _addPositionListeners() {
    if (!this._positionListenersRegistered) {
      this.win.addEventListener("resize", this);
      this._positionListenersRegistered = true;
    }
  }

  _removePositionListeners() {
    if (this._positionListenersRegistered) {
      this.win.removeEventListener("resize", this);
      this._positionListenersRegistered = false;
    }
  }

  _addPanelConflictListeners() {
    if (!this._panelConflictListenersRegistered) {
      this.win.addEventListener("popupshowing", this);
      this.win.gURLBar.controller.addQueryListener(this);
      this._panelConflictListenersRegistered = true;
    }
  }

  _removePanelConflictListeners() {
    if (this._panelConflictListenersRegistered) {
      this.win.removeEventListener("popupshowing", this);
      this.win.gURLBar.controller.removeQueryListener(this);
      this._panelConflictListenersRegistered = false;
    }
  }

  /**
   * Close the tour when the urlbar is opened in the chrome. Set up by
   * gURLBar.controller.addQueryListener in _addPanelConflictListeners.
   */
  onViewOpen() {
    this.endTour();
  }

  _handlePrefChange(subject, topic, prefName) {
    switch (prefName) {
      case this.pref?.name:
        try {
          this._featureTourProgress = JSON.parse(
            Services.prefs.getStringPref(
              this.pref.name,
              this.pref.defaultValue ?? null
            )
          );
        } catch (error) {
          this._featureTourProgress = null;
        }
        if (topic === "nsPref:changed") {
          this._maybeAdvanceScreens();
        }
        break;
    }
  }

  _maybeAdvanceScreens() {
    if (this.doc.visibilityState === "hidden" || !this.featureTourProgress) {
      return;
    }

    // If we have more than one screen, it means that we're displaying a feature
    // tour, and transitions are handled based on the value of a tour progress
    // pref. Otherwise, just show the feature callout. If a pref change results
    // from an event in a Spotlight message, initialize the feature callout with
    // the next message in the tour.
    if (
      this.config?.screens.length === 1 ||
      this.currentScreen === "spotlight"
    ) {
      this.showFeatureCallout();
      return;
    }

    let prefVal = this.featureTourProgress;
    // End the tour according to the tour progress pref or if the user disabled
    // contextual feature recommendations.
    if (prefVal.complete || !this.cfrFeaturesUserPref) {
      this.endTour();
    } else if (prefVal.screen !== this.currentScreen?.id) {
      // Pref changes only matter to us insofar as they let us advance an
      // ongoing tour. If the tour was closed and the pref changed later, e.g.
      // by editing the pref directly, we don't want to start up the tour again.
      // This is more important in the chrome, which is always open.
      if (this.context === "chrome" && !this.currentScreen) {
        return;
      }
      this.ready = false;
      this._container?.classList.toggle(
        "hidden",
        this._container?.localName !== "panel"
      );
      this._pageEventManager?.emit({
        type: "touradvance",
        target: this._container,
      });
      const onFadeOut = async () => {
        // If the initial message was deployed from outside by ASRouter as a
        // result of a trigger, we can't continue it through _loadConfig, since
        // that effectively requests a message with a `featureCalloutCheck`
        // trigger. So we need to load up the same message again, merely
        // changing the startScreen index. Just check that the next screen and
        // the current screen are both within the message's screens array.
        let nextMessage = null;
        if (
          this.context === "chrome" &&
          this.message?.trigger.id !== "featureCalloutCheck"
        ) {
          if (
            this.config?.screens.some(s => s.id === this.currentScreen?.id) &&
            this.config.screens.some(s => s.id === prefVal.screen)
          ) {
            nextMessage = this.message;
          }
        }
        this._container?.remove();
        this.renderObserver?.disconnect();
        this._removePositionListeners();
        this._removePanelConflictListeners();
        this.doc.querySelector(`[src="${BUNDLE_SRC}"]`)?.remove();
        if (nextMessage) {
          const isMessageUnblocked = await lazy.ASRouter.isUnblockedMessage(
            nextMessage
          );
          if (!isMessageUnblocked) {
            this.endTour();
            return;
          }
        }
        let updated = await this._updateConfig(nextMessage);
        if (!updated && !this.currentScreen) {
          this.endTour();
          return;
        }
        let rendering = await this._renderCallout();
        if (!rendering) {
          this.endTour();
        }
      };
      if (this._container?.localName === "panel") {
        this._container.removeEventListener("popuphiding", this);
        this._container.addEventListener("popuphidden", onFadeOut, {
          once: true,
        });
        this._container.hidePopup(true);
      } else {
        this.win.setTimeout(onFadeOut, TRANSITION_MS);
      }
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
        if (this.doc.activeElement) {
          let element = this.doc.activeElement;
          this.savedFocus = {
            element,
            focusVisible: element.matches(":focus-visible"),
          };
        } else {
          this.savedFocus = null;
        }
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
          this.context === "chrome"
            ? Services.focus.focusedElement
            : this.doc.activeElement;
        // If the window has a focused element, let it handle the ESC key instead.
        if (
          !focusedElement ||
          focusedElement === this.doc.body ||
          (focusedElement === this.browser && this.theme.simulateContent) ||
          this._container.contains(focusedElement)
        ) {
          this.win.AWSendEventTelemetry?.({
            event: "DISMISS",
            event_context: {
              source: `KEY_${event.key}`,
              page: this.location,
            },
            message_id: this.config?.id.toUpperCase(),
          });
          this._dismiss();
          event.preventDefault();
        }
        break;
      }

      case "visibilitychange":
        this._maybeAdvanceScreens();
        break;

      case "resize":
      case "toggle":
        this.win.requestAnimationFrame(() => this._positionCallout());
        break;

      case "popupshowing":
        // If another panel is showing, close the tour.
        if (
          event.target !== this._container &&
          event.target.localName === "panel" &&
          event.target.id !== "ctrlTab-panel" &&
          event.target.ownerGlobal === this.win
        ) {
          this.endTour();
        }
        break;

      case "popuphiding":
        if (event.target === this._container) {
          this.endTour();
        }
        break;

      case "unload":
        try {
          this.teardownFeatureTourProgress();
        } catch (error) {}
        break;

      default:
    }
  }

  async _addCalloutLinkElements() {
    for (const path of [
      "browser/newtab/onboarding.ftl",
      "browser/spotlight.ftl",
      "branding/brand.ftl",
      "toolkit/branding/brandings.ftl",
      "browser/newtab/asrouter.ftl",
      "browser/featureCallout.ftl",
    ]) {
      this.win.MozXULElement.insertFTLIfNeeded(path);
    }

    const addChromeSheet = href => {
      try {
        this.win.windowUtils.loadSheetUsingURIString(
          href,
          Ci.nsIDOMWindowUtils.AUTHOR_SHEET
        );
      } catch (error) {
        // the sheet was probably already loaded. I don't think there's a way to
        // check for this via JS, but the method checks and throws if it's
        // already loaded, so we can just treat the error as expected.
      }
    };
    const addStylesheet = href => {
      if (this.win.isChromeWindow) {
        // for chrome, load the stylesheet using a special method to make sure
        // it's loaded synchronously before the first paint & position.
        return addChromeSheet(href);
      }
      if (this.doc.querySelector(`link[href="${href}"]`)) {
        return null;
      }
      const link = this.doc.head.appendChild(this.doc.createElement("link"));
      link.rel = "stylesheet";
      link.href = href;
      return null;
    };
    // Update styling to be compatible with about:welcome bundle
    await addStylesheet(
      "chrome://browser/content/aboutwelcome/aboutwelcome.css"
    );
  }

  /**
   * @typedef {
   * | "topleft"
   * | "topright"
   * | "bottomleft"
   * | "bottomright"
   * | "leftcenter"
   * | "rightcenter"
   * | "topcenter"
   * | "bottomcenter"
   * } PopupAttachmentPoint
   *
   * @see nsMenuPopupFrame
   *
   * Each attachment point corresponds to an attachment point on the edge of a
   * frame. For example, "topleft" corresponds to the frame's top left corner,
   * and "rightcenter" corresponds to the center of the right edge of the frame.
   */

  /**
   * @typedef {Object} PanelPosition Specifies how the callout panel should be
   *   positioned relative to the anchor element, by providing which point on
   *   the callout should be aligned with which point on the anchor element.
   * @property {PopupAttachmentPoint} anchor_attachment
   * @property {PopupAttachmentPoint} callout_attachment
   * @property {String} [panel_position_string] The attachments joined into a
   *   string, e.g. "bottomleft topright". Passed to XULPopupElement::openPopup.
   *   This is not provided by JSON, but generated from anchor_attachment and
   *   callout_attachment.
   * @property {Number} [offset_x] Offset in pixels to apply to the callout
   *   position in the horizontal direction.
   * @property {Number} [offset_y] The same in the vertical direction.
   *
   * This is used when you want the callout to be displayed as a <panel>
   * element. A panel is critical when the callout is displayed in the browser
   * chrome, anchored to an element whose position on the screen is dynamic,
   * such as a button. When the anchor moves, the panel will automatically move
   * with it. Also, when the elements are aligned so that the callout would
   * extend beyond the edge of the screen, the panel will automatically flip
   * itself to the other side of the anchor element. This requires specifying
   * both an anchor attachment point and a callout attachment point. For
   * example, to get the callout to appear under a button, with its arrow on the
   * right side of the callout:
   * { anchor_attachment: "bottomcenter", callout_attachment: "topright" }
   */

  /**
   * @typedef {
   * | "top"
   * | "bottom"
   * | "end"
   * | "start"
   * | "top-end"
   * | "top-start"
   * | "top-center-arrow-end"
   * | "top-center-arrow-start"
   * } HTMLArrowPosition
   *
   * @see FeatureCallout._positionCallout()
   * The position of the callout arrow relative to the callout container. Only
   * used for HTML callouts, typically in content pages. If the position
   * contains a dash, the value before the dash refers to which edge of the
   * feature callout the arrow points from. The value after the dash describes
   * where along that edge the arrow sits, with middle as the default.
   */

  /**
   * @typedef {Object} PositionOverride CSS properties to override
   *   the callout's position relative to the anchor element. Although the
   *   callout is not actually a child of the anchor element, this allows
   *   absolute positioning of the callout relative to the anchor element. In
   *   other words, { top: "0px", left: "0px" } will position the callout in the
   *   top left corner of the anchor element, in the same way these properties
   *   would position a child element.
   * @property {String} [top]
   * @property {String} [left]
   * @property {String} [right]
   * @property {String} [bottom]
   */

  /**
   * @typedef {Object} Anchor
   * @property {String} selector CSS selector for the anchor node.
   * @property {Element} [element] The anchor node resolved from the selector.
   *   Not provided by JSON, but generated dynamically.
   * @property {PanelPosition} [panel_position] Used to show the callout in a
   *   XUL panel. Only works in chrome documents, like the main browser window.
   * @property {HTMLArrowPosition} [arrow_position] Used to show the callout in
   *   an HTML div container. Mutually exclusive with panel_position.
   * @property {PositionOverride} [absolute_position] Only used for HTML
   *   callouts, i.e. when panel_position is not specified. Allows absolute
   *   positioning of the callout relative to the anchor element.
   * @property {Boolean} [hide_arrow] Whether to hide the arrow.
   * @property {Boolean} [no_open_on_anchor] Whether to set the [open] style on
   *   the anchor element when the callout is shown. False to set it, true to
   *   not set it. This only works for panel callouts. Not all elements have an
   *   [open] style. Buttons do, for example. It's usually similar to :active.
   * @property {Number} [arrow_width] The desired width of the arrow in a number
   *   of pixels. 33.94113 by default (this corresponds to 24px edges).
   */

  /**
   * Return the first visible anchor element for the current screen. Screens can
   * specify multiple anchors in an array, and the first one that is visible
   * will be used. If none are visible, return null.
   * @returns {Anchor|null}
   */
  _getAnchor() {
    /** @type {Anchor[]} */
    const anchors = Array.isArray(this.currentScreen?.anchors)
      ? this.currentScreen.anchors
      : [];
    for (let anchor of anchors) {
      if (!anchor || typeof anchor !== "object") {
        lazy.log.debug(
          `In ${this.location}: Invalid anchor config. Expected an object, got: ${anchor}`
        );
        continue;
      }
      const { selector, arrow_position, panel_position } = anchor;
      if (panel_position) {
        let panel_position_string =
          this._getPanelPositionString(panel_position);
        // if the positionString doesn't match the format we expect, don't
        // render the callout.
        if (!panel_position_string && !arrow_position) {
          lazy.log.debug(
            `In ${
              this.location
            }: Invalid panel_position config. Expected an object with anchor_attachment and callout_attachment properties, got: ${JSON.stringify(
              panel_position
            )}`
          );
          continue;
        }
        panel_position.panel_position_string = panel_position_string;
      }
      if (
        arrow_position &&
        !this._HTMLArrowPositions.includes(arrow_position)
      ) {
        lazy.log.debug(
          `In ${
            this.location
          }: Invalid arrow_position config. Expected one of ${JSON.stringify(
            this._HTMLArrowPositions
          )}, got: ${arrow_position}`
        );
        continue;
      }
      const element = selector && this.doc.querySelector(selector);
      if (!element) {
        continue; // Element doesn't exist at all.
      }
      const isVisible = () => {
        if (
          this.context === "chrome" &&
          typeof this.win.isElementVisible === "function"
        ) {
          // In chrome windows, we can use the isElementVisible function to
          // check that the element has non-zero width and height. If it was
          // hidden, it would most likely have zero width and/or height.
          if (!this.win.isElementVisible(element)) {
            return false;
          }
        }
        // CSS rules like visibility: hidden or display: none. These result in
        // element being invisible and unclickable.
        const style = this.win.getComputedStyle(element);
        return style?.visibility === "visible" && style?.display !== "none";
      };
      if (!isVisible()) {
        continue;
      }
      if (
        this.context === "chrome" &&
        element.id &&
        anchor.selector.includes(`#${element.id}`)
      ) {
        let widget = lazy.CustomizableUI.getWidget(element.id);
        if (
          widget &&
          (this.win.CustomizationHandler.isCustomizing() ||
            widget.areaType?.includes("panel"))
        ) {
          // The element is a customizable widget (a toolbar item, e.g. the
          // reload button or the downloads button). Widgets can be in various
          // areas, like the overflow panel or the customization palette.
          // Widgets in the palette are present in the chrome's DOM during
          // customization, but can't be used.
          continue;
        }
      }
      return { ...anchor, element };
    }
    return null;
  }

  /** @see PopupAttachmentPoint */
  _popupAttachmentPoints = [
    "topleft",
    "topright",
    "bottomleft",
    "bottomright",
    "leftcenter",
    "rightcenter",
    "topcenter",
    "bottomcenter",
  ];

  /**
   * Return a string representing the position of the panel relative to the
   * anchor element. Passed to XULPopupElement::openPopup. The string is of the
   * form "anchor_attachment callout_attachment".
   *
   * @param {PanelPosition} panelPosition
   * @returns {String|null} A string like "bottomcenter topright", or null if
   *   the panelPosition object is invalid.
   */
  _getPanelPositionString(panelPosition) {
    const { anchor_attachment, callout_attachment } = panelPosition;
    if (
      !this._popupAttachmentPoints.includes(anchor_attachment) ||
      !this._popupAttachmentPoints.includes(callout_attachment)
    ) {
      return null;
    }
    let positionString = `${anchor_attachment} ${callout_attachment}`;
    return positionString;
  }

  /**
   * Set/override methods on a panel element. Can be used to override methods on
   * the custom element class, or to add additional methods.
   *
   * @param {MozPanel} panel The panel to set methods for
   */
  _setPanelMethods(panel) {
    // This method is optionally called by MozPanel::_setSideAttribute, though
    // it does not exist on the class.
    panel.setArrowPosition = function setArrowPosition(event) {
      if (!this.hasAttribute("show-arrow")) {
        return;
      }
      let { alignmentPosition, alignmentOffset, popupAlignment } = event;
      let positionParts = alignmentPosition?.match(
        /^(before|after|start|end)_(before|after|start|end)$/
      );
      if (!positionParts) {
        return;
      }
      // Hide the arrow if the `flip` behavior has caused the panel to
      // offset relative to its anchor, since the arrow would no longer
      // point at the true anchor. This differs from an arrow that is
      // intentionally hidden by the user in message.
      if (this.getAttribute("hide-arrow") !== "permanent") {
        if (alignmentOffset) {
          this.setAttribute("hide-arrow", "temporary");
        } else {
          this.removeAttribute("hide-arrow");
        }
      }
      let arrowPosition = "top";
      switch (positionParts[1]) {
        case "start":
        case "end": {
          // Inline arrow, i.e. arrow is on one of the left/right edges.
          let isRTL =
            this.ownerGlobal.getComputedStyle(this).direction === "rtl";
          let isRight = isRTL ^ (positionParts[1] === "start");
          let side = isRight ? "end" : "start";
          arrowPosition = `inline-${side}`;
          if (popupAlignment?.includes("center")) {
            arrowPosition = `inline-${side}`;
          } else if (positionParts[2] === "before") {
            arrowPosition = `inline-${side}-top`;
          } else if (positionParts[2] === "after") {
            arrowPosition = `inline-${side}-bottom`;
          }
          break;
        }
        case "before":
        case "after": {
          // Block arrow, i.e. arrow is on one of the top/bottom edges.
          let side = positionParts[1] === "before" ? "bottom" : "top";
          arrowPosition = side;
          if (popupAlignment?.includes("center")) {
            arrowPosition = side;
          } else if (positionParts[2] === "end") {
            arrowPosition = `${side}-end`;
          } else if (positionParts[2] === "start") {
            arrowPosition = `${side}-start`;
          }
          break;
        }
      }
      this.setAttribute("arrow-position", arrowPosition);
    };
  }

  _createContainer() {
    const anchor = this._getAnchor();
    // Don't render the callout if none of the anchors is visible.
    if (!anchor) {
      return false;
    }

    const { autohide, padding } = this.currentScreen.content;
    const { panel_position, hide_arrow, no_open_on_anchor, arrow_width } =
      anchor;
    const needsPanel =
      "MozXULElement" in this.win && !!panel_position?.panel_position_string;

    if (this._container) {
      if (needsPanel ^ (this._container?.localName === "panel")) {
        this._container.remove();
      }
    }

    if (!this._container?.parentElement) {
      if (needsPanel) {
        let fragment = this.win.MozXULElement.parseXULToFragment(`<panel
            class="panel-no-padding"
            orient="vertical"
            ignorekeys="true"
            noautofocus="true"
            flip="slide"
            type="arrow"
            position="${panel_position.panel_position_string}"
            ${hide_arrow ? "" : 'show-arrow=""'}
            ${autohide ? "" : 'noautohide="true"'}
            ${no_open_on_anchor ? 'no-open-on-anchor=""' : ""}
          />`);
        this._container = fragment.firstElementChild;
        this._setPanelMethods(this._container);
      } else {
        this._container = this.doc.createElement("div");
        this._container?.classList.add("hidden");
      }
      this._container.classList.add("featureCallout", "callout-arrow");
      if (hide_arrow) {
        this._container.setAttribute("hide-arrow", "permanent");
      } else {
        this._container.removeAttribute("hide-arrow");
      }
      this._container.id = CONTAINER_ID;
      this._container.setAttribute(
        "aria-describedby",
        `#${CONTAINER_ID} .welcome-text`
      );
      if (arrow_width) {
        this._container.style.setProperty("--arrow-width", `${arrow_width}px`);
      } else {
        this._container.style.removeProperty("--arrow-width");
      }
      if (padding) {
        this._container.style.setProperty("--callout-padding", `${padding}px`);
      } else {
        this._container.style.removeProperty("--callout-padding");
      }
      let contentBox = this.doc.createElement("div");
      contentBox.id = CONTENT_BOX_ID;
      contentBox.classList.add("onboardingContainer");
      // This value is reported as the "page" in about:welcome telemetry
      contentBox.dataset.page = this.location;
      this._applyTheme();
      if (needsPanel && this.win.isChromeWindow) {
        this.doc.getElementById("mainPopupSet").appendChild(this._container);
      } else {
        this.doc.body.prepend(this._container);
      }
      const makeArrow = classPrefix => {
        const arrowRotationBox = this.doc.createElement("div");
        arrowRotationBox.classList.add("arrow-box", `${classPrefix}-arrow-box`);
        const arrow = this.doc.createElement("div");
        arrow.classList.add("arrow", `${classPrefix}-arrow`);
        arrowRotationBox.appendChild(arrow);
        return arrowRotationBox;
      };
      this._container.appendChild(makeArrow("shadow"));
      this._container.appendChild(contentBox);
      this._container.appendChild(makeArrow("background"));
    }
    return this._container;
  }

  /** @see HTMLArrowPosition */
  _HTMLArrowPositions = [
    "top",
    "bottom",
    "end",
    "start",
    "top-end",
    "top-start",
    "top-center-arrow-end",
    "top-center-arrow-start",
  ];

  /**
   * Set callout's position relative to parent element
   */
  _positionCallout() {
    const container = this._container;
    const anchor = this._getAnchor();
    if (!container || !anchor) {
      this.endTour();
      return;
    }
    const parentEl = anchor.element;
    const { doc } = this;
    const arrowPosition = anchor.arrow_position || "top";
    const arrowWidth = anchor.arrow_width || 33.94113;
    const arrowHeight = arrowWidth / 2;
    const overlapAmount = 5;
    let overlap = overlapAmount - arrowHeight;
    // Is the document layout right to left?
    const RTL = this.doc.dir === "rtl";
    const customPosition = anchor.absolute_position;

    const getOffset = el => {
      const rect = el.getBoundingClientRect();
      return {
        left: rect.left + this.win.scrollX,
        right: rect.right + this.win.scrollX,
        top: rect.top + this.win.scrollY,
        bottom: rect.bottom + this.win.scrollY,
      };
    };

    const centerVertically = () => {
      let topOffset =
        (container.getBoundingClientRect().height -
          parentEl.getBoundingClientRect().height) /
        2;
      container.style.top = `${getOffset(parentEl).top - topOffset}px`;
    };

    /**
     * Horizontally align a top/bottom-positioned callout according to the
     * passed position.
     * @param {String} position one of...
     *   - "center": for use with top/bottom. arrow is in the center, and the
     *       center of the callout aligns with the parent center.
     *   - "center-arrow-start": for use with center-arrow-top-start. arrow is
     *       on the start (left) side of the callout, and the callout is aligned
     *       so that the arrow points to the center of the parent element.
     *   - "center-arrow-end": for use with center-arrow-top-end. arrow is on
     *       the end, and the arrow points to the center of the parent.
     *   - "start": currently unused. align the callout's starting edge with the
     *       parent's starting edge.
     *   - "end": currently unused. same as start but for the ending edge.
     */
    const alignHorizontally = position => {
      switch (position) {
        case "center": {
          const sideOffset =
            (parentEl.getBoundingClientRect().width -
              container.getBoundingClientRect().width) /
            2;
          const containerSide = RTL
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
        case "end":
        case "start": {
          const containerSide =
            RTL ^ (position === "end")
              ? parentEl.getBoundingClientRect().left +
                parentEl.getBoundingClientRect().width -
                container.getBoundingClientRect().width
              : parentEl.getBoundingClientRect().left;
          container.style.left = `${Math.max(containerSide, 0)}px`;
          break;
        }
        case "center-arrow-end":
        case "center-arrow-start": {
          const parentRect = parentEl.getBoundingClientRect();
          const containerWidth = container.getBoundingClientRect().width;
          const containerSide =
            RTL ^ position.endsWith("end")
              ? parentRect.left +
                parentRect.width / 2 +
                12 +
                arrowWidth / 2 -
                containerWidth
              : parentRect.left + parentRect.width / 2 - 12 - arrowWidth / 2;
          const maxContainerSide =
            doc.documentElement.clientWidth - containerWidth;
          container.style.left = `${Math.min(
            maxContainerSide,
            Math.max(containerSide, 0)
          )}px`;
        }
      }
    };

    // Remember not to use HTML-only properties/methods like offsetHeight. Try
    // to use getBoundingClientRect() instead, which is available on XUL
    // elements. This is necessary to support feature callout in chrome, which
    // is still largely XUL-based.
    const positioners = {
      // availableSpace should be the space between the edge of the page in the
      // assumed direction and the edge of the parent (with the callout being
      // intended to fit between those two edges) while needed space should be
      // the space necessary to fit the callout container.
      top: {
        availableSpace() {
          return (
            doc.documentElement.clientHeight -
            getOffset(parentEl).top -
            parentEl.getBoundingClientRect().height
          );
        },
        neededSpace: container.getBoundingClientRect().height - overlap,
        position() {
          // Point to an element above the callout
          let containerTop =
            getOffset(parentEl).top +
            parentEl.getBoundingClientRect().height -
            overlap;
          container.style.top = `${Math.max(0, containerTop)}px`;
          alignHorizontally("center");
        },
      },
      bottom: {
        availableSpace() {
          return getOffset(parentEl).top;
        },
        neededSpace: container.getBoundingClientRect().height - overlap,
        position() {
          // Point to an element below the callout
          let containerTop =
            getOffset(parentEl).top -
            container.getBoundingClientRect().height +
            overlap;
          container.style.top = `${Math.max(0, containerTop)}px`;
          alignHorizontally("center");
        },
      },
      right: {
        availableSpace() {
          return getOffset(parentEl).left;
        },
        neededSpace: container.getBoundingClientRect().width - overlap,
        position() {
          // Point to an element to the right of the callout
          let containerLeft =
            getOffset(parentEl).left -
            container.getBoundingClientRect().width +
            overlap;
          container.style.left = `${Math.max(0, containerLeft)}px`;
          if (
            container.getBoundingClientRect().height <=
            parentEl.getBoundingClientRect().height
          ) {
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
        neededSpace: container.getBoundingClientRect().width - overlap,
        position() {
          // Point to an element to the left of the callout
          let containerLeft =
            getOffset(parentEl).left +
            parentEl.getBoundingClientRect().width -
            overlap;
          container.style.left = `${Math.max(0, containerLeft)}px`;
          if (
            container.getBoundingClientRect().height <=
            parentEl.getBoundingClientRect().height
          ) {
            container.style.top = `${getOffset(parentEl).top}px`;
          } else {
            centerVertically();
          }
        },
      },
      "top-start": {
        availableSpace() {
          return (
            doc.documentElement.clientHeight -
            getOffset(parentEl).top -
            parentEl.getBoundingClientRect().height
          );
        },
        neededSpace: container.getBoundingClientRect().height - overlap,
        position() {
          // Point to an element above and at the start of the callout
          let containerTop =
            getOffset(parentEl).top +
            parentEl.getBoundingClientRect().height -
            overlap;
          container.style.top = `${Math.max(0, containerTop)}px`;
          alignHorizontally("start");
        },
      },
      "top-end": {
        availableSpace() {
          return (
            doc.documentElement.clientHeight -
            getOffset(parentEl).top -
            parentEl.getBoundingClientRect().height
          );
        },
        neededSpace: container.getBoundingClientRect().height - overlap,
        position() {
          // Point to an element above and at the end of the callout
          let containerTop =
            getOffset(parentEl).top +
            parentEl.getBoundingClientRect().height -
            overlap;
          container.style.top = `${Math.max(0, containerTop)}px`;
          alignHorizontally("end");
        },
      },
      "top-center-arrow-start": {
        availableSpace() {
          return (
            doc.documentElement.clientHeight -
            getOffset(parentEl).top -
            parentEl.getBoundingClientRect().height
          );
        },
        neededSpace: container.getBoundingClientRect().height - overlap,
        position() {
          // Point to an element above and at the start of the callout
          let containerTop =
            getOffset(parentEl).top +
            parentEl.getBoundingClientRect().height -
            overlap;
          container.style.top = `${Math.max(0, containerTop)}px`;
          alignHorizontally("center-arrow-start");
        },
      },
      "top-center-arrow-end": {
        availableSpace() {
          return (
            doc.documentElement.clientHeight -
            getOffset(parentEl).top -
            parentEl.getBoundingClientRect().height
          );
        },
        neededSpace: container.getBoundingClientRect().height - overlap,
        position() {
          // Point to an element above and at the end of the callout
          let containerTop =
            getOffset(parentEl).top +
            parentEl.getBoundingClientRect().height -
            overlap;
          container.style.top = `${Math.max(0, containerTop)}px`;
          alignHorizontally("center-arrow-end");
        },
      },
    };

    const clearPosition = () => {
      Object.keys(positioners).forEach(position => {
        container.style[position] = "unset";
      });
      container.removeAttribute("arrow-position");
    };

    const setArrowPosition = position => {
      let val;
      switch (position) {
        case "bottom":
          val = "bottom";
          break;
        case "left":
          val = "inline-start";
          break;
        case "right":
          val = "inline-end";
          break;
        case "top-start":
        case "top-center-arrow-start":
          val = RTL ? "top-end" : "top-start";
          break;
        case "top-end":
        case "top-center-arrow-end":
          val = RTL ? "top-start" : "top-end";
          break;
        case "top":
        default:
          val = "top";
          break;
      }

      container.setAttribute("arrow-position", val);
    };

    const addValueToPixelValue = (value, pixelValue) => {
      return `${parseFloat(pixelValue) + value}px`;
    };

    const subtractPixelValueFromValue = (pixelValue, value) => {
      return `${value - parseFloat(pixelValue)}px`;
    };

    const overridePosition = () => {
      // We override _every_ positioner here, because we want to manually set
      // all container.style.positions in every positioner's "position" function
      // regardless of the actual arrow position

      // Note: We override the position functions with new functions here, but
      // they don't actually get executed until the respective position
      // functions are called and this function is not executed unless the
      // message has a custom position property.

      // We're positioning relative to a parent element's bounds, if that parent
      // element exists.

      for (const position in positioners) {
        if (!Object.prototype.hasOwnProperty.call(positioners, position)) {
          continue;
        }

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

            if (RTL) {
              container.style.right = leftPosition;
            } else {
              container.style.left = leftPosition;
            }
          }

          if (customPosition.right) {
            const rightPosition = subtractPixelValueFromValue(
              customPosition.right,
              parentEl.getBoundingClientRect().right -
                container.getBoundingClientRect().width
            );

            if (RTL) {
              container.style.right = rightPosition;
            } else {
              container.style.left = rightPosition;
            }
          }

          if (customPosition.bottom) {
            container.style.top = subtractPixelValueFromValue(
              customPosition.bottom,
              parentEl.getBoundingClientRect().bottom -
                container.getBoundingClientRect().height
            );
          }
        };
      }
    };

    const calloutFits = position => {
      // Does callout element fit in this position relative
      // to the parent element without going off screen?

      // Only consider which edge of the callout the arrow points from,
      // not the alignment of the arrow along the edge of the callout
      let [edgePosition] = position.split("-");
      return (
        positioners[edgePosition].availableSpace() >
        positioners[edgePosition].neededSpace
      );
    };

    const choosePosition = () => {
      let position = arrowPosition;
      if (!this._HTMLArrowPositions.includes(position)) {
        // Configured arrow position is not valid
        position = null;
      }
      if (["start", "end"].includes(position)) {
        // position here is referencing the direction that the callout container
        // is pointing to, and therefore should be the _opposite_ side of the
        // arrow eg. if arrow is at the "end" in LTR layouts, the container is
        // pointing at an element to the right of itself, while in RTL layouts
        // it is pointing to the left of itself
        position = RTL ^ (position === "start") ? "left" : "right";
      }
      // If we're overriding the position, we don't need to sort for available space
      if (customPosition || (position && calloutFits(position))) {
        return position;
      }
      let sortedPositions = ["top", "bottom", "left", "right"]
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

    clearPosition(container);

    if (customPosition) {
      overridePosition();
    }

    let finalPosition = choosePosition();
    if (finalPosition) {
      positioners[finalPosition].position();
      setArrowPosition(finalPosition);
    }

    container.classList.remove("hidden");
  }

  /** Expose top level functions expected by the aboutwelcome bundle. */
  _setupWindowFunctions() {
    if (this.AWSetup) {
      return;
    }

    const handleActorMessage =
      lazy.AboutWelcomeParent.prototype.onContentMessage.bind({});
    const getActionHandler = name => data =>
      handleActorMessage(`AWPage:${name}`, data, this.doc);

    const telemetryMessageHandler = getActionHandler("TELEMETRY_EVENT");
    const AWSendEventTelemetry = data => {
      if (this.config?.metrics !== "block") {
        return telemetryMessageHandler(data);
      }
      return null;
    };
    this._windowFuncs = {
      AWGetFeatureConfig: () => this.config,
      AWGetSelectedTheme: getActionHandler("GET_SELECTED_THEME"),
      // Do not send telemetry if message config sets metrics as 'block'.
      AWSendEventTelemetry,
      AWSendToDeviceEmailsSupported: getActionHandler(
        "SEND_TO_DEVICE_EMAILS_SUPPORTED"
      ),
      AWSendToParent: (name, data) => getActionHandler(name)(data),
      AWFinish: () => this.endTour(),
      AWEvaluateScreenTargeting: getActionHandler("EVALUATE_SCREEN_TARGETING"),
    };
    for (const [name, func] of Object.entries(this._windowFuncs)) {
      this.win[name] = func;
    }

    this.AWSetup = true;
  }

  /** Clean up the functions defined above. */
  _clearWindowFunctions() {
    if (this.AWSetup) {
      this.AWSetup = false;

      for (const name of Object.keys(this._windowFuncs)) {
        delete this.win[name];
      }
    }
  }

  /**
   * Emit an event to the broker, if one is present.
   * @param {String} name
   * @param {any} data
   */
  _emitEvent(name, data) {
    this.listener?.(this.win, name, data);
  }

  endTour(skipFadeOut = false) {
    // We don't want focus events that happen during teardown to affect
    // this.savedFocus
    this.win.removeEventListener("focus", this, {
      capture: true,
      passive: true,
    });
    this.win.removeEventListener("keypress", this, { capture: true });
    this._pageEventManager?.emit({
      type: "tourend",
      target: this._container,
    });
    this._container?.removeEventListener("popuphiding", this);
    this._pageEventManager?.clear();

    // Delete almost everything to get this ready to show a different message.
    this.teardownFeatureTourProgress();
    this.pref = null;
    this.ready = false;
    this.message = null;
    this.content = null;
    this.currentScreen = null;
    // wait for fade out transition
    this._container?.classList.toggle(
      "hidden",
      this._container?.localName !== "panel"
    );
    this._clearWindowFunctions();
    const onFadeOut = () => {
      this._container?.remove();
      this.renderObserver?.disconnect();
      this._removePositionListeners();
      this._removePanelConflictListeners();
      this.doc.querySelector(`[src="${BUNDLE_SRC}"]`)?.remove();
      // Put the focus back to the last place the user focused outside of the
      // featureCallout windows.
      if (this.savedFocus) {
        this.savedFocus.element.focus({
          focusVisible: this.savedFocus.focusVisible,
        });
      }
      this.savedFocus = null;
      this._emitEvent("end");
    };
    if (this._container?.localName === "panel") {
      this._container.addEventListener("popuphidden", onFadeOut, {
        once: true,
      });
      this._container.hidePopup(!skipFadeOut);
    } else if (this._container) {
      this.win.setTimeout(onFadeOut, skipFadeOut ? 0 : TRANSITION_MS);
    } else {
      onFadeOut();
    }
  }

  _dismiss() {
    let action = this.currentScreen?.content.dismiss_button?.action;
    if (action?.type) {
      this.win.AWSendToParent("SPECIAL_ACTION", action);
      if (!action.dismiss) {
        return;
      }
    }
    this.endTour();
  }

  async _addScriptsAndRender() {
    const reactSrc = "resource://activity-stream/vendor/react.js";
    const domSrc = "resource://activity-stream/vendor/react-dom.js";
    // Add React script
    const getReactReady = () => {
      return new Promise(resolve => {
        let reactScript = this.doc.createElement("script");
        reactScript.src = reactSrc;
        this.doc.head.appendChild(reactScript);
        reactScript.addEventListener("load", resolve);
      });
    };
    // Add ReactDom script
    const getDomReady = () => {
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
   * Update the internal config with a new message. If a message is not
   * provided, try requesting one from ASRouter. The message content is stored
   * in this.config, which is returned by AWGetFeatureConfig. The aboutwelcome
   * bundle will use that function to get the content when it executes.
   * @param {Object} [message] ASRouter message. Omit to request a new one.
   * @returns {Promise<boolean>} true if a message is loaded, false if not.
   */
  async _updateConfig(message) {
    if (this.loadingConfig) {
      return false;
    }

    this.message = message || (await this._loadConfig());

    switch (this.message.template) {
      case "feature_callout":
        break;
      case "spotlight":
        // Special handling for spotlight messages, which can be configured as a
        // kind of introduction to a feature tour.
        this.currentScreen = "spotlight";
      // fall through
      default:
        return false;
    }

    this.config = this.message.content;

    // Set the default start screen.
    let newScreen = this.config?.screens?.[this.config?.startScreen || 0];
    // If we have a feature tour in progress, try to set the start screen to
    // whichever screen is configured in the feature tour pref.
    if (
      this.config.screens &&
      this.config?.tour_pref_name &&
      this.config.tour_pref_name === this.pref?.name &&
      this.featureTourProgress
    ) {
      const newIndex = this.config.screens.findIndex(
        screen => screen.id === this.featureTourProgress.screen
      );
      if (newIndex !== -1) {
        newScreen = this.config.screens[newIndex];
        if (newScreen?.id !== this.currentScreen?.id) {
          // This is how we tell the bundle to render the correct screen.
          this.config.startScreen = newIndex;
        }
      }
    }
    if (newScreen?.id === this.currentScreen?.id) {
      return false;
    }

    this.currentScreen = newScreen;
    return true;
  }

  /**
   * Request a message from ASRouter, targeting the `browser` and `page` values
   * passed to the constructor.
   * @returns {Promise<Object>} the requested message.
   */
  async _loadConfig() {
    this.loadingConfig = true;
    await lazy.ASRouter.waitForInitialized;
    let result = await lazy.ASRouter.sendTriggerMessage({
      browser: this.browser,
      // triggerId and triggerContext
      id: "featureCalloutCheck",
      context: { source: this.location },
    });
    this.loadingConfig = false;
    return result.message;
  }

  /**
   * Try to render the callout in the current document.
   * @returns {Promise<Boolean>} whether the callout was rendered.
   */
  async _renderCallout() {
    this._setupWindowFunctions();
    await this._addCalloutLinkElements();
    let container = this._createContainer();
    if (container) {
      // This results in rendering the Feature Callout
      await this._addScriptsAndRender();
      this._observeRender(container.querySelector(`#${CONTENT_BOX_ID}`));
      if (container.localName === "div") {
        this._addPositionListeners();
      }
      return true;
    }
    return false;
  }

  /**
   * For each member of the screen's page_event_listeners array, add a listener.
   * @param {Array<PageEventListenerConfig>} listeners
   *
   * @typedef {Object} PageEventListenerConfig
   * @property {PageEventListenerParams} params Event listener parameters
   * @property {PageEventListenerAction} action Sent when the event fires
   *
   * @typedef {Object} PageEventListenerParams See PageEventManager.sys.mjs
   * @property {String} type Event type string e.g. `click`
   * @property {String} [selectors] Target selector, e.g. `tag.class, #id[attr]`
   * @property {PageEventListenerOptions} [options] addEventListener options
   *
   * @typedef {Object} PageEventListenerOptions
   * @property {Boolean} [capture] Use event capturing phase?
   * @property {Boolean} [once] Remove listener after first event?
   * @property {Boolean} [preventDefault] Prevent default action?
   * @property {Number} [interval] Used only for `timeout` and `interval` event
   *   types. These don't set up real event listeners, but instead invoke the
   *   action on a timer.
   *
   * @typedef {Object} PageEventListenerAction Action sent to AboutWelcomeParent
   * @property {String} [type] Action type, e.g. `OPEN_URL`
   * @property {Object} [data] Extra data, properties depend on action type
   * @property {Boolean} [dismiss] Dismiss screen after performing action?
   * @property {Boolean} [reposition] Reposition screen after performing action?
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
    const page = this.location;
    const message_id = this.config?.id.toUpperCase();
    const source =
      typeof event.target === "string"
        ? event.target
        : this._getUniqueElementIdentifier(event.target);
    if (action.type) {
      this.win.AWSendEventTelemetry?.({
        event: "PAGE_EVENT",
        event_context: {
          action: action.type,
          reason: event.type?.toUpperCase(),
          source,
          page,
        },
        message_id,
      });
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
    if (action.reposition) {
      this.win.requestAnimationFrame(() => this._positionCallout());
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

  /**
   * Get the element that should be initially focused. Prioritize the primary
   * button, then the secondary button, then any additional button, excluding
   * pseudo-links and the dismiss button. If no button is found, focus the first
   * input element. If no affirmative action is found, focus the first button,
   * which is probably the dismiss button. If no button is found, focus the
   * container itself.
   * @returns {Element|null} The element to focus when the callout is shown.
   */
  getInitialFocus() {
    if (!this._container) {
      return null;
    }
    return (
      this._container.querySelector(
        ".primary:not(:disabled, [hidden], .text-link, .cta-link, .split-button)"
      ) ||
      this._container.querySelector(
        ".secondary:not(:disabled, [hidden], .text-link, .cta-link, .split-button)"
      ) ||
      this._container.querySelector(
        "button:not(:disabled, [hidden], .text-link, .cta-link, .dismiss-button, .split-button)"
      ) ||
      this._container.querySelector("input:not(:disabled, [hidden])") ||
      this._container.querySelector(
        "button:not(:disabled, [hidden], .text-link, .cta-link)"
      )
    );
  }

  /**
   * Show a feature callout message, either by requesting one from ASRouter or
   * by showing a message passed as an argument.
   * @param {Object} [message] optional message to show instead of requesting one
   * @returns {Promise<Boolean>} true if a message was shown
   */
  async showFeatureCallout(message) {
    let updated = await this._updateConfig(message);

    if (!updated || !this.config?.screens?.length) {
      return !!this.currentScreen;
    }

    if (!this.renderObserver) {
      this.renderObserver = new this.win.MutationObserver(() => {
        // Check if the Feature Callout screen has loaded for the first time
        if (!this.ready && this._container.querySelector(".screen")) {
          const onRender = () => {
            this.ready = true;
            this._pageEventManager?.clear();
            this._attachPageEventListeners(
              this.currentScreen?.content?.page_event_listeners
            );
            this.getInitialFocus()?.focus();
            this.win.addEventListener("keypress", this, { capture: true });
            if (this._container.localName === "div") {
              this.win.addEventListener("focus", this, {
                capture: true, // get the event before retargeting
                passive: true,
              });
              this._positionCallout();
            } else {
              this._container.classList.remove("hidden");
            }
          };
          if (
            this._container.localName === "div" &&
            this.doc.activeElement &&
            !this.savedFocus
          ) {
            let element = this.doc.activeElement;
            this.savedFocus = {
              element,
              focusVisible: element.matches(":focus-visible"),
            };
          }
          // Once the screen element is added to the DOM, wait for the
          // animation frame after next to ensure that _positionCallout
          // has access to the rendered screen with the correct height
          if (this._container.localName === "div") {
            this.win.requestAnimationFrame(() => {
              this.win.requestAnimationFrame(onRender);
            });
          } else if (this._container.localName === "panel") {
            const anchor = this._getAnchor();
            if (!anchor?.panel_position) {
              this.endTour();
              return;
            }
            const {
              panel_position_string: position,
              offset_x: x,
              offset_y: y,
            } = anchor.panel_position;
            this._container.addEventListener("popupshown", onRender, {
              once: true,
            });
            this._container.addEventListener("popuphiding", this);
            this._addPanelConflictListeners();
            this._container.openPopup(anchor.element, { position, x, y });
          }
        }
      });
    }

    this._pageEventManager?.clear();
    this.ready = false;
    this._container?.remove();
    this.renderObserver?.disconnect();

    if (!this.cfrFeaturesUserPref) {
      this.endTour();
      return false;
    }

    let rendering = (await this._renderCallout()) && !!this.currentScreen;
    if (!rendering) {
      this.endTour();
    }

    if (this.message.template) {
      lazy.ASRouter.addImpression(this.message);
    }
    return rendering;
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
        !!this.theme.simulateContent
      );
      this._container.classList.toggle(
        "lwtNewtab",
        !!(
          this.theme.lwtNewtab !== false &&
          this.theme.simulateContent &&
          ["themed-content", "newtab"].includes(this.theme.preset)
        )
      );
      for (const type of ["light", "dark", "hcm"]) {
        const scheme = this.theme[type];
        for (const name of FeatureCallout.themePropNames) {
          this._setThemeVariable(
            `--fc-${name}-${type}`,
            scheme?.[name] || this.theme.all?.[name]
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
    "primary-button-background",
    "primary-button-color",
    "primary-button-border",
    "primary-button-background-hover",
    "primary-button-color-hover",
    "primary-button-border-hover",
    "primary-button-background-active",
    "primary-button-color-active",
    "primary-button-border-active",
    "link-color",
    "link-color-hover",
    "link-color-active",
  ];

  /** @type {Object<String, FeatureCalloutTheme>} */
  static themePresets = {
    // For themed system pages like New Tab and Firefox View. Themed content
    // colors inherit from the user's theme through contentTheme.js.
    "themed-content": {
      all: {
        background:
          "var(--newtab-background-color, var(--in-content-page-background)) linear-gradient(var(--newtab-background-color-secondary), var(--newtab-background-color-secondary))",
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
        "primary-button-background":
          "var(--in-content-primary-button-background)",
        "primary-button-color": "var(--in-content-primary-button-text-color)",
        "primary-button-border":
          "var(--in-content-primary-button-border-color)",
        "primary-button-background-hover":
          "var(--in-content-primary-button-background-hover)",
        "primary-button-color-hover":
          "var(--in-content-primary-button-text-color-hover)",
        "primary-button-border-hover":
          "var(--in-content-primary-button-border-hover)",
        "primary-button-background-active":
          "var(--in-content-primary-button-background-active)",
        "primary-button-color-active":
          "var(--in-content-primary-button-text-color-active)",
        "primary-button-border-active":
          "var(--in-content-primary-button-border-active)",
        "link-color": "LinkText",
        "link-color-hover": "LinkText",
        "link-color-active": "ActiveText",
        "link-color-visited": "VisitedText",
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
        // use default primary button colors in _feature-callout-theme.scss
        "link-color": "LinkText",
        "link-color-hover": "LinkText",
        "link-color-active": "ActiveText",
        "link-color-visited": "VisitedText",
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
    newtab: {
      all: {
        background:
          "var(--newtab-background-color, #F9F9FB) linear-gradient(var(--newtab-background-color-secondary, #FFF), var(--newtab-background-color-secondary, #FFF))",
        color: "var(--newtab-text-primary-color, WindowText)",
        border:
          "color-mix(in srgb, var(--newtab-background-color-secondary, #FFF) 80%, #000)",
        "accent-color": "#0061e0",
        "button-background": "color-mix(in srgb, transparent 93%, #000)",
        "button-color": "var(--newtab-text-primary-color, WindowText)",
        "button-border": "transparent",
        "button-background-hover": "color-mix(in srgb, transparent 88%, #000)",
        "button-color-hover": "var(--newtab-text-primary-color, WindowText)",
        "button-border-hover": "transparent",
        "button-background-active": "color-mix(in srgb, transparent 80%, #000)",
        "button-color-active": "var(--newtab-text-primary-color, WindowText)",
        "button-border-active": "transparent",
        // use default primary button colors in _feature-callout-theme.scss
        "link-color": "rgb(0, 97, 224)",
        "link-color-hover": "rgb(0, 97, 224)",
        "link-color-active": "color-mix(in srgb, rgb(0, 97, 224) 80%, #000)",
        "link-color-visited": "rgb(0, 97, 224)",
      },
      dark: {
        "accent-color": "rgb(0, 221, 255)",
        background:
          "var(--newtab-background-color, #2B2A33) linear-gradient(var(--newtab-background-color-secondary, #42414D), var(--newtab-background-color-secondary, #42414D))",
        border:
          "color-mix(in srgb, var(--newtab-background-color-secondary, #42414D) 80%, #FFF)",
        "button-background": "color-mix(in srgb, transparent 80%, #000)",
        "button-background-hover": "color-mix(in srgb, transparent 65%, #000)",
        "button-background-active": "color-mix(in srgb, transparent 55%, #000)",
        "link-color": "rgb(0, 221, 255)",
        "link-color-hover": "rgb(0,221,255)",
        "link-color-active": "color-mix(in srgb, rgb(0, 221, 255) 60%, #FFF)",
        "link-color-visited": "rgb(0, 221, 255)",
      },
      hcm: {
        background: "-moz-dialog",
        color: "-moz-dialogtext",
        border: "-moz-dialogtext",
        "accent-color": "SelectedItem",
        "button-background": "ButtonFace",
        "button-color": "ButtonText",
        "button-border": "ButtonText",
        "button-background-hover": "ButtonText",
        "button-color-hover": "ButtonFace",
        "button-border-hover": "ButtonText",
        "button-background-active": "ButtonText",
        "button-color-active": "ButtonFace",
        "button-border-active": "ButtonText",
        "link-color": "LinkText",
        "link-color-hover": "LinkText",
        "link-color-active": "ActiveText",
        "link-color-visited": "VisitedText",
      },
    },
    // These colors are intended to inherit the user's theme properties from the
    // main chrome window, for callouts to be anchored to chrome elements.
    // Specific schemes aren't necessary since the theme and frontend
    // stylesheets handle these variables' values.
    chrome: {
      all: {
        // Use a gradient because it's possible (due to custom themes) that the
        // arrowpanel-background will be semi-transparent, causing the arrow to
        // show through the callout background. Put the Menu color behind the
        // arrowpanel-background.
        background:
          "Menu linear-gradient(var(--arrowpanel-background), var(--arrowpanel-background))",
        color: "var(--arrowpanel-color)",
        border: "var(--arrowpanel-border-color)",
        "accent-color": "var(--focus-outline-color)",
        "button-background": "var(--button-bgcolor)",
        "button-color": "var(--button-color)",
        "button-border": "transparent",
        "button-background-hover": "var(--button-hover-bgcolor)",
        "button-color-hover": "var(--button-color)",
        "button-border-hover": "transparent",
        "button-background-active": "var(--button-active-bgcolor)",
        "button-color-active": "var(--button-color)",
        "button-border-active": "transparent",
        "primary-button-background": "var(--button-primary-bgcolor)",
        "primary-button-color": "var(--button-primary-color)",
        "primary-button-border": "transparent",
        "primary-button-background-hover":
          "var(--button-primary-hover-bgcolor)",
        "primary-button-color-hover": "var(--button-primary-color)",
        "primary-button-border-hover": "transparent",
        "primary-button-background-active":
          "var(--button-primary-active-bgcolor)",
        "primary-button-color-active": "var(--button-primary-color)",
        "primary-button-border-active": "transparent",
        "link-color": "LinkText",
        "link-color-hover": "LinkText",
        "link-color-active": "ActiveText",
        "link-color-visited": "VisitedText",
      },
      hcm: {
        background: "var(--arrowpanel-background)",
      },
    },
  };
}
