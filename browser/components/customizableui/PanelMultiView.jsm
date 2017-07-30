/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["PanelMultiView"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AppConstants",
  "resource://gre/modules/AppConstants.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "CustomizableWidgets",
  "resource:///modules/CustomizableWidgets.jsm");

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
    return this.node.getAttribute("viewtype") == "subview";
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

  get panelViews() {
    // If there's a dedicated subViews container, we're not in the right binding
    // to use SlidingPanelViews.
    if (this._subViews)
      return null;

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
   * Getter that returns the currently visible subview OR the subview that is
   * about to be shown whilst a 'ViewShowing' event is being dispatched.
   *
   * @return {panelview}
   */
  get current() {
    return this._viewShowing || this._currentSubView
  }
  get _currentSubView() {
    return this.panelViews ? this.panelViews.currentView : this.__currentSubView;
  }
  set _currentSubView(panel) {
    if (this.panelViews)
      this.panelViews.currentView = panel;
    else
      this.__currentSubView = panel;
    return panel;
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

    this._currentSubView = this._anchorElement = this._subViewObserver = null;
    this._mainViewHeight = 0;
    this.__transitioning = this._ignoreMutations = false;

    const {document, window} = this;

    this._clickCapturer =
      document.getAnonymousElementByAttribute(this.node, "anonid", "clickCapturer");
    this._viewContainer =
      document.getAnonymousElementByAttribute(this.node, "anonid", "viewContainer");
    this._mainViewContainer =
      document.getAnonymousElementByAttribute(this.node, "anonid", "mainViewContainer");
    this._subViews =
      document.getAnonymousElementByAttribute(this.node, "anonid", "subViews");
    this._viewStack =
      document.getAnonymousElementByAttribute(this.node, "anonid", "viewStack");
    this._offscreenViewStack =
      document.getAnonymousElementByAttribute(this.node, "anonid", "offscreenViewStack");

    XPCOMUtils.defineLazyGetter(this, "_panelViewCache", () => {
      let viewCacheId = this.node.getAttribute("viewCacheId");
      return viewCacheId ? document.getElementById(viewCacheId) : null;
    });

    this._panel.addEventListener("popupshowing", this);
    this._panel.addEventListener("popuphidden", this);
    this._panel.addEventListener("popupshown", this);
    if (this.panelViews) {
      let cs = window.getComputedStyle(document.documentElement);
      // Set CSS-determined attributes now to prevent a layout flush when we do
      // it when transitioning between panels.
      this._dir = cs.direction;
      this.setMainView(this.panelViews.currentView);
      this.showMainView();
    } else {
      this._clickCapturer.addEventListener("click", this);

      this._mainViewContainer.setAttribute("panelid", this._panel.id);

      if (this._mainView) {
        this.setMainView(this._mainView);
      }
    }

    this.node.setAttribute("viewtype", "main");

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
    Object.defineProperty(this.node, "current", {
      enumerable: true,
      get: () => this.current
    });
  }

  destructor() {
    // Guard against re-entrancy.
    if (!this.node)
      return;

    if (this._mainView) {
      let mainView = this._mainView;
      if (this._panelViewCache)
        this._panelViewCache.appendChild(mainView);
      mainView.removeAttribute("mainview");
    }
    if (this._subViews)
      this._moveOutKids(this._subViews);

    if (this.panelViews) {
      this._moveOutKids(this._viewStack);
      this.panelViews.clear();
    } else {
      this._clickCapturer.removeEventListener("click", this);
    }
    this._panel.removeEventListener("mousemove", this);
    this._panel.removeEventListener("popupshowing", this);
    this._panel.removeEventListener("popupshown", this);
    this._panel.removeEventListener("popuphidden", this);
    this.window.removeEventListener("keydown", this);
    this._dispatchViewEvent(this.node, "destructed");
    this.node = this._clickCapturer = this._viewContainer = this._mainViewContainer =
      this._subViews = this._viewStack = this.__dwu = this._panelViewCache = null;
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
    if (this.panelViews) {
      this._viewStack.appendChild(viewNode);
      if (!this.panelViews.includes(viewNode))
        this.panelViews.push(viewNode);
    } else {
      this._subViews.appendChild(viewNode);
    }
  }

  goBack(target) {
    let [current, previous] = this.panelViews.back();
    return this.showSubView(current, target, previous);
  }

  /**
   * Checks whether it is possible to navigate backwards currently. Returns
   * false if this is the panelmultiview's mainview, true otherwise.
   *
   * @param  {panelview} view View to check, defaults to the currently active view.
   * @return {Boolean}
   */
  _canGoBack(view = this._currentSubView) {
    return view != this._mainView;
  }

  setMainView(aNewMainView) {
    if (this._mainView) {
      if (!this.panelViews)
        this._subViews.appendChild(this._mainView);
      this._mainView.removeAttribute("mainview");
    }
    this._mainViewId = aNewMainView.id;
    aNewMainView.setAttribute("mainview", "true");
    if (this.panelViews) {
      // If the new main view is not yet in the zeroth position, make sure it's
      // inserted there.
      if (aNewMainView.parentNode != this._viewStack && this._viewStack.firstChild != aNewMainView) {
        this._viewStack.insertBefore(aNewMainView, this._viewStack.firstChild);
      }
    } else {
      this._mainViewContainer.appendChild(aNewMainView);
    }
  }

  showMainView() {
    if (this.showingSubView) {
      let viewNode = this._currentSubView;
      this._dispatchViewEvent(viewNode, "ViewHiding");
      if (this.panelViews) {
        viewNode.removeAttribute("current");
        this.showSubView(this._mainViewId);
        this.node.setAttribute("viewtype", "main");
      } else {
        this._transitionHeight(() => {
          viewNode.removeAttribute("current");
          this._currentSubView = null;
          this.node.setAttribute("viewtype", "main");
        });
      }
    }

    if (!this.panelViews) {
      this._shiftMainView();
    }
  }

  showSubView(aViewId, aAnchor, aPreviousView) {
    const {document, window} = this;
    return (async () => {
      // Support passing in the node directly.
      let viewNode = typeof aViewId == "string" ? this.node.querySelector("#" + aViewId) : aViewId;
      if (!viewNode) {
        viewNode = document.getElementById(aViewId);
        if (viewNode) {
          this._placeSubView(viewNode);
        } else {
          throw new Error(`Subview ${aViewId} doesn't exist!`);
        }
      } else if (viewNode.parentNode == this._panelViewCache) {
        this._placeSubView(viewNode);
      }

      let reverse = !!aPreviousView;
      let previousViewNode = aPreviousView || this._currentSubView;
      let playTransition = (!!previousViewNode && previousViewNode != viewNode);

      let dwu, previousRect;
      if (playTransition || this.panelViews) {
        dwu = this._dwu;
        previousRect = previousViewNode.__lastKnownBoundingRect =
          dwu.getBoundsWithoutFlushing(previousViewNode);
        if (this.panelViews) {
          // Here go the measures that have the same caching lifetime as the width
          // of the main view, i.e. 'forever', during the instance lifetime.
          if (!this._mainViewWidth) {
            this._mainViewWidth = previousRect.width;
            let top = dwu.getBoundsWithoutFlushing(previousViewNode.firstChild || previousViewNode).top;
            let bottom = dwu.getBoundsWithoutFlushing(previousViewNode.lastChild || previousViewNode).bottom;
            this._viewVerticalPadding = previousRect.height - (bottom - top);
          }
          // Here go the measures that have the same caching lifetime as the height
          // of the main view, i.e. whilst the panel is shown and/ or visible.
          if (!this._mainViewHeight) {
            this._mainViewHeight = previousRect.height;
            this._viewContainer.style.minHeight = this._mainViewHeight + "px";
          }
        }
      }

      this._viewShowing = viewNode;

      // Make sure that new panels always have a title set.
      if (this.panelViews && aAnchor) {
        if (!viewNode.hasAttribute("title"))
          viewNode.setAttribute("title", aAnchor.getAttribute("label"));
        viewNode.classList.add("PanelUI-subView");
      }
      if (this.panelViews && this._mainViewWidth)
        viewNode.style.maxWidth = viewNode.style.minWidth = this._mainViewWidth + "px";

      // Emit the ViewShowing event so that the widget definition has a chance
      // to lazily populate the subview with things.
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

      this._viewShowing = null;
      if (cancel) {
        return;
      }

      this._currentSubView = viewNode;
      viewNode.setAttribute("current", true);
      if (this.panelViews) {
        this.node.setAttribute("viewtype", "subview");
        if (!playTransition)
          this.descriptionHeightWorkaround(viewNode);
      }

      // Now we have to transition the panel. There are a few parts to this:
      //
      // 1) The main view content gets shifted so that the center of the anchor
      //    node is at the left-most edge of the panel.
      // 2) The subview deck slides in so that it takes up almost all of the
      //    panel.
      // 3) If the subview is taller then the main panel contents, then the panel
      //    must grow to meet that new height. Otherwise, it must shrink.
      //
      // All three of these actions make use of CSS transformations, so they
      // should all occur simultaneously.
      if (this.panelViews && playTransition) {
        // Sliding the next subview in means that the previous panelview stays
        // where it is and the active panelview slides in from the left in LTR
        // mode, right in RTL mode.
        let onTransitionEnd = () => {
          this._dispatchViewEvent(previousViewNode, "ViewHiding");
          previousViewNode.removeAttribute("current");
          this.descriptionHeightWorkaround(viewNode);
        };

        // There's absolutely no need to show off our epic animation skillz when
        // the panel's not even open.
        if (this._panel.state != "open") {
          onTransitionEnd();
          return;
        }

        if (aAnchor)
          aAnchor.setAttribute("open", true);

        // Set the viewContainer dimensions to make sure only the current view
        // is visible.
        this._viewContainer.style.height = Math.max(previousRect.height, this._mainViewHeight) + "px";
        this._viewContainer.style.width = previousRect.width + "px";
        // Lock the dimensions of the window that hosts the popup panel.
        let rect = this._panel.popupBoxObject.getOuterScreenRect();
        this._panel.setAttribute("width", rect.width);
        this._panel.setAttribute("height", rect.height);

        this._viewBoundsOffscreen(viewNode, previousRect, viewRect => {
          this._transitioning = true;
          if (this._autoResizeWorkaroundTimer)
            window.clearTimeout(this._autoResizeWorkaroundTimer);
          this._viewContainer.setAttribute("transition-reverse", reverse);
          let nodeToAnimate = reverse ? previousViewNode : viewNode;

          if (!reverse) {
            // We set the margin here to make sure the view is positioned next
            // to the view that is currently visible. The animation is taken
            // care of by transitioning the `transform: translateX()` property
            // instead.
            // Once the transition finished, we clean both properties up.
            nodeToAnimate.style.marginInlineStart = previousRect.width + "px";
          }

          // Set the transition style and listen for its end to clean up and
          // make sure the box sizing becomes dynamic again.
          // Somehow, putting these properties in PanelUI.css doesn't work for
          // newly shown nodes in a XUL parent node.
          nodeToAnimate.style.transition = "transform ease-" + (reverse ? "in" : "out") +
            " var(--panelui-subview-transition-duration)";
          nodeToAnimate.style.willChange = "transform";
          nodeToAnimate.style.borderInlineStart = "1px solid var(--panel-separator-color)";

          // Wait until after the first paint to ensure setting 'current=true'
          // has taken full effect; once both views are visible, we want to
          // correctly measure rects using `dwu.getBoundsWithoutFlushing`.
          window.addEventListener("MozAfterPaint", () => {
            if (this._panel.state != "open") {
              onTransitionEnd();
              return;
            }
            // Now set the viewContainer dimensions to that of the new view, which
            // kicks of the height animation.
            this._viewContainer.style.height = Math.max(viewRect.height, this._mainViewHeight) + "px";
            this._viewContainer.style.width = viewRect.width + "px";
            this._panel.removeAttribute("width");
            this._panel.removeAttribute("height");

            // The 'magic' part: build up the amount of pixels to move right or left.
            let moveToLeft = (this._dir == "rtl" && !reverse) || (this._dir == "ltr" && reverse);
            let movementX = reverse ? viewRect.width : previousRect.width;
            let moveX = (moveToLeft ? "" : "-") + movementX;
            nodeToAnimate.style.transform = "translateX(" + moveX + "px)";
            // We're setting the width property to prevent flickering during the
            // sliding animation with smaller views.
            nodeToAnimate.style.width = viewRect.width + "px";

            this._viewContainer.addEventListener("transitionend", this._transitionEndListener = ev => {
              // It's quite common that `height` on the view container doesn't need
              // to transition, so we make sure to do all the work on the transform
              // transition-end, because that is guaranteed to happen.
              if (ev.target != nodeToAnimate || ev.propertyName != "transform")
                return;

              this._viewContainer.removeEventListener("transitionend", this._transitionEndListener);
              this._transitionEndListener = null;
              onTransitionEnd();
              this._transitioning = false;
              this._resetKeyNavigation(previousViewNode);

              // Myeah, panel layout auto-resizing is a funky thing. We'll wait
              // another few milliseconds to remove the width and height 'fixtures',
              // to be sure we don't flicker annoyingly.
              // NB: HACK! Bug 1363756 is there to fix this.
              this._autoResizeWorkaroundTimer = window.setTimeout(() => {
                this._viewContainer.style.removeProperty("height");
                this._viewContainer.style.removeProperty("width");
              }, 500);

              // Take another breather, just like before, to wait for the 'current'
              // attribute removal to take effect. This prevents a flicker.
              // The cleanup we do doesn't affect the display anymore, so we're not
              // too fussed about the timing here.
              window.addEventListener("MozAfterPaint", () => {
                nodeToAnimate.style.removeProperty("border-inline-start");
                nodeToAnimate.style.removeProperty("transition");
                nodeToAnimate.style.removeProperty("transform");
                nodeToAnimate.style.removeProperty("width");

                if (!reverse)
                  viewNode.style.removeProperty("margin-inline-start");
                if (aAnchor)
                  aAnchor.removeAttribute("open");

                this._viewContainer.removeAttribute("transition-reverse");

                this._dispatchViewEvent(viewNode, "ViewShown");
              }, { once: true });
            });
          }, { once: true });
        });
      } else if (!this.panelViews) {
        this._transitionHeight(() => {
          viewNode.setAttribute("current", true);
          this.node.setAttribute("viewtype", "subview");
          // Now that the subview is visible, we can check the height of the
          // description elements it contains.
          this.descriptionHeightWorkaround(viewNode);
          this._dispatchViewEvent(viewNode, "ViewShown");
        });
        this._shiftMainView(aAnchor);
      }
    })().catch(e => Cu.reportError(e));
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
    if (this.panelViews) {
      let custWidget = CustomizableWidgets.find(widget => widget.viewId == viewNode.id);
      let method = "on" + eventName;
      if (custWidget && custWidget[method]) {
        if (anchor && custWidget.onInit)
          custWidget.onInit(anchor);
        custWidget[method]({ target: viewNode, preventDefault: () => cancel = true, detail });
      }
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

  /**
   * Calculate the correct bounds of a panelview node offscreen to minimize the
   * amount of paint flashing and keep the stack vs panel layouts from interfering.
   *
   * @param {panelview} viewNode Node to measure the bounds of.
   * @param {Rect}      previousRect Rect representing the previous view
   *                                 (used to fill in any blanks).
   * @param {Function}  callback Called when we got the measurements in and pass
   *                             them on as its first argument.
   */
  _viewBoundsOffscreen(viewNode, previousRect, callback) {
    if (viewNode.__lastKnownBoundingRect) {
      callback(viewNode.__lastKnownBoundingRect);
      return;
    }

    if (viewNode.customRectGetter) {
      // Can't use Object.assign directly with a DOM Rect object because its properties
      // aren't enumerable.
      let {height, width} = previousRect;
      let rect = Object.assign({height, width}, viewNode.customRectGetter());
      let {header} = viewNode;
      if (header) {
        rect.height += this._dwu.getBoundsWithoutFlushing(header).height;
      }
      callback(rect);
      return;
    }

    let oldSibling = viewNode.nextSibling || null;
    this._offscreenViewStack.appendChild(viewNode);

    this.window.addEventListener("MozAfterPaint", () => {
      let viewRect = this._dwu.getBoundsWithoutFlushing(viewNode);

      try {
        this._viewStack.insertBefore(viewNode, oldSibling);
      } catch (ex) {
        this._viewStack.appendChild(viewNode);
      }

      callback(viewRect);
    }, { once: true });
  }

  /**
   * Applies the height transition for which <panelmultiview> is designed.
   *
   * The height transition involves two elements, the viewContainer and its only
   * immediate child the viewStack. In order for this to work correctly, the
   * viewContainer must have "overflow: hidden;" and the two elements must have
   * no margins or padding. This means that the height of the viewStack is never
   * limited by the viewContainer, but when the height of the container is not
   * constrained it matches the height of the viewStack.
   *
   * @param changeFn
   *        This synchronous function is called to make the DOM changes
   *        that will result in a new height of the viewStack.
   */
  _transitionHeight(changeFn) {
    if (this._panel.state != "open") {
      changeFn();
      return;
    }

    // Lock the dimensions of the window that hosts the popup panel. This
    // in turn constrains the height of the viewContainer.
    let rect = this._panel.popupBoxObject.getOuterScreenRect();
    this._panel.setAttribute("width", rect.width);
    this._panel.setAttribute("height", rect.height);

    // Read the current height of the viewStack. If we are in the middle
    // of a transition, this is the actual height of the element at this
    // point in time.
    let oldHeight = this._dwu.getBoundsWithoutFlushing(this._viewStack).height;

    // Make the necessary DOM changes, and remove the "height" property of the
    // viewStack to ensure that we read its final value even if we are in the
    // middle of a transition. To avoid flickering, we have to prevent the panel
    // from being painted in this temporary state, which requires a synchronous
    // layout when reading the new height.
    this._viewStack.style.removeProperty("height");
    changeFn();
    let newHeight = this._viewStack.getBoundingClientRect().height;

    // Now we can allow the popup panel to resize again. This must occur
    // in the same tick as the code below, but we can do this before
    // setting the starting height in case the transition is not needed.
    this._panel.removeAttribute("width");
    this._panel.removeAttribute("height");

    if (oldHeight != newHeight) {
      // Height transitions can only occur between two numeric values, and
      // cannot start if the height is not set. In case a transition is
      // needed, we have to set the height to the old value, then force a
      // synchronous layout so the panel won't resize unexpectedly.
      this._viewStack.style.height = oldHeight + "px";
      this._viewStack.getBoundingClientRect().height;

      // We can now set the new height to start the transition, but
      // before doing that we set up a listener to reset the height to
      // "auto" at the end, so that DOM changes made after the
      // transition ends are still reflected by the height of the panel.
      let onTransitionEnd = event => {
        if (event.target != this._viewStack) {
          return;
        }
        this._viewStack.removeEventListener("transitionend", onTransitionEnd);
        this._viewStack.style.removeProperty("height");
      };
      this._viewStack.addEventListener("transitionend", onTransitionEnd);
      this._viewStack.style.height = newHeight + "px";
    }
  }

  _shiftMainView(aAnchor) {
    if (aAnchor) {
      // We need to find the edge of the anchor, relative to the main panel.
      // Then we need to add half the width of the anchor. This is the target
      // that we need to transition to.
      let anchorRect = aAnchor.getBoundingClientRect();
      let mainViewRect = this._mainViewContainer.getBoundingClientRect();
      let center = aAnchor.clientWidth / 2;
      let direction = aAnchor.ownerGlobal.getComputedStyle(aAnchor).direction;
      let edge;
      if (direction == "ltr") {
        edge = anchorRect.left - mainViewRect.left;
      } else {
        edge = mainViewRect.right - anchorRect.right;
      }

      // If the anchor is an element on the far end of the mainView we
      // don't want to shift the mainView too far, we would reveal empty
      // space otherwise.
      let cstyle = this.window.getComputedStyle(this.document.documentElement);
      let exitSubViewGutterWidth =
        cstyle.getPropertyValue("--panel-ui-exit-subview-gutter-width");
      let maxShift = mainViewRect.width - parseInt(exitSubViewGutterWidth);
      let target = Math.min(maxShift, edge + center);

      let neg = direction == "ltr" ? "-" : "";
      this._mainViewContainer.style.transform = `translateX(${neg}${target}px)`;
      aAnchor.setAttribute("panel-multiview-anchor", true);
    } else {
      this._mainViewContainer.style.transform = "";
      if (this.anchorElement)
        this.anchorElement.removeAttribute("panel-multiview-anchor");
    }
    this.anchorElement = aAnchor;
  }

  handleEvent(aEvent) {
    if (aEvent.type.startsWith("popup") && aEvent.target != this._panel) {
      // Shouldn't act on e.g. context menus being shown from within the panel.
      return;
    }
    switch (aEvent.type) {
      case "click":
        if (aEvent.originalTarget == this._clickCapturer) {
          this.showMainView();
        }
        break;
      case "keydown":
        this._keyNavigation(aEvent);
        break;
      case "mousemove":
        this._resetKeyNavigation();
        break;
      case "popupshowing":
        this.node.setAttribute("panelopen", "true");
        // Bug 941196 - The panel can get taller when opening a subview. Disabling
        // autoPositioning means that the panel won't jump around if an opened
        // subview causes the panel to exceed the dimensions of the screen in the
        // direction that the panel originally opened in. This property resets
        // every time the popup closes, which is why we have to set it each time.
        this._panel.autoPosition = false;
        if (this.panelViews) {
          this.window.addEventListener("keydown", this);
          this._panel.addEventListener("mousemove", this);
        }

        // Before opening the panel, we have to limit the maximum height of any
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
        this._viewStack.style.maxHeight = maxHeight + "px";

        // When using block-in-box layout inside a scrollable frame, like in the
        // main menu contents scroller, if we allow the contents to scroll then
        // it will not cause its container to expand. Thus, we layout first
        // without any scrolling (using "display: flex;"), and only if the view
        // exceeds the available space we set the height explicitly and enable
        // scrolling.
        if (this._mainView.hasAttribute("blockinboxworkaround")) {
          let blockInBoxWorkaround = () => {
            let mainViewHeight =
                this._dwu.getBoundsWithoutFlushing(this._mainView).height;
            if (mainViewHeight > maxHeight) {
              this._mainView.style.height = maxHeight + "px";
              this._mainView.setAttribute("exceeding", "true");
            }
          };
          // On Windows, we cannot measure the full height of the main view
          // until it is visible. Unfortunately, this causes a visible jump when
          // the view needs to scroll, but there is no easy way around this.
          if (AppConstants.platform == "win") {
            // We register a "once" listener so we don't need to store the value
            // of maxHeight elsewhere on the object.
            this._panel.addEventListener("popupshown", blockInBoxWorkaround,
                                         { once: true });
          } else {
            blockInBoxWorkaround();
          }
        }
        break;
      case "popupshown":
        // Now that the main view is visible, we can check the height of the
        // description elements it contains.
        this.descriptionHeightWorkaround();
        break;
      case "popuphidden":
        // WebExtensions consumers can hide the popup from viewshowing, or
        // mid-transition, which disrupts our state:
        this._viewShowing = null;
        this._transitioning = false;
        this.node.removeAttribute("panelopen");
        this.showMainView();
        if (this.panelViews) {
          if (this._transitionEndListener) {
            this._viewContainer.removeEventListener("transitionend", this._transitionEndListener);
            this._transitionEndListener = null;
          }
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
        }

        // Always try to layout the panel normally when reopening it. This is
        // also the layout that will be used in customize mode.
        if (this._mainView.hasAttribute("blockinboxworkaround")) {
          this._mainView.style.removeProperty("height");
          this._mainView.removeAttribute("exceeding");
        }
        this._dispatchViewEvent(this.node, "PanelMultiViewHidden");
        break;
    }
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
        if (button.classList.contains("subviewbutton-back"))
          continue;
        // If we've been here before, forget about it!
        if (button.hasAttribute("tabindex"))
          break;
        button.setAttribute("tabindex", 0);
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
        let maxIdx = buttons.length - 1;
        let buttonIndex = isDown ? 0 : maxIdx;
        if (typeof navMap.selected == "number") {
          // Buttons may get selected whilst the panel is shown, so add an extra
          // check here.
          do {
            buttonIndex = navMap.selected = (navMap.selected + (isDown ? 1 : -1));
          } while (buttons[buttonIndex] && buttons[buttonIndex].disabled)
          if (isDown && buttonIndex > maxIdx)
            buttonIndex = 0;
          else if (!isDown && buttonIndex < 0)
            buttonIndex = maxIdx;
        }
        let button = buttons[buttonIndex];
        button.focus();
        navMap.selected = buttonIndex;
        break;
      }
      case "ArrowLeft":
      case "ArrowRight": {
        stop();
        let dir = this._dir;
        if ((dir == "ltr" && keyCode == "ArrowLeft") ||
            (dir == "rtl" && keyCode == "ArrowRight")) {
          if (this._canGoBack(view))
            this.goBack(view.backButton);
          break;
        }
        // If the current button is _not_ one that points to a subview, pressing
        // the arrow key shouldn't do anything.
        if (!navMap.selected || !buttons[navMap.selected].classList.contains("subviewbutton-nav"))
          break;
        // Fall-through...
      }
      case "Enter": {
        let button = buttons[navMap.selected];
        if (!button)
          break;
        stop();
        // Unfortunately, 'tabindex' doesn't not execute the default action, so
        // we explicitly do this here.
        button.click();
        break;
      }
    }
  }

  /**
   * Clear all traces of keyboard navigation happening right now.
   *
   * @param {panelview} view View to reset the key navigation attributes of.
   *                         Defaults to `this._currentSubView`.
   */
  _resetKeyNavigation(view = this._currentSubView) {
    let navMap = this._keyNavigationMap.get(view);
    this._keyNavigationMap.clear();
    if (!navMap)
      return;

    let buttons = this._getNavigableElements(view);
    if (!buttons.length)
      return;

    let button = buttons[navMap.selected];
    if (button)
      button.blur();
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
    if (this._canGoBack(view))
      buttons.unshift(view.backButton);
    let dwu = this._dwu;
    return buttons.filter(button => {
      let bounds = dwu.getBoundsWithoutFlushing(button);
      return bounds.width > 0 && bounds.height > 0;
    });
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
    if (!viewNode.hasAttribute("descriptionheightworkaround")) {
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
}
