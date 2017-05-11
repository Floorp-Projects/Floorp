/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["PanelMultiView"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

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

  get ignoreMutations() {
    return this._ignoreMutations;
  }
  set ignoreMutations(val) {
    this._ignoreMutations = val;
    if (!val && this._panel.state == "open") {
      if (this.showingSubView) {
        this._syncContainerWithSubView();
      } else {
        this._syncContainerWithMainView();
      }
    }
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
    return this.window.QueryInterface(Ci.nsIInterfaceRequestor)
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

  constructor(xulNode) {
    this.node = xulNode;

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

    this._panel.addEventListener("popupshowing", this);
    this._panel.addEventListener("popuphidden", this);
    if (this.panelViews) {
      let cs = window.getComputedStyle(document.documentElement);
      // Set CSS-determined attributes now to prevent a layout flush when we do
      // it when transitioning between panels.
      this._dir = cs.direction;
      this.setMainView(this.panelViews.currentView);
      this.showMainView();
    } else {
      this._panel.addEventListener("popupshown", this);
      this._clickCapturer.addEventListener("click", this);
      this._subViews.addEventListener("overflow", this);
      this._mainViewContainer.addEventListener("overflow", this);

      // Get a MutationObserver ready to react to subview size changes. We
      // only attach this MutationObserver when a subview is being displayed.
      this._subViewObserver = new window.MutationObserver(this._syncContainerWithSubView.bind(this));
      this._mainViewObserver = new window.MutationObserver(this._syncContainerWithMainView.bind(this));

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
    ["goBack", "setHeightToFit", "setMainView", "showMainView", "showSubView"].forEach(method => {
      Object.defineProperty(this.node, method, {
        enumerable: true,
        value: (...args) => this[method](...args)
      });
    });
  }

  destructor() {
    if (this._mainView) {
      this._mainView.removeAttribute("mainview");
    }
    if (this.panelViews) {
      this.panelViews.clear();
    } else {
      this._mainViewObserver.disconnect();
      this._subViewObserver.disconnect();
      this._subViews.removeEventListener("overflow", this);
      this._mainViewContainer.removeEventListener("overflow", this);
      this._clickCapturer.removeEventListener("click", this);
    }
    this._panel.removeEventListener("popupshowing", this);
    this._panel.removeEventListener("popupshown", this);
    this._panel.removeEventListener("popuphidden", this);
    this.node = this._clickCapturer = this._viewContainer = this._mainViewContainer =
      this._subViews = this._viewStack = null;
  }

  goBack(target) {
    let [current, previous] = this.panelViews.back();
    return this.showSubView(current, target, previous);
  }

  setMainView(aNewMainView) {
    if (this.panelViews) {
      // If the new main view is not yet in the zeroth position, make sure it's
      // inserted there.
      if (aNewMainView.parentNode != this._viewStack && this._viewStack.firstChild != aNewMainView) {
        this._viewStack.insertBefore(aNewMainView, this._viewStack.firstChild);
      }
    } else {
      if (this._mainView) {
        this._mainViewObserver.disconnect();
        this._subViews.appendChild(this._mainView);
        this._mainView.removeAttribute("mainview");
      }
      this._mainViewId = aNewMainView.id;
      aNewMainView.setAttribute("mainview", "true");
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

        viewNode.removeAttribute("current");
        this._currentSubView = null;

        this._subViewObserver.disconnect();

        this._setViewContainerHeight(this._mainViewHeight);

        this.node.setAttribute("viewtype", "main");
      }

      this._shiftMainView();
    }
  }

  showSubView(aViewId, aAnchor, aPreviousView) {
    const {document, window} = this;
    return window.Task.spawn(function*() {
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
      if (playTransition) {
        dwu = this._dwu;
        previousRect = previousViewNode.__lastKnownBoundingRect =
          dwu.getBoundsWithoutFlushing(previousViewNode);
      }
      viewNode.setAttribute("current", true);

      // Emit the ViewShowing event so that the widget definition has a chance
      // to lazily populate the subview with things.
      let detail = {
        blockers: new Set(),
        addBlocker(aPromise) {
          this.blockers.add(aPromise);
        },
      };

      let evt = new window.CustomEvent("ViewShowing", { bubbles: true, cancelable: true, detail });
      viewNode.dispatchEvent(evt);

      let cancel = evt.defaultPrevented;
      if (detail.blockers.size) {
        try {
          let results = yield window.Promise.all(detail.blockers);
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
      this.node.setAttribute("viewtype", "subview");

      if (this.panelViews && playTransition) {
        // Sliding the next subview in means that the previous panelview stays
        // where it is and the active panelview slides in from the left in LTR
        // mode, right in RTL mode.
        let onTransitionEnd = () => {
          evt = new window.CustomEvent("ViewHiding", { bubbles: true, cancelable: true });
          previousViewNode.dispatchEvent(evt);
          previousViewNode.removeAttribute("current");
        };

        // There's absolutely no need to show off our epic animation skillz when
        // the panel's not even open.
        if (this._panel.state != "open") {
          onTransitionEnd();
          return;
        }

        if (aAnchor)
          aAnchor.setAttribute("open", true);
        this._viewContainer.style.height = previousRect.height + "px";
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
              }, 0);
            }
          }

          // Set the viewContainer dimensions to make sure only the current view
          // is visible.
          this._viewContainer.style.height = viewRect.height + "px";
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
          let seen = 0;
          this._viewContainer.addEventListener("transitionend", listener = ev => {
            if (ev.target == this._viewContainer && ev.propertyName == "height") {
              // Myeah, panel layout auto-resizing is a funky thing. We'll wait
              // another few milliseconds to remove the width and height 'fixtures',
              // to be sure we don't flicker annoyingly.
              // NB: HACK! Bug 1363756 is there to fix this.
              window.setTimeout(() => {
                this._viewContainer.style.removeProperty("height");
                this._viewContainer.style.removeProperty("width");
              }, 500);
              ++seen;
            } else if (ev.target == nodeToAnimate && ev.propertyName == "transform") {
              onTransitionEnd();
              this._transitioning = false;

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
              }, { once: true });
              ++seen;
            }
            if (seen == 2)
              this._viewContainer.removeEventListener("transitionend", listener);
          });
        }, { once: true });
      } else if (!this.panelViews) {
        this._shiftMainView(aAnchor);

        this._mainViewHeight = this._viewStack.clientHeight;

        let newHeight = this._heightOfSubview(viewNode, this._subViews);
        this._setViewContainerHeight(newHeight);

        this._subViewObserver.observe(viewNode, {
          attributes: true,
          characterData: true,
          childList: true,
          subtree: true
        });
      }
    }.bind(this));
  }

  _setViewContainerHeight(aHeight) {
    let container = this._viewContainer;
    this._transitioning = true;

    let onTransitionEnd = () => {
      container.removeEventListener("transitionend", onTransitionEnd);
      this._transitioning = false;
    };

    container.addEventListener("transitionend", onTransitionEnd);
    container.style.height = `${aHeight}px`;
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
      case "overflow":
        if (!this.panelViews && aEvent.target.localName == "vbox") {
          // Resize the right view on the next tick.
          if (this.showingSubView) {
            this.window.setTimeout(this._syncContainerWithSubView.bind(this), 0);
          } else if (!this.transitioning) {
            this.window.setTimeout(this._syncContainerWithMainView.bind(this), 0);
          }
        }
        break;
      case "popupshowing":
        this.node.setAttribute("panelopen", "true");
        // Bug 941196 - The panel can get taller when opening a subview. Disabling
        // autoPositioning means that the panel won't jump around if an opened
        // subview causes the panel to exceed the dimensions of the screen in the
        // direction that the panel originally opened in. This property resets
        // every time the popup closes, which is why we have to set it each time.
        this._panel.autoPosition = false;

        if (!this.panelViews) {
          this._syncContainerWithMainView();
          this._mainViewObserver.observe(this._mainView, {
            attributes: true,
            characterData: true,
            childList: true,
            subtree: true
          });
        }
        break;
      case "popupshown":
        this._setMaxHeight();
        break;
      case "popuphidden":
        this.node.removeAttribute("panelopen");
        this._mainView.style.removeProperty("height");
        this.showMainView();
        if (!this.panelViews)
          this._mainViewObserver.disconnect();
        break;
    }
  }

  _shouldSetPosition() {
    return this.node.getAttribute("nosubviews") == "true";
  }

  _shouldSetHeight() {
    return this.node.getAttribute("nosubviews") != "true";
  }

  _setMaxHeight() {
    if (!this._shouldSetHeight())
      return;

    // Ignore the mutation that'll fire when we set the height of
    // the main view.
    this.ignoreMutations = true;
    this._mainView.style.height = this.node.getBoundingClientRect().height + "px";
    this.ignoreMutations = false;
  }

  _adjustContainerHeight() {
    if (!this.ignoreMutations && !this.showingSubView && !this._transitioning) {
      let height;
      if (this.showingSubViewAsMainView) {
        height = this._heightOfSubview(this._mainView);
      } else {
        height = this._mainView.scrollHeight;
      }
      this._viewContainer.style.height = height + "px";
    }
  }

  _syncContainerWithSubView() {
    // Check that this panel is still alive:
    if (!this._panel || !this._panel.parentNode) {
      return;
    }

    if (!this.ignoreMutations && this.showingSubView) {
      let newHeight = this._heightOfSubview(this._currentSubView, this._subViews);
      this._viewContainer.style.height = newHeight + "px";
    }
  }

  _syncContainerWithMainView() {
    // Check that this panel is still alive:
    if (!this._panel || !this._panel.parentNode) {
      return;
    }

    if (this._shouldSetPosition()) {
      this._panel.adjustArrowPosition();
    }

    if (this._shouldSetHeight()) {
      this._adjustContainerHeight();
    }
  }

  /**
   * Call this when the height of one of your views (the main view or a
   * subview) changes and you want the heights of the multiview and panel
   * to be the same as the view's height.
   * If the caller can give a hint of the expected height change with the
   * optional aExpectedChange parameter, it prevents flicker.
   */
  setHeightToFit(aExpectedChange) {
    // Set the max-height to zero, wait until the height is actually
    // updated, and then remove it.  If it's not removed, weird things can
    // happen, like widgets in the panel won't respond to clicks even
    // though they're visible.
    const {window} = this;
    let count = 5;
    let height = window.getComputedStyle(this.node).height;
    if (aExpectedChange)
      this.node.style.maxHeight = (parseInt(height, 10) + aExpectedChange) + "px";
    else
      this.node.style.maxHeight = "0";
    let interval = window.setInterval(() => {
      if (height != window.getComputedStyle(this.node).height || --count == 0) {
        window.clearInterval(interval);
        this.node.style.removeProperty("max-height");
      }
    }, 0);
  }

  _heightOfSubview(aSubview, aContainerToCheck) {
    function getFullHeight(element) {
      // XXXgijs: unfortunately, scrollHeight rounds values, and there's no alternative
      // that works with overflow: auto elements. Fortunately for us,
      // we have exactly 1 (potentially) scrolling element in here (the subview body),
      // and rounding 1 value is OK - rounding more than 1 and adding them means we get
      // off-by-1 errors. Now we might be off by a subpixel, but we care less about that.
      // So, use scrollHeight *only* if the element is vertically scrollable.
      let height;
      let elementCS;
      if (element.scrollTopMax) {
        height = element.scrollHeight;
        // Bounding client rects include borders, scrollHeight doesn't:
        elementCS = win.getComputedStyle(element);
        height += parseFloat(elementCS.borderTopWidth) +
                  parseFloat(elementCS.borderBottomWidth);
      } else {
        height = element.getBoundingClientRect().height;
        if (height > 0) {
          elementCS = win.getComputedStyle(element);
        }
      }
      if (elementCS) {
        // Include margins - but not borders or paddings because they
        // were dealt with above.
        height += parseFloat(elementCS.marginTop) + parseFloat(elementCS.marginBottom);
      }
      return height;
    }
    let win = aSubview.ownerGlobal;
    let body = aSubview.querySelector(".panel-subview-body");
    let height = getFullHeight(body || aSubview);
    if (body) {
      let header = aSubview.querySelector(".panel-subview-header");
      let footer = aSubview.querySelector(".panel-subview-footer");
      height += (header ? getFullHeight(header) : 0) +
                (footer ? getFullHeight(footer) : 0);
    }
    if (aContainerToCheck) {
      let containerCS = win.getComputedStyle(aContainerToCheck);
      height += parseFloat(containerCS.paddingTop) + parseFloat(containerCS.paddingBottom);
    }
    return Math.ceil(height);
  }
}
