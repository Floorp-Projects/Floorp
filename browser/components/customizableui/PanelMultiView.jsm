/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["PanelMultiView"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AppConstants",
  "resource://gre/modules/AppConstants.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "BrowserUtils",
  "resource://gre/modules/BrowserUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "CustomizableUI",
  "resource:///modules/CustomizableUI.jsm");

const TRANSITION_PHASES = Object.freeze({
  START: 1,
  PREPARE: 2,
  TRANSITION: 3,
  END: 4
});

/**
 * Simple implementation of the sliding window pattern; panels are added to a
 * linked list, in-order, and the currently shown panel is remembered using a
 * marker. The marker shifts as navigation between panels is continued, where
 * the panel at index 0 is always the starting point:
 *           ┌────┬────┬────┬────┐
 *           │▓▓▓▓│    │    │    │ Start
 *           └────┴────┴────┴────┘
 *      ┌────┬────┬────┬────┐
 *      │    │▓▓▓▓│    │    │      Forward
 *      └────┴────┴────┴────┘
 * ┌────┬────┬────┬────┐
 * │    │    │▓▓▓▓│    │           Forward
 * └────┴────┴────┴────┘
 *      ┌────┬────┬────┬────┐
 *      │    │▓▓▓▓│    │    │      Back
 *      └────┴────┴────┴────┘
 */
class SlidingPanelViews extends Array {
  constructor() {
    super();
    this._marker = 0;
  }

  /**
   * Get the index that points to the currently selected view.
   *
   * @return {Number}
   */
  get current() {
    return this._marker;
  }

  /**
   * Setter for the current index, which changes the order of elements and
   * updates the internal marker for the currently selected view.
   * We're manipulating the array directly to have it reflect the order of
   * navigation, instead of continuously growing the array with the next selected
   * view to keep memory usage within reasonable proportions. With this method,
   * the data structure grows no larger than the number of panels inside the
   * panelMultiView.
   *
   * @param  {Number} index Index of the item to move to the current position.
   * @return {Number} The new marker index.
   */
  set current(index) {
    if (index == this._marker) {
      // Never change a winning team.
      return index;
    }
    if (index == -1 || index > (this.length - 1)) {
      throw new Error(`SlidingPanelViews :: index ${index} out of bounds`);
    }

    let view = this.splice(index, 1)[0];
    if (this._marker > index) {
      // Correct the current marker if the view-to-select was removed somewhere
      // before it.
      --this._marker;
    }
    // Then add the view-to-select right after the currently selected view.
    this.splice(++this._marker, 0, view);
    return this._marker;
  }

  /**
   * Getter for the currently selected view node.
   *
   * @return {panelview}
   */
  get currentView() {
    return this[this._marker];
  }

  /**
   * Setter for the currently selected view node.
   *
   * @param  {panelview} view
   * @return {Number} Index of the currently selected view.
   */
  set currentView(view) {
    if (!view)
      return this.current;
    // This will throw an error if the view could not be found.
    return this.current = this.indexOf(view);
  }

  /**
   * Getter for the previous view, which is always positioned one position after
   * the current view.
   *
   * @return {panelview}
   */
  get previousView() {
    return this[this._marker + 1];
  }

  /**
   * Going back is an explicit action on the data structure, moving the marker
   * one step back.
   *
   * @return {Array} A list of two items: the newly selected view and the previous one.
   */
  back() {
    if (this._marker > 0)
      --this._marker;
    return [this.currentView, this.previousView];
  }

  /**
   * Reset the data structure to its original construct, removing all references
   * to view nodes.
   */
  clear() {
    this._marker = 0;
    this.splice(0, this.length);
  }
}

/**
 * This is the implementation of the panelUI.xml XBL binding, moved to this
 * module, to make it easier to fork the logic for the newer photon structure.
 * Goals are:
 * 1. to make it easier to programmatically extend the list of panels,
 * 2. allow for navigation between panels multiple levels deep and
 * 3. maintain the pre-photon structure with as little effort possible.
 *
 * @type {PanelMultiView}
 */
this.PanelMultiView = class {
  get document() {
    return this.node.ownerDocument;
  }

  get window() {
    return this.node.ownerGlobal;
  }

  get _panel() {
    return this.node.parentNode;
  }

  get showingSubView() {
    return this._showingSubView;
  }
  get _mainViewId() {
    return this.node.getAttribute("mainViewId");
  }
  set _mainViewId(val) {
    this.node.setAttribute("mainViewId", val);
    return val;
  }
  get _mainView() {
    return this._mainViewId ? this.document.getElementById(this._mainViewId) : null;
  }

  get _transitioning() {
    return this.__transitioning;
  }
  set _transitioning(val) {
    this.__transitioning = val;
    if (val) {
      this.node.setAttribute("transitioning", "true");
    } else {
      this.node.removeAttribute("transitioning");
    }
  }

  /**
   * @return {Boolean} |true| when the 'ephemeral' attribute is set, which means
   *                   that this instance should be ready to be thrown away at
   *                   any time.
   */
  get _ephemeral() {
    return this.node.hasAttribute("ephemeral");
  }

  get panelViews() {
    if (this._panelViews)
      return this._panelViews;

    this._panelViews = new SlidingPanelViews();
    this._panelViews.push(...this.node.getElementsByTagName("panelview"));
    return this._panelViews;
  }
  get _dwu() {
    if (this.__dwu)
      return this.__dwu;
    return this.__dwu = this.window.QueryInterface(Ci.nsIInterfaceRequestor)
                                   .getInterface(Ci.nsIDOMWindowUtils);
  }
  get _screenManager() {
    if (this.__screenManager)
      return this.__screenManager;
    return this.__screenManager = Cc["@mozilla.org/gfx/screenmanager;1"]
                                    .getService(Ci.nsIScreenManager);
  }
  /**
   * @return {panelview} the currently visible subview OR the subview that is
   *                     about to be shown whilst a 'ViewShowing' event is being
   *                     dispatched.
   */
  get current() {
    return this._viewShowing || this._currentSubView;
  }
  get _currentSubView() {
    return this.panelViews.currentView;
  }
  set _currentSubView(panel) {
    this.panelViews.currentView = panel;
  }
  /**
   * @return {Promise} showSubView() returns a promise, which is kept here for
   *                   random access.
   */
  get currentShowPromise() {
    return this._currentShowPromise || Promise.resolve();
  }
  get _keyNavigationMap() {
    if (!this.__keyNavigationMap)
      this.__keyNavigationMap = new Map();
    return this.__keyNavigationMap;
  }
  get _multiLineElementsMap() {
    if (!this.__multiLineElementsMap)
      this.__multiLineElementsMap = new WeakMap();
    return this.__multiLineElementsMap;
  }

  constructor(xulNode, testMode = false) {
    this.node = xulNode;
    // If `testMode` is `true`, the consumer is only interested in accessing the
    // methods of this instance. (E.g. in unit tests.)
    if (testMode)
      return;

    this._currentSubView = this._subViewObserver = null;
    this._mainViewHeight = 0;
    this.__transitioning = this._ignoreMutations = this._showingSubView = false;

    const {document, window} = this;

    this._viewContainer =
      document.getAnonymousElementByAttribute(this.node, "anonid", "viewContainer");
    this._viewStack =
      document.getAnonymousElementByAttribute(this.node, "anonid", "viewStack");
    this._offscreenViewStack =
      document.getAnonymousElementByAttribute(this.node, "anonid", "offscreenViewStack");

    XPCOMUtils.defineLazyGetter(this, "_panelViewCache", () => {
      let viewCacheId = this.node.getAttribute("viewCacheId");
      return viewCacheId ? document.getElementById(viewCacheId) : null;
    });

    this._panel.addEventListener("popupshowing", this);
    this._panel.addEventListener("popuppositioned", this);
    this._panel.addEventListener("popuphidden", this);
    this._panel.addEventListener("popupshown", this);
    let cs = window.getComputedStyle(document.documentElement);
    // Set CSS-determined attributes now to prevent a layout flush when we do
    // it when transitioning between panels.
    this._dir = cs.direction;
    this.setMainView(this.panelViews.currentView);
    this.showMainView();

    this._showingSubView = false;

    // Proxy these public properties and methods, as used elsewhere by various
    // parts of the browser, to this instance.
    ["_mainView", "ignoreMutations", "showingSubView",
     "_panelViews"].forEach(property => {
      Object.defineProperty(this.node, property, {
        enumerable: true,
        get: () => this[property],
        set: (val) => this[property] = val
      });
    });
    ["goBack", "descriptionHeightWorkaround", "setMainView", "showMainView",
     "showSubView"].forEach(method => {
      Object.defineProperty(this.node, method, {
        enumerable: true,
        value: (...args) => this[method](...args)
      });
    });
    ["current", "currentShowPromise"].forEach(property => {
      Object.defineProperty(this.node, property, {
        enumerable: true,
        get: () => this[property]
      });
    });
  }

  destructor() {
    // Guard against re-entrancy.
    if (!this.node)
      return;

    this._cleanupTransitionPhase();
    if (this._ephemeral)
      this.hideAllViewsExcept(null);
    let mainView = this._mainView;
    if (mainView) {
      if (this._panelViewCache)
        this._panelViewCache.appendChild(mainView);
      mainView.removeAttribute("mainview");
    }

    this._moveOutKids(this._viewStack);
    this.panelViews.clear();
    this._panel.removeEventListener("mousemove", this);
    this._panel.removeEventListener("popupshowing", this);
    this._panel.removeEventListener("popuppositioned", this);
    this._panel.removeEventListener("popupshown", this);
    this._panel.removeEventListener("popuphidden", this);
    this.window.removeEventListener("keydown", this);
    this.node = this._viewContainer = this._viewStack = this.__dwu =
      this._panelViewCache = this._transitionDetails = null;
  }

  /**
   * Remove any child subviews into the panelViewCache, to ensure
   * they remain usable even if this panelmultiview instance is removed
   * from the DOM.
   * @param viewNodeContainer the container from which to remove subviews
   */
  _moveOutKids(viewNodeContainer) {
    if (!this._panelViewCache)
      return;

    // Node.children and Node.childNodes is live to DOM changes like the
    // ones we're about to do, so iterate over a static copy:
    let subviews = Array.from(viewNodeContainer.childNodes);
    for (let subview of subviews) {
      // XBL lists the 'children' XBL element explicitly. :-(
      if (subview.nodeName != "children")
        this._panelViewCache.appendChild(subview);
    }
  }

  _placeSubView(viewNode) {
    this._viewStack.appendChild(viewNode);
    if (!this.panelViews.includes(viewNode))
      this.panelViews.push(viewNode);
  }

  _setHeader(viewNode, titleText) {
    // If the header already exists, update or remove it as requested.
    let header = viewNode.firstChild;
    if (header && header.classList.contains("panel-header")) {
      if (titleText) {
        header.querySelector("label").setAttribute("value", titleText);
      } else {
        header.remove();
      }
      return;
    }

    // The header doesn't exist, only create it if needed.
    if (!titleText) {
      return;
    }

    header = this.document.createElement("box");
    header.classList.add("panel-header");

    let backButton = this.document.createElement("toolbarbutton");
    backButton.className =
      "subviewbutton subviewbutton-iconic subviewbutton-back";
    backButton.setAttribute("closemenu", "none");
    backButton.setAttribute("tabindex", "0");
    backButton.setAttribute("tooltip",
      this.node.getAttribute("data-subviewbutton-tooltip"));
    backButton.addEventListener("command", () => {
      // The panelmultiview element may change if the view is reused.
      viewNode.panelMultiView.goBack();
      backButton.blur();
    });

    let label = this.document.createElement("label");
    label.setAttribute("value", titleText);

    header.append(backButton, label);
    viewNode.prepend(header);
  }

  goBack() {
    let [current, previous] = this.panelViews.back();
    return this.showSubView(current, null, previous);
  }

  /**
   * Checks whether it is possible to navigate backwards currently. Returns
   * false if this is the panelmultiview's mainview, true otherwise.
   *
   * @param  {panelview} view View to check, defaults to the currently active view.
   * @return {Boolean}
   */
  _canGoBack(view = this._currentSubView) {
    return view.id != this._mainViewId;
  }

  setMainView(aNewMainView) {
    if (!aNewMainView)
      return;

    if (this._mainView) {
      this._mainView.removeAttribute("mainview");
    }
    this._mainViewId = aNewMainView.id;
    aNewMainView.setAttribute("mainview", "true");
    // If the new main view is not yet in the zeroth position, make sure it's
    // inserted there.
    if (aNewMainView.parentNode != this._viewStack &&
        this._viewStack.firstChild != aNewMainView) {
      this._viewStack.insertBefore(aNewMainView, this._viewStack.firstChild);
    }
  }

  showMainView() {
    if (!this._mainViewId)
      return Promise.resolve();

    return this.showSubView(this._mainView);
  }

  /**
   * Ensures that all the panelviews, that are currently part of this instance,
   * are hidden, except one specifically.
   *
   * @param {panelview} [theOne] The panelview DOM node to ensure is visible.
   *                             Optional.
   */
  hideAllViewsExcept(theOne = null) {
    for (let panelview of this._panelViews) {
      // When the panelview was already reparented, don't interfere any more.
      if (panelview == theOne || !this.node || panelview.panelMultiView != this.node)
        continue;
      if (panelview.hasAttribute("current"))
        this._dispatchViewEvent(panelview, "ViewHiding");
      panelview.removeAttribute("current");
    }

    this._viewShowing = null;

    if (!this.node || !theOne)
      return;

    this._currentSubView = theOne;
    if (!theOne.hasAttribute("current")) {
      theOne.setAttribute("current", true);
      this.descriptionHeightWorkaround(theOne);
      this._dispatchViewEvent(theOne, "ViewShown");
    }
    this._showingSubView = theOne.id != this._mainViewId;
  }

  showSubView(aViewId, aAnchor, aPreviousView) {
    this._currentShowPromise = (async () => {
      // Support passing in the node directly.
      let viewNode = typeof aViewId == "string" ? this.node.querySelector("#" + aViewId) : aViewId;
      if (!viewNode) {
        viewNode = this.document.getElementById(aViewId);
        if (viewNode) {
          this._placeSubView(viewNode);
        } else {
          throw new Error(`Subview ${aViewId} doesn't exist!`);
        }
      } else if (viewNode.parentNode == this._panelViewCache) {
        this._placeSubView(viewNode);
      }

      viewNode.panelMultiView = this.node;
      this._setHeader(viewNode, viewNode.getAttribute("title") ||
                                (aAnchor && aAnchor.getAttribute("label")));

      let reverse = !!aPreviousView;
      let previousViewNode = aPreviousView || this._currentSubView;
      // If the panelview to show is the same as the previous one, the 'ViewShowing'
      // event has already been dispatched. Don't do it twice.
      let showingSameView = viewNode == previousViewNode;
      let playTransition = (!!previousViewNode && !showingSameView && this._panel.state == "open");
      let isMainView = viewNode.id == this._mainViewId;

      let dwu = this._dwu;
      let previousRect = previousViewNode.__lastKnownBoundingRect =
          dwu.getBoundsWithoutFlushing(previousViewNode);
      // Cache the measures that have the same caching lifetime as the width
      // or height of the main view, i.e. whilst the panel is shown and/ or
      // visible.
      if (!this._mainViewWidth) {
        this._mainViewWidth = previousRect.width;
        let top = dwu.getBoundsWithoutFlushing(previousViewNode.firstChild || previousViewNode).top;
        let bottom = dwu.getBoundsWithoutFlushing(previousViewNode.lastChild || previousViewNode).bottom;
        this._viewVerticalPadding = previousRect.height - (bottom - top);
      }
      if (!this._mainViewHeight) {
        this._mainViewHeight = previousRect.height;
        this._viewContainer.style.minHeight = this._mainViewHeight + "px";
      }

      this._viewShowing = viewNode;
      // Because the 'mainview' attribute may be out-of-sync, due to view node
      // reparenting in combination with ephemeral PanelMultiView instances,
      // this is the best place to correct it (just before showing).
      if (isMainView)
        viewNode.setAttribute("mainview", true);
      else
        viewNode.removeAttribute("mainview");

      if (aAnchor) {
        viewNode.classList.add("PanelUI-subView");
      }
      if (!isMainView && this._mainViewWidth)
        viewNode.style.maxWidth = viewNode.style.minWidth = this._mainViewWidth + "px";

      if (!showingSameView || !viewNode.hasAttribute("current")) {
        // Emit the ViewShowing event so that the widget definition has a chance
        // to lazily populate the subview with things or perhaps even cancel this
        // whole operation.
        let detail = {
          blockers: new Set(),
          addBlocker(promise) {
            this.blockers.add(promise);
          }
        };
        let cancel = this._dispatchViewEvent(viewNode, "ViewShowing", aAnchor, detail);
        if (detail.blockers.size) {
          try {
            let results = await Promise.all(detail.blockers);
            cancel = cancel || results.some(val => val === false);
          } catch (e) {
            Cu.reportError(e);
            cancel = true;
          }
        }

        if (cancel) {
          this._viewShowing = null;
          return;
        }
      }

      // Now we have to transition the panel. If we've got an older transition
      // still running, make sure to clean it up.
      await this._cleanupTransitionPhase();
      if (playTransition) {
        await this._transitionViews(previousViewNode, viewNode, reverse, previousRect, aAnchor);
        this._updateKeyboardFocus(viewNode);
      } else {
        this.hideAllViewsExcept(viewNode);
      }
    })().catch(e => Cu.reportError(e));
    return this._currentShowPromise;
  }

  /**
   * Apply a transition to 'slide' from the currently active view to the next
   * one.
   * Sliding the next subview in means that the previous panelview stays where it
   * is and the active panelview slides in from the left in LTR mode, right in
   * RTL mode.
   *
   * @param {panelview} previousViewNode Node that is currently shown as active,
   *                                     but is about to be transitioned away.
   * @param {panelview} viewNode         Node that will becode the active view,
   *                                     after the transition has finished.
   * @param {Boolean}   reverse          Whether we're navigation back to a
   *                                     previous view or forward to a next view.
   * @param {Object}    previousRect     Rect object, with the same structure as
   *                                     a DOMRect, of the `previousViewNode`.
   * @param {Element}   anchor           the anchor for which we're opening
   *                                     a new panelview, if any
   */
  async _transitionViews(previousViewNode, viewNode, reverse, previousRect, anchor) {
    // There's absolutely no need to show off our epic animation skillz when
    // the panel's not even open.
    if (this._panel.state != "open") {
      return;
    }

    const {window, document} = this;

    if (this._autoResizeWorkaroundTimer)
      window.clearTimeout(this._autoResizeWorkaroundTimer);

    let details = this._transitionDetails = {
      phase: TRANSITION_PHASES.START,
      previousViewNode, viewNode, reverse, anchor
    };

    if (anchor)
      anchor.setAttribute("open", "true");

    // Since we're going to show two subview at the same time, don't abuse the
    // 'current' attribute, since it's needed for other state-keeping, but use
    // a separate 'in-transition' attribute instead.
    previousViewNode.setAttribute("in-transition", true);
    // Set the viewContainer dimensions to make sure only the current view is
    // visible.
    this._viewContainer.style.height = Math.max(previousRect.height, this._mainViewHeight) + "px";
    this._viewContainer.style.width = previousRect.width + "px";
    // Lock the dimensions of the window that hosts the popup panel.
    let rect = this._panel.popupBoxObject.getOuterScreenRect();
    this._panel.setAttribute("width", rect.width);
    this._panel.setAttribute("height", rect.height);

    let viewRect;
    if (reverse && viewNode.__lastKnownBoundingRect) {
      // Use the cached size when going back to a previous view, but not when
      // reopening a subview, because its contents may have changed.
      viewRect = viewNode.__lastKnownBoundingRect;
      viewNode.setAttribute("in-transition", true);
    } else if (viewNode.customRectGetter) {
      // Can't use Object.assign directly with a DOM Rect object because its properties
      // aren't enumerable.
      let {height, width} = previousRect;
      viewRect = Object.assign({height, width}, viewNode.customRectGetter());
      let header = viewNode.firstChild;
      if (header && header.classList.contains("panel-header")) {
        viewRect.height += this._dwu.getBoundsWithoutFlushing(header).height;
      }
      viewNode.setAttribute("in-transition", true);
    } else {
      let oldSibling = viewNode.nextSibling || null;
      this._offscreenViewStack.style.minHeight =
        this._viewContainer.style.height;
      this._offscreenViewStack.appendChild(viewNode);
      viewNode.setAttribute("in-transition", true);

      // Now that the subview is visible, we can check the height of the
      // description elements it contains.
      this.descriptionHeightWorkaround(viewNode);

      viewRect = await BrowserUtils.promiseLayoutFlushed(this.document, "layout", () => {
        return this._dwu.getBoundsWithoutFlushing(viewNode);
      });

      try {
        this._viewStack.insertBefore(viewNode, oldSibling);
      } catch (ex) {
        this._viewStack.appendChild(viewNode);
      }

      this._offscreenViewStack.style.removeProperty("min-height");
    }

    this._transitioning = true;
    details.phase = TRANSITION_PHASES.PREPARE;

    // The 'magic' part: build up the amount of pixels to move right or left.
    let moveToLeft = (this._dir == "rtl" && !reverse) || (this._dir == "ltr" && reverse);
    let deltaX = previousRect.width;
    let deepestNode = reverse ? previousViewNode : viewNode;

    // With a transition when navigating backwards - user hits the 'back'
    // button - we need to make sure that the views are positioned in a way
    // that a translateX() unveils the previous view from the right direction.
    if (reverse)
      this._viewStack.style.marginInlineStart = "-" + deltaX + "px";

    // Set the transition style and listen for its end to clean up and make sure
    // the box sizing becomes dynamic again.
    // Somehow, putting these properties in PanelUI.css doesn't work for newly
    // shown nodes in a XUL parent node.
    this._viewStack.style.transition = "transform var(--animation-easing-function)" +
      " var(--panelui-subview-transition-duration)";
    this._viewStack.style.willChange = "transform";
    // Use an outline instead of a border so that the size is not affected.
    deepestNode.style.outline = "1px solid var(--panel-separator-color)";

    // Now set the viewContainer dimensions to that of the new view, which
    // kicks of the height animation.
    this._viewContainer.style.height = Math.max(viewRect.height, this._mainViewHeight) + "px";
    this._viewContainer.style.width = viewRect.width + "px";
    this._panel.removeAttribute("width");
    this._panel.removeAttribute("height");
    // We're setting the width property to prevent flickering during the
    // sliding animation with smaller views.
    viewNode.style.width = viewRect.width + "px";

    await BrowserUtils.promiseLayoutFlushed(document, "layout", () => {});

    // Kick off the transition!
    details.phase = TRANSITION_PHASES.TRANSITION;
    this._viewStack.style.transform = "translateX(" + (moveToLeft ? "" : "-") + deltaX + "px)";

    await new Promise(resolve => {
      details.resolve = resolve;
      this._viewContainer.addEventListener("transitionend", details.listener = ev => {
        // It's quite common that `height` on the view container doesn't need
        // to transition, so we make sure to do all the work on the transform
        // transition-end, because that is guaranteed to happen.
        if (ev.target != this._viewStack || ev.propertyName != "transform")
          return;
        this._viewContainer.removeEventListener("transitionend", details.listener);
        delete details.listener;
        resolve();
      });
      this._viewContainer.addEventListener("transitioncancel", details.cancelListener = ev => {
        if (ev.target != this._viewStack)
          return;
        this._viewContainer.removeEventListener("transitioncancel", details.cancelListener);
        delete details.cancelListener;
        resolve();
      });
    });

    details.phase = TRANSITION_PHASES.END;

    await this._cleanupTransitionPhase(details);
  }

  /**
   * Attempt to clean up the attributes and properties set by `_transitionViews`
   * above. Which attributes and properties depends on the phase the transition
   * was left from - normally that'd be `TRANSITION_PHASES.END`.
   *
   * @param {Object} details Dictionary object containing details of the transition
   *                         that should be cleaned up after. Defaults to the most
   *                         recent details.
   */
  async _cleanupTransitionPhase(details = this._transitionDetails) {
    if (!details || !this.node)
      return;

    let {phase, previousViewNode, viewNode, reverse, resolve, listener, cancelListener, anchor} = details;
    if (details == this._transitionDetails)
      this._transitionDetails = null;

    // Do the things we _always_ need to do whenever the transition ends or is
    // interrupted.
    this.hideAllViewsExcept(viewNode);
    previousViewNode.removeAttribute("in-transition");
    viewNode.removeAttribute("in-transition");
    if (reverse)
      this._resetKeyNavigation(previousViewNode);

    if (anchor)
      anchor.removeAttribute("open");

    if (phase >= TRANSITION_PHASES.START) {
      this._panel.removeAttribute("width");
      this._panel.removeAttribute("height");
      // Myeah, panel layout auto-resizing is a funky thing. We'll wait
      // another few milliseconds to remove the width and height 'fixtures',
      // to be sure we don't flicker annoyingly.
      // NB: HACK! Bug 1363756 is there to fix this.
      this._autoResizeWorkaroundTimer = this.window.setTimeout(() => {
        if (!this._viewContainer)
          return;
        this._viewContainer.style.removeProperty("height");
        this._viewContainer.style.removeProperty("width");
      }, 500);
    }
    if (phase >= TRANSITION_PHASES.PREPARE) {
      this._transitioning = false;
      if (reverse)
        this._viewStack.style.removeProperty("margin-inline-start");
      let deepestNode = reverse ? previousViewNode : viewNode;
      deepestNode.style.removeProperty("outline");
      this._viewStack.style.removeProperty("transition");
    }
    if (phase >= TRANSITION_PHASES.TRANSITION) {
      this._viewStack.style.removeProperty("transform");
      viewNode.style.removeProperty("width");
      if (listener)
        this._viewContainer.removeEventListener("transitionend", listener);
      if (cancelListener)
        this._viewContainer.removeEventListener("transitioncancel", cancelListener);
      if (resolve)
        resolve();
    }
    if (phase >= TRANSITION_PHASES.END) {
      // We force 'display: none' on the previous view node to make sure that it
      // doesn't cause an annoying flicker whilst resetting the styles above.
      previousViewNode.style.display = "none";
      await BrowserUtils.promiseLayoutFlushed(this.document, "layout", () => {});
      previousViewNode.style.removeProperty("display");
    }
  }

  /**
   * Helper method to emit an event on a panelview, whilst also making sure that
   * the correct method is called on CustomizableWidget instances.
   *
   * @param  {panelview} viewNode  Target of the event to dispatch.
   * @param  {String}    eventName Name of the event to dispatch.
   * @param  {DOMNode}   [anchor]  Node where the panel is anchored to. Optional.
   * @param  {Object}    [detail]  Event detail object. Optional.
   * @return {Boolean} `true` if the event was canceled by an event handler, `false`
   *                   otherwise.
   */
  _dispatchViewEvent(viewNode, eventName, anchor, detail) {
    let cancel = false;
    if (eventName != "PanelMultiViewHidden") {
      // Don't need to do this for PanelMultiViewHidden event
      CustomizableUI.ensureSubviewListeners(viewNode);
    }

    let evt = new this.window.CustomEvent(eventName, {
      detail,
      bubbles: true,
      cancelable: eventName == "ViewShowing"
    });
    viewNode.dispatchEvent(evt);
    if (!cancel)
      cancel = evt.defaultPrevented;
    return cancel;
  }

  _calculateMaxHeight() {
    // While opening the panel, we have to limit the maximum height of any
    // view based on the space that will be available. We cannot just use
    // window.screen.availTop and availHeight because these may return an
    // incorrect value when the window spans multiple screens.
    let anchorBox = this._panel.anchorNode.boxObject;
    let screen = this._screenManager.screenForRect(anchorBox.screenX,
                                                   anchorBox.screenY,
                                                   anchorBox.width,
                                                   anchorBox.height);
    let availTop = {}, availHeight = {};
    screen.GetAvailRect({}, availTop, {}, availHeight);
    let cssAvailTop = availTop.value / screen.defaultCSSScaleFactor;

    // The distance from the anchor to the available margin of the screen is
    // based on whether the panel will open towards the top or the bottom.
    let maxHeight;
    if (this._panel.alignmentPosition.startsWith("before_")) {
      maxHeight = anchorBox.screenY - cssAvailTop;
    } else {
      let anchorScreenBottom = anchorBox.screenY + anchorBox.height;
      let cssAvailHeight = availHeight.value / screen.defaultCSSScaleFactor;
      maxHeight = cssAvailTop + cssAvailHeight - anchorScreenBottom;
    }

    // To go from the maximum height of the panel to the maximum height of
    // the view stack, we need to subtract the height of the arrow and the
    // height of the opposite margin, but we cannot get their actual values
    // because the panel is not visible yet. However, we know that this is
    // currently 11px on Mac, 13px on Windows, and 13px on Linux. We also
    // want an extra margin, both for visual reasons and to prevent glitches
    // due to small rounding errors. So, we just use a value that makes
    // sense for all platforms. If the arrow visuals change significantly,
    // this value will be easy to adjust.
    const EXTRA_MARGIN_PX = 20;
    maxHeight -= EXTRA_MARGIN_PX;
    return maxHeight;
  }

  handleEvent(aEvent) {
    if (aEvent.type.startsWith("popup") && aEvent.target != this._panel) {
      // Shouldn't act on e.g. context menus being shown from within the panel.
      return;
    }
    switch (aEvent.type) {
      case "keydown":
        this._keyNavigation(aEvent);
        break;
      case "mousemove":
        this._resetKeyNavigation();
        break;
      case "popupshowing": {
        this.node.setAttribute("panelopen", "true");
        if (this.panelViews && !this.node.hasAttribute("disablekeynav")) {
          this.window.addEventListener("keydown", this);
          this._panel.addEventListener("mousemove", this);
        }
        break;
      }
      case "popuppositioned": {
        // When autoPosition is true, the popup window manager would attempt to re-position
        // the panel as subviews are opened and it changes size. The resulting popoppositioned
        // events triggers the binding's arrow position adjustment - and its reflow.
        // This is not needed here, as we calculated and set maxHeight so it is known
        // to fit the screen while open.
        // autoPosition gets reset after each popuppositioned event, and when the
        // popup closes, so we must set it back to false each time.
        this._panel.autoPosition = false;

        if (this._panel.state == "showing") {
          let maxHeight = this._calculateMaxHeight();
          this._viewStack.style.maxHeight = maxHeight + "px";
          this._offscreenViewStack.style.maxHeight = maxHeight + "px";
        }
        break;
      }
      case "popupshown":
        // Now that the main view is visible, we can check the height of the
        // description elements it contains.
        this.descriptionHeightWorkaround();
        break;
      case "popuphidden": {
        // WebExtensions consumers can hide the popup from viewshowing, or
        // mid-transition, which disrupts our state:
        this._viewShowing = null;
        this._transitioning = false;
        this.node.removeAttribute("panelopen");
        this.showMainView();
        for (let panelView of this._viewStack.children) {
          if (panelView.nodeName != "children") {
            panelView.__lastKnownBoundingRect = null;
            panelView.style.removeProperty("min-width");
            panelView.style.removeProperty("max-width");
          }
        }
        this.window.removeEventListener("keydown", this);
        this._panel.removeEventListener("mousemove", this);
        this._resetKeyNavigation();

        // Clear the main view size caches. The dimensions could be different
        // when the popup is opened again, e.g. through touch mode sizing.
        this._mainViewHeight = 0;
        this._mainViewWidth = 0;
        this._viewContainer.style.removeProperty("min-height");
        this._viewStack.style.removeProperty("max-height");
        this._viewContainer.style.removeProperty("min-width");
        this._viewContainer.style.removeProperty("max-width");

        this._dispatchViewEvent(this.node, "PanelMultiViewHidden");
        break;
      }
    }
  }

  /**
   * Based on going up or down, select the previous or next focusable button
   * in the current view.
   *
   * @param {Object}  navMap   the navigation keyboard map object for the view
   * @param {Array}   buttons  an array of focusable buttons to select an item from.
   * @param {Boolean} isDown   whether we're going down (true) or up (false) in this view.
   *
   * @return {DOMNode} the button we selected.
   */
  _updateSelectedKeyNav(navMap, buttons, isDown) {
    let lastSelected = navMap.selected && navMap.selected.get();
    let newButton = null;
    let maxIdx = buttons.length - 1;
    if (lastSelected) {
      let buttonIndex = buttons.indexOf(lastSelected);
      if (buttonIndex != -1) {
        // Buttons may get selected whilst the panel is shown, so add an extra
        // check here.
        do {
          buttonIndex = buttonIndex + (isDown ? 1 : -1);
        } while (buttons[buttonIndex] && buttons[buttonIndex].disabled);
        if (isDown && buttonIndex > maxIdx)
          buttonIndex = 0;
        else if (!isDown && buttonIndex < 0)
          buttonIndex = maxIdx;
        newButton = buttons[buttonIndex];
      } else {
        // The previously selected item is no longer selectable. Find the next item:
        let allButtons = lastSelected.closest("panelview").getElementsByTagName("toolbarbutton");
        let maxAllButtonIdx = allButtons.length - 1;
        let allButtonIndex = allButtons.indexOf(lastSelected);
        while (allButtonIndex >= 0 && allButtonIndex <= maxAllButtonIdx) {
          allButtonIndex++;
          // Check if the next button is in the list of focusable buttons.
          buttonIndex = buttons.indexOf(allButtons[allButtonIndex]);
          if (buttonIndex != -1) {
            // If it is, just use that button if we were going down, or the previous one
            // otherwise. If this was the first button, newButton will end up undefined,
            // which is fine because we'll fall back to using the last button at the
            // bottom of this method.
            newButton = buttons[isDown ? buttonIndex : buttonIndex - 1];
            break;
          }
        }
      }
    }

    // If we couldn't find something, select the first or last item:
    if (!newButton) {
      newButton = buttons[isDown ? 0 : maxIdx];
    }
    navMap.selected = Cu.getWeakReference(newButton);
    return newButton;
  }

  /**
   * Allow for navigating subview buttons using the arrow keys and the Enter key.
   * The Up and Down keys can be used to navigate the list up and down and the
   * Enter, Right or Left - depending on the text direction - key can be used to
   * simulate a click on the currently selected button.
   * The Right or Left key - depending on the text direction - can be used to
   * navigate to the previous view, functioning as a shortcut for the view's
   * back button.
   * Thus, in LTR mode:
   *  - The Right key functions the same as the Enter key, simulating a click
   *  - The Left key triggers a navigation back to the previous view.
   *
   * @param {KeyEvent} event
   */
  _keyNavigation(event) {
    if (this._transitioning)
      return;

    let view = this._currentSubView;
    let navMap = this._keyNavigationMap.get(view);
    if (!navMap) {
      navMap = {};
      this._keyNavigationMap.set(view, navMap);
    }

    let buttons = navMap.buttons;
    if (!buttons || !buttons.length) {
      buttons = navMap.buttons = this._getNavigableElements(view);
      // Set the 'tabindex' attribute on the buttons to make sure they're focussable.
      for (let button of buttons) {
        if (!button.classList.contains("subviewbutton-back") &&
            !button.hasAttribute("tabindex")) {
          button.setAttribute("tabindex", 0);
        }
      }
    }
    if (!buttons.length)
      return;

    let stop = () => {
      event.stopPropagation();
      event.preventDefault();
    };

    let keyCode = event.code;
    switch (keyCode) {
      case "ArrowDown":
      case "ArrowUp": {
        stop();
        let isDown = (keyCode == "ArrowDown");
        let button = this._updateSelectedKeyNav(navMap, buttons, isDown);
        button.focus();
        break;
      }
      case "ArrowLeft":
      case "ArrowRight": {
        stop();
        let dir = this._dir;
        if ((dir == "ltr" && keyCode == "ArrowLeft") ||
            (dir == "rtl" && keyCode == "ArrowRight")) {
          if (this._canGoBack(view))
            this.goBack();
          break;
        }
        // If the current button is _not_ one that points to a subview, pressing
        // the arrow key shouldn't do anything.
        if (!navMap.selected || !navMap.selected.get() ||
            !navMap.selected.get().classList.contains("subviewbutton-nav")) {
          break;
        }
        // Fall-through...
      }
      case "Space":
      case "Enter": {
        let button = navMap.selected && navMap.selected.get();
        if (!button)
          break;
        stop();

        // Unfortunately, 'tabindex' doesn't execute the default action, so
        // we explicitly do this here.
        // We are sending a command event and then a click event.
        // This is done in order to mimic a "real" mouse click event.
        // The command event executes the action, then the click event closes the menu.
        button.doCommand();
        let clickEvent = new event.target.ownerGlobal.MouseEvent("click", {"bubbles": true});
        button.dispatchEvent(clickEvent);
        break;
      }
    }
  }

  /**
   * Clear all traces of keyboard navigation happening right now.
   *
   * @param {panelview} view View to reset the key navigation attributes of.
   *                         If no view is passed, all navigation attributes for
   *                         this panelmultiview are cleared.
   */
  _resetKeyNavigation(view) {
    let viewToBlur = view || this._currentSubView;
    let navMap = this._keyNavigationMap.get(viewToBlur);
    if (navMap && navMap.selected && navMap.selected.get()) {
      navMap.selected.get().blur();
    }

    // We clear the entire key navigation map ONLY if *no* view was passed in.
    // This happens e.g. when the popup is hidden completely, or the user moves
    // their mouse.
    // If a view is passed in, we just delete the map for that view. This happens
    // when going back from a view (which resets the map for that view only)
    if (view) {
      this._keyNavigationMap.delete(view);
    } else {
      this._keyNavigationMap.clear();
    }
  }

  /**
   * Retrieve the button elements from a view node that can be used for navigation
   * using the keyboard; enabled buttons and the back button, if visible.
   *
   * @param  {nsIDOMNode} view
   * @return {Array}
   */
  _getNavigableElements(view) {
    let buttons = Array.from(view.querySelectorAll(".subviewbutton:not([disabled])"));
    let dwu = this._dwu;
    return buttons.filter(button => {
      let bounds = dwu.getBoundsWithoutFlushing(button);
      return bounds.width > 0 && bounds.height > 0;
    });
  }

  /**
   * Focus the last selected element in the view, if any.
   *
   * @param {panelview} view the view in which to update keyboard focus.
   */
  _updateKeyboardFocus(view) {
    let navMap = this._keyNavigationMap.get(view);
    if (navMap && navMap.selected && navMap.selected.get()) {
      navMap.selected.get().focus();
    }
  }

  /**
   * If the main view or a subview contains wrapping elements, the attribute
   * "descriptionheightworkaround" should be set on the view to force all the
   * wrapping "description", "label" or "toolbarbutton" elements to a fixed
   * height. If the attribute is set and the visibility, contents, or width
   * of any of these elements changes, this function should be called to
   * refresh the calculated heights.
   *
   * This may trigger a synchronous layout.
   *
   * @param viewNode
   *        Indicates the node to scan for descendant elements. This is the main
   *        view if omitted.
   */
  descriptionHeightWorkaround(viewNode = this._mainView) {
    if (!viewNode || !viewNode.hasAttribute("descriptionheightworkaround")) {
      // This view does not require the workaround.
      return;
    }

    // We batch DOM changes together in order to reduce synchronous layouts.
    // First we reset any change we may have made previously. The first time
    // this is called, and in the best case scenario, this has no effect.
    let items = [];
    // Non-hidden <label> or <description> elements that also aren't empty
    // and also don't have a value attribute can be multiline (if their
    // text content is long enough).
    let isMultiline = ":not(:-moz-any([hidden],[value],:empty))";
    let selector = [
      "description" + isMultiline,
      "label" + isMultiline,
      "toolbarbutton[wrap]:not([hidden])",
    ].join(",");
    for (let element of viewNode.querySelectorAll(selector)) {
      // Ignore items in hidden containers.
      if (element.closest("[hidden]")) {
        continue;
      }
      // Take the label for toolbarbuttons; it only exists on those elements.
      element = element.labelElement || element;

      let bounds = element.getBoundingClientRect();
      let previous = this._multiLineElementsMap.get(element);
      // We don't need to (re-)apply the workaround for invisible elements or
      // on elements we've seen before and haven't changed in the meantime.
      if (!bounds.width || !bounds.height ||
          (previous && element.textContent == previous.textContent &&
                       bounds.width == previous.bounds.width)) {
        continue;
      }

      items.push({ element });
    }

    // Removing the 'height' property will only cause a layout flush in the next
    // loop below if it was set.
    for (let item of items) {
      item.element.style.removeProperty("height");
    }

    // We now read the computed style to store the height of any element that
    // may contain wrapping text.
    for (let item of items) {
      item.bounds = item.element.getBoundingClientRect();
    }

    // Now we can make all the necessary DOM changes at once.
    for (let { element, bounds } of items) {
      this._multiLineElementsMap.set(element, { bounds, textContent: element.textContent });
      element.style.height = bounds.height + "px";
    }
  }
};
