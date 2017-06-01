/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["PanelMultiView"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
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
  get showingSubViewAsMainView() {
    return this.node.getAttribute("mainViewIsSubView") == "true";
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
    ["_mainView", "ignoreMutations", "showingSubView"].forEach(property => {
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
    this._panel.removeEventListener("popupshowing", this);
    this._panel.removeEventListener("popupshown", this);
    this._panel.removeEventListener("popuphidden", this);
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
    if (this.panelViews) {
      this.showSubView(this._mainViewId);
    } else {
      if (this.showingSubView) {
        let viewNode = this._currentSubView;
        let evt = new this.window.CustomEvent("ViewHiding", { bubbles: true, cancelable: true });
        viewNode.dispatchEvent(evt);

        this._transitionHeight(() => {
          viewNode.removeAttribute("current");
          this._currentSubView = null;
          this.node.setAttribute("viewtype", "main");
        });
      }

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
          if (this.panelViews) {
            this._viewStack.appendChild(viewNode);
            this.panelViews.push(viewNode);
          } else {
            this._subViews.appendChild(viewNode);
          }
        } else {
          throw new Error(`Subview ${aViewId} doesn't exist!`);
        }
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
            let top = dwu.getBoundsWithoutFlushing(previousViewNode.firstChild).top;
            let bottom = dwu.getBoundsWithoutFlushing(previousViewNode.lastChild).bottom;
            this._viewVerticalPadding = previousRect.height - (bottom - top);
          }
          // Here go the measures that have the same caching lifetime as the height
          // of the main view, i.e. whilst the panel is shown and/ or visible.
          if (!this._mainViewHeight) {
            this._mainViewHeight = previousRect.height;
          }
        }
      }

      // Emit the ViewShowing event so that the widget definition has a chance
      // to lazily populate the subview with things.
      let detail = {
        blockers: new Set(),
        addBlocker(aPromise) {
          this.blockers.add(aPromise);
        },
      };

      // Make sure that new panels always have a title set.
      if (this.panelViews && aAnchor) {
        if (aAnchor && !viewNode.hasAttribute("title"))
          viewNode.setAttribute("title", aAnchor.getAttribute("label"));
        viewNode.classList.add("PanelUI-subView");
        let custWidget = CustomizableWidgets.find(widget => widget.viewId == viewNode.id);
        if (custWidget) {
          if (custWidget.onInit)
            custWidget.onInit(aAnchor);
          custWidget.onViewShowing({ target: aAnchor, detail });
        }
      }
      viewNode.setAttribute("current", true);
      if (this.panelViews && this._mainViewWidth)
        viewNode.style.maxWidth = viewNode.style.minWidth = this._mainViewWidth + "px";

      let evt = new window.CustomEvent("ViewShowing", { bubbles: true, cancelable: true, detail });
      viewNode.dispatchEvent(evt);

      let cancel = evt.defaultPrevented;
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
        return;
      }

      this._currentSubView = viewNode;
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
          evt = new window.CustomEvent("ViewHiding", { bubbles: true, cancelable: true });
          previousViewNode.dispatchEvent(evt);
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
        this._viewContainer.style.height = Math.max(previousRect.height, this._mainViewHeight) + "px";
        this._viewContainer.style.width = previousRect.width + "px";

        this._transitioning = true;
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
          let viewRect = viewNode.__lastKnownBoundingRect;
          if (!viewRect) {
            viewRect = dwu.getBoundsWithoutFlushing(viewNode);
            if (!reverse) {
              // When showing two nodes at the same time (one partly out of view,
              // but that doesn't seem to make a difference in this case) inside
              // a XUL node container, the flexible box layout on the vertical
              // axis gets confused. I.e. it lies.
              // So what we need to resort to here is count the height of each
              // individual child element of the view.
              viewRect.height = [viewNode.header, ...viewNode.children].reduce((acc, node) => {
                return acc + dwu.getBoundsWithoutFlushing(node).height;
              }, this._viewVerticalPadding);
            }
          }

          // Set the viewContainer dimensions to make sure only the current view
          // is visible.
          this._viewContainer.style.height = Math.max(viewRect.height, this._mainViewHeight) + "px";
          this._viewContainer.style.width = viewRect.width + "px";

          // The 'magic' part: build up the amount of pixels to move right or left.
          let moveToLeft = (this._dir == "rtl" && !reverse) || (this._dir == "ltr" && reverse);
          let movementX = reverse ? viewRect.width : previousRect.width;
          let moveX = (moveToLeft ? "" : "-") + movementX;
          nodeToAnimate.style.transform = "translateX(" + moveX + "px)";
          // We're setting the width property to prevent flickering during the
          // sliding animation with smaller views.
          nodeToAnimate.style.width = viewRect.width + "px";

          let listener;
          this._viewContainer.addEventListener("transitionend", listener = ev => {
            // It's quite common that `height` on the view container doesn't need
            // to transition, so we make sure to do all the work on the transform
            // transition-end, because that is guaranteed to happen.
            if (ev.target != nodeToAnimate || ev.propertyName != "transform")
              return;

            this._viewContainer.removeEventListener("transitionend", listener);
            onTransitionEnd();
            this._transitioning = false;
            this._resetKeyNavigation(previousViewNode);

            // Myeah, panel layout auto-resizing is a funky thing. We'll wait
            // another few milliseconds to remove the width and height 'fixtures',
            // to be sure we don't flicker annoyingly.
            // NB: HACK! Bug 1363756 is there to fix this.
            window.setTimeout(() => {
              // Only remove the height when the view is larger than the main
              // view, otherwise it'll snap back to its own height.
              if (viewNode == this._mainView || viewRect.height > this._mainViewHeight)
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

              evt = new window.CustomEvent("ViewShown", { bubbles: true, cancelable: false });
              viewNode.dispatchEvent(evt);
            }, { once: true });
          });
        }, { once: true });
      } else if (!this.panelViews) {
        this._transitionHeight(() => {
          viewNode.setAttribute("current", true);
          this.node.setAttribute("viewtype", "subview");
          // Now that the subview is visible, we can check the height of the
          // description elements it contains.
          this.descriptionHeightWorkaround(viewNode);
        });
        this._shiftMainView(aAnchor);
      }
    })();
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
        break;
      case "popupshown":
        // Now that the main view is visible, we can check the height of the
        // description elements it contains.
        this.descriptionHeightWorkaround();

        if (!this.panelViews) {
          // Now that the panel has opened, we can compute the distance from its
          // anchor to the available margin of the screen, based on whether the
          // panel actually opened towards the top or the bottom. We use this to
          // limit its maximum height, which is relevant when opening a subview.
          let maxHeight;
          if (this._panel.alignmentPosition.startsWith("before_")) {
            maxHeight = this._panel.getOuterScreenRect().bottom -
                        this.window.screen.availTop;
          } else {
            maxHeight = this.window.screen.availTop +
                        this.window.screen.availHeight -
                        this._panel.getOuterScreenRect().top;
          }
          // To go from the maximum height of the panel to the maximum height of
          // the view stack, we start by subtracting the height of the arrow box.
          // We don't need to trigger a new layout because this does not change.
          let arrowBox = this.document.getAnonymousElementByAttribute(
                                          this._panel, "anonid", "arrowbox");
          maxHeight -= this._dwu.getBoundsWithoutFlushing(arrowBox).height;
          // We subtract a fixed margin to account for variable borders. We don't
          // try to measure this accurately so it's faster, we don't depend on
          // the arrowpanel structure, and we don't hit rounding errors. Instead,
          // we use a value that is much greater than the typical borders and
          // makes sense visually.
          const EXTRA_MARGIN_PX = 8;
          this._viewStack.style.maxHeight = (maxHeight - EXTRA_MARGIN_PX) + "px";
        }
        break;
      case "popuphidden":
        this.node.removeAttribute("panelopen");
        this.showMainView();
        if (this.panelViews) {
          this.window.removeEventListener("keydown", this);
          this._panel.removeEventListener("mousemove", this);
          this._resetKeyNavigation();
          this._mainViewHeight = 0;
        }
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
   * "description" or wrapping toolbarbutton elements to a fixed height.
   * If the attribute is set and the visibility, contents, or width of any of
   * these elements changes, this function should be called to refresh the
   * calculated heights.
   *
   * This may trigger a synchronous layout.
   *
   * @param viewNode
   *        Indicates the node to scan for descendant elements. This is the main
   *        view if omitted.
   */
  descriptionHeightWorkaround(viewNode = this._mainView) {
    if (!this.node.hasAttribute("descriptionheightworkaround")) {
      // This view does not require the workaround.
      return;
    }

    // We batch DOM changes together in order to reduce synchronous layouts.
    // First we reset any change we may have made previously. The first time
    // this is called, and in the best case scenario, this has no effect.
    let items = [];
    for (let element of viewNode.querySelectorAll(
         "description:not([hidden]):not([value]),toolbarbutton[wrap]:not([hidden])")) {
      // Take the label for toolbarbuttons; it only exists on those elements.
      element = element.labelElement || element;

      let bounds = this._dwu.getBoundsWithoutFlushing(element);
      let previous = this._multiLineElementsMap.get(element);
      // Only remove the 'height' property, which will cause a layout flush, when
      // absolutely necessary.
      if (!bounds.width || !bounds.height ||
          (previous && element.textContent == previous.textContent &&
                       bounds.width == previous.bounds.width)) {
        continue;
      }

      element.style.removeProperty("height");
      items.push({ element });
    }

    // We now read the computed style to store the height of any element that
    // may contain wrapping text, which will be zero if the element is hidden.
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
